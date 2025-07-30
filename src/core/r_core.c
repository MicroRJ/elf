//
// See Copyright Notice In elf.h
//
#define _CRT_SECURE_NO_WARNINGS

//
// translation unit for the intepreter and the "core"
// runtime stuff
//

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#if defined(__EMSCRIPTEN__)
   #include <emscripten.h>
   #include <unistd.h>
#endif


#include "elf.h"
#include "internal_utils.h"

#include "r_core.h"

#include "system.h"

#include "subsystem.h"
#include "r_bytecode.h"
#include "r_auxilary.h"
#include "hash.c"

#include "o_string.c"
#include "o_table.c"
#include "o_closure.c"
#include "r_diagnostics.c"

#include "c_compiler.h"
#include "r_mundane.c"
#include "r_collector.c"
#include "lib_core.h"
#include "lib_math.c"
#include "lib_core.c"
#include "lib_time.c"
#include "lib_sys.c"
#include "lib_vec.c"
#include "lib_table.c"
#include "lib_string.c"
#include "lib_random.c"

//
// todo: remove!
//

static int elf_call(elf_State *S, int nargs, int nrets);
static int _resume(elf_State *S);


static void _debug_stack_push(elf_State *S, elf_Value v) {
	ASSERT(S->stack_ptr - S->stack < S->stack_max);
	* GET_TOP(S) ++ = v;
}


int elf_get_global_slot(elf_State *S, elf_String *name) {

	if (name != 0) {
		return elf_table_get_or_add(S->globals, VALUE_STRING(name));
	}

	return ARRAY_GROW(S->globals->array, 1);
}

int elf_set_global(elf_State *S, elf_String *name, elf_Value value) {

	int id = elf_get_global_slot(S, name);
	S->globals->array[id] = value;

	return id;
}

int elf_exec(elf_State *R, bool as_expr, int nargs, int nrets, elf_String *name, elf_String *contents) {
	elf_Proto proto = elf_compile(R, name, contents, as_expr);
	elf_Closure *closure = elf_new_closure(R, proto);
	elf_Value *rets = R->stack_ptr;
	elf_push_closure(R, closure);
	elf_push_this(R);
	nrets = elf_call(R, nargs + 1, nrets);
	R->stack_ptr = rets + nrets;
	return nrets;
}

elf_b32 elf_exec_file(elf_State *S, char *name, int nargs, int nrets) {
	FILE *io = fopen(name,"rb");
	fseek(io, 0, SEEK_END);
	int size = ftell(io);
	fseek(io, 0, SEEK_SET);
	elf_String *contents = elf_new_string2(S, size);
	fread(contents->text, 1, size, io);
	fclose(io);

	return elf_exec(S, false, nargs, nrets, elf_new_string(S, name), contents);
}

static elf_Table *instantiate(elf_State *S, NameFunctionPair *lib, int num) {
	elf_Table *tab = elf_new_table(S);
	for (int i = 0; i < num; i ++) {
		elf_String *name = elf_new_string(S, lib[i].name);
		elf_table_set(tab, VALUE_STRING(name), VALUE_FUNCTION(lib[i].fn));
	}
	return tab;
}

static void install(elf_State *S, char *prefix, NameFunctionPair *bindings, int num) {
	// todo: ensure the symbol name is valid
	for(int i = 0; i < num; i ++) {
		char *temp = bindings[i].name;
		if(prefix) temp = elf_tpf("%s.%s",prefix,temp);

		elf_String *name = elf_alloc_string(S,temp);
		// elf_debug_log("	lib: %s",name->text);
		elf_table_set(S->globals, VALUE_STRING(name), VALUE_FUNCTION(bindings[i].fn));
	}
}

//
// so here we rely on the virtual memory system for things
// to work properly, otherwise, we're screwed...
// todo: proper allocations
//
void elf_init(elf_State *R) {
	R->G.open_object_slots = sys_virtual_alloc(GIGABYTES(1));
	R->G.close_object_slots = sys_virtual_alloc(GIGABYTES(1));

#if 1
	R->exec_trail_capacity = 128;
	R->exec_trail = sys_virtual_alloc(R->exec_trail_capacity * sizeof(*R->exec_trail));
#endif

	R->frame_stack = sys_virtual_alloc(GIGABYTES(1));
	R->frame_stack_max = GIGABYTES(1) / sizeof(*R->frame_stack);

	R->stack = sys_virtual_alloc(GIGABYTES(1));
	R->stack_max = GIGABYTES(1) / sizeof(elf_Value);
	R->stack_ptr = R->stack;

	R->frame.locals = R->stack;

#if 1
	R->trace_table = elf_new_table(R);
#endif


	// todo: metatables should be per module?
	R->metatables.string = instantiate(R, string_metafuncs, COUNTOF(string_metafuncs));
	R->metatables.table = instantiate(R, table_metafuncs, COUNTOF(table_metafuncs));
	R->globals = elf_new_table(R);
	R->strings = elf_new_table(R);

	static NameFunctionPair lib_base[] = {
		{"ntoi", core_lib_ntoi},
		{"iton", core_lib_iton},
	};
	install(R,     0,  lib_base  , COUNTOF(lib_base));
	install(R,"elf" , _lib_time  , COUNTOF(_lib_time));
	install(R,     0,  lib_math  , COUNTOF(lib_math));
	install(R,"elf" ,  lib_core  , COUNTOF(lib_core));
	install(R,"elf" ,  lib_random, COUNTOF(lib_random));
}


/*
Note: the function is expected to be the first argument, however
the number of arguments does not include the function itself.
Note: The function's prototype tells how many locals to allocate
space for, this includes the arguments, the remaining space is
allocated and initialized to zero.
Note: if the number of arguments is greater than the number of
locals (too many arguments) then the number of locals for this
call frame is the number of arguments.
Todo: why is the function itself required to be adjacent to
the arguments? Couldn't we just call a function at any given
register.
Todo: why do we require the user to push the return values to
the stack, to then copy them?
*/

static inline void _push_stack_frame(elf_State *S, elf_Stack_Frame frame){
	ASSERT(S->frame_index < S->frame_stack_max);
	S->frame_stack[S->frame_index ++] = S->frame;
	S->frame = frame;
}

static inline void _pop_stack_frame(elf_State *S){
	ASSERT(S->frame_index > 0);
	S->frame = S->frame_stack[-- S->frame_index];
}

static int elf_call(elf_State *S, int numargs, int maxrets) {
	int numrets = -1;
	elf_Value *base_ptr = S->stack_ptr - numargs;
	elf_Value value = base_ptr[-1];

	if(value.tag==elf_tag_closure){
		elf_Closure *closure = value.x_closure;
		elf_Proto proto = closure->proto;
		int framesize = numargs;
		if (framesize < proto.stacksize) {
			framesize = proto.stacksize;
		}
		elf_Stack_Frame frame = {};
		frame.bytecounter = S->byte;
		frame.locals = base_ptr;
		frame.nargs = numargs;
		frame.nrets = maxrets;
		frame.closure = closure;
		frame.nlocals = framesize;
		_push_stack_frame(S,frame);

		int toclear = framesize - numargs;
		ASSERT(toclear >= 0);
		clear_memory(base_ptr + numargs, toclear * sizeof(elf_Value));
		S->stack_ptr = base_ptr + framesize;

		numrets = _resume(S);
		_pop_stack_frame(S);
	}else if(value.tag==elf_tag_function){
		elf_Stack_Frame frame = {};
		frame.bytecounter = S->byte;
		frame.locals = base_ptr;
		frame.nargs = numargs;
		frame.nrets = maxrets;
		frame.nlocals = numargs;
		_push_stack_frame(S,frame);
		S->stack_ptr = base_ptr + numargs;

		elf_Value *retsptr = S->stack_ptr;
		numrets = value.x_proc(S);
		if(numrets>=0){
			elf_Value *stackptr = S->stack_ptr;
			if ((stackptr - retsptr) < numrets) {
				elf_error(S,NO_BYTE,elf_tpf("number of values on stack '%i', is incoherent with specified number of return values '%i'",(int)(stackptr - retsptr),numrets));
			}
			retsptr = stackptr - numrets;
			clear_memory(base_ptr - 1, maxrets * sizeof(elf_Value));
			copy_memory(base_ptr - 1, retsptr, MIN(numrets,maxrets) * sizeof(elf_Value));
		}else{
			// elf_error(S,NO_BYTE,elf_tpf("error when calling function, return value: %i",numrets));
		}
		_pop_stack_frame(S);
	}else{
		elf_error(S,NO_BYTE,elf_tpf("cannot call '%s'", tag2s[value.tag]));
	}
	return numrets;
}

// todo: make this legit!
static inline void divcheck(elf_State *S, elf_Value *xx, elf_Value *yy) {
	if ((yy->tag == elf_tag_num) && (yy->x_num == 0)) elf_error(S,NO_BYTE,"division by zero"); else
	if ((yy->tag == elf_tag_int) && (yy->x_int == 0)) elf_error(S,NO_BYTE,"integer division by zero");
}


static int tagcheck(elf_State *R, Instr id, int stk, elf_tag_enum x, elf_tag_enum y) {
	if (x != y) {
		elf_error(R,id,elf_tpf("$%i, expected %s, instead got %s",stk,tag2s[x],tag2s[y]));
	}
	return x == y;
}

// todo: the overload should just be a string id, the heck...
static int _callov(elf_State *S, elf_Object *obj, char const *name, int reg, int nargs, elf_Value *args) {
	if (obj->meta == 0) {
		elf_error(S,NO_BYTE,"object does not have a metatable, cannot use overload");
	}
	int num_rets = 0;
	elf_Value value = elf_tgetx_any(obj->meta,name);
	if((value.tag)==elf_tag_closure||(value.tag)==elf_tag_function) {
		elf_Value *top = GET_TOP(S);
		PUSHV(S,value);
		PUSHV(S,VALUE_OBJECT(obj));
		copy_memory(GET_TOP(S),args,sizeof(elf_Value)*nargs);
		GET_TOP(S) += nargs;
		num_rets = elf_call(S,nargs+1,1);
		S->frame.locals[reg] = *top;
		// GET_LOCAL(S,reg)=*top;
	}else{
		for(int i=0;i<obj->meta->ntotal;i++){
			if(obj->meta->slots[i].key.tag==elf_tag_str){
				printf(" -> '%s'\n", obj->meta->slots[i].key.x_str->text);
			}
		}

		char *obj2s[]={"obj","cls","str","tab"};
		elf_error(S,NO_BYTE,elf_tpf("'%s': meta field of object '%s' is '%s', not a function, there are %i metafield(s) available:",name,obj2s[obj->type],tag2s[value.tag],obj->meta->ntotal));

		// todo: remove
		elf_tgetx_any(obj->meta,name);
	}
	return num_rets;
}

int _resume(elf_State *R) {

	int numrets = -1;

	// todo: wtf ???
	elf_State *M = R;

	elf_Table *globals = M->globals;

	elf_Stack_Frame F = R->frame;
	elf_Value *locals = F.locals;
	elf_Closure *closure = F.closure;
	elf_Proto proto = closure->proto;
	elf_Value *values = closure->values;

	int next_instr = 0;
	while (next_instr < proto.numbytes) {
		elf_Value xx,yy,zz;
		int instr = next_instr ++;
		int module_instr = proto.bytes + instr;
		elf_bytecode byte = M->bytes[module_instr];

		// todo: in case we crash, preferably do this right
		// before or pass it in to the function?
		R->byte = module_instr;
		ASSERT(GET_TOP(R) >= locals + proto.stacksize);

		// if (R->flags & FLAG_BYTELOGGING || F.logging)
		// {
		// 	fpf_byte(stdout,M,-1,module_instr,byte);
		// }
		// if (R->flags & FLAG_DEBUGGER) {
		// 	elf_debugger("debugger 'FLAG_DEBUGGER'");
		// }
		#if 0
		if(!R->disable_tracing){
			if(R->flags & FLAG_TRACING){
				elf_Value trace_value = elf_table_get(R->trace_table,VALUE_INTEGER(module_instr));
				if(trace_value.tag!=elf_tag_nil){
					if(module_instr!=R->trace_start_instr){
						ASSERT(BC_OP(byte) != BC_J && BC_OP(byte) != BC_JZ && BC_OP(byte) != BC_JNZ);
						ASSERT(trace_value.x_i64 != R->trace_start_instr);
						ASSERT(trace_value.x_i64 != R->trace_stop_instr);
						R->trace_inner_loop_stack[R->trace_inner_loop_counter] = module_instr;
						R->trace_inner_loop_counter ++;
					}else{
						ASSERT(R->active_trace_len==0);
					}
					ASSERT(module_instr!=R->trace_stop_instr);
				}
				if(R->trace_inner_loop_counter==0){
					ARRAY_ADD(R->trace_buffer,byte);
					R->active_trace_len += 1;
				}
			}
			// trace start / end condition
			if((BC_OP(byte)==BC_J||BC_OP(byte)==BC_JZ||BC_OP(byte)==BC_JNZ)&&(BC_ARGX(byte)<0)){
				int trace_start_instr = module_instr + BC_ARGX(byte);

				if(R->flags&FLAG_TRACING){
					if(R->trace_stop_instr!=module_instr){
						ASSERT(R->trace_inner_loop_counter>0);
						R->trace_inner_loop_counter-=1;
						ASSERT(trace_start_instr==R->trace_inner_loop_stack[R->trace_inner_loop_counter]);
					}else{
						ASSERT(R->trace_inner_loop_counter==0);
						R->flags &= ~FLAG_TRACING;
						elf_debug_log("finished recording trace, %i bytes recorded", R->active_trace_len);
					}
				}else{
					ASSERT(R->trace_inner_loop_counter == 0);
					elf_Value trace_value = elf_table_get(R->trace_table,VALUE_INTEGER(trace_start_instr));
					if(trace_value.tag==elf_tag_nil){
						if(R->track[module_instr]>=64){
							R->flags |= FLAG_TRACING;
							R->trace_start_instr = trace_start_instr;
							R->trace_stop_instr  = module_instr;
							R->active_trace_pos  = ARRAY_LENGTH(R->trace_buffer);
							R->active_trace_len  = 0;
							elf_table_set(R->trace_table,VALUE_INTEGER(trace_start_instr),VALUE_INTEGER(R->active_trace_pos));
						}else{
							R->track[module_instr]+=1;
						}
					}
				}
			}
		}
		#endif
		#if 1
		{
			elf_i32 index = R->exec_trail_index ++;
			index &= (R->exec_trail_capacity - 1);
			R->exec_trail[index] = (elf_trail_entry){
				.address = module_instr,
				.bytecode = byte,
			};
		}
		#endif
		switch(BC_OP(byte)){
			case BC_CALL:{
				R->byte = module_instr;
				int callreg = BC_ARGX(byte);
				int numargs = BC_ARGY(byte);
				int maxrets = BC_ARGZ(byte);
				xx = locals[callreg];
				// todo: why '>'
				ASSERT(R->stack_ptr >= locals + proto.stacksize);
				// todo: don't make recursive...
				_set_stack_ptr(R, locals + callreg + 1 + numargs);
				elf_call(R,numargs,maxrets);
				_set_stack_ptr(R, locals + proto.stacksize);
			}break;
			case BC_RET: {
				numrets = MIN(BC_ARGY(byte),F.nrets);
				for(int i = 0; i < numrets; i ++) {
					locals[i-1] = locals[i+BC_ARGX(byte)];
				}
				goto esc;
			} break;
			case BC_NOP: {
			} break;
			case BC_J: {
				int x = BC_ARGX(byte);
				next_instr = instr + x;
			} break;
			case BC_JZ: {
				if(locals[BC_ARGY(byte)].x_i64 == 0) {
					int x = BC_ARGX(byte);
					next_instr = instr + x;
				}
			} break;
			case BC_JNZ: {
				if(locals[BC_ARGY(byte)].x_i64 != 0) {
					int x = BC_ARGX(byte);
					next_instr = instr + x;
				}
			} break;
			case BC_RELOAD: {
				locals[BC_ARGX(byte)] = locals[BC_ARGY(byte)];
			} break;
			/* todo: prob cache 'globals->array' as globals
			instead... */
			case BC_GETGLOBAL: {
				locals[BC_ARGX(byte)]=globals->array[BC_ARGY(byte)];
			} break;
			case BC_SETGLOBAL: {
				globals->array[BC_ARGX(byte)]=locals[BC_ARGY(byte)];
			} break;
			case BC_LOADNIL: {
				locals[BC_ARGX(byte)].tag    = elf_tag_nil;
				locals[BC_ARGX(byte)].x_int  = 0;
			} break;
			case BC_GETKINT: {
				locals[BC_ARGX(byte)].tag   = elf_tag_int;
				locals[BC_ARGX(byte)].x_int = M->integers[BC_ARGY(byte)];
			} break;
			case BC_GETKNUM: {
				locals[BC_ARGX(byte)].tag   = elf_tag_num;
				locals[BC_ARGX(byte)].x_num = M->numbers[BC_ARGY(byte)];
			} break;
			case BC_GETUPVAL: {
				ASSERT(WITHIN(BC_ARGY(byte),0,proto.ncaptures));
				locals[BC_ARGX(byte)] = values[BC_ARGY(byte)];
			} break;
			case BC_CLOSURE: {
				ASSERT(WITHIN(BC_ARGY(byte),0,ARRAY_LENGTH(M->protos)));
				elf_Proto proto = M->protos[BC_ARGY(byte)];
				elf_Closure *new_closure = elf_alloc_closure(R,proto);
				copy_memory(new_closure->values,locals+BC_ARGX(byte),proto.ncaptures*sizeof(elf_Value));
				locals[BC_ARGX(byte)].tag   = elf_tag_closure;
				locals[BC_ARGX(byte)].x_closure = new_closure;
			} break;
			case BC_TABLE: {
				elf_Table *tab = elf_alloc_table(R);
				locals[BC_ARGX(byte)].tag   = elf_tag_tab;
				locals[BC_ARGX(byte)].x_tab = tab;
			} break;
			case BC_TYPEGUARD: {
				tagcheck(R,module_instr,BC_ARGX(byte),BC_ARGY(byte),locals[BC_ARGX(byte)].tag);
			} break;
			case BC_GETMETAFIELD: {
				elf_Value yy = locals[BC_ARGY(byte)];
				elf_Table *metatable = {0};
				switch (yy.tag) {
					case elf_tag_str:
					case elf_tag_tab:
					case elf_tag_userobj:
					case elf_tag_closure: {
						metatable = yy.x_obj->meta;
					} goto _lookup;
					case elf_tag_num: {
						metatable = R->metatables.number;
						elf_error(R,module_instr,"this feature is not implemented yet, metatables for numeric types");
					} goto _lookup;
					case elf_tag_int: {
						metatable = R->metatables.integer;
						elf_error(R,module_instr,"this feature is not implemented yet, metatables for numeric types");
					} goto _lookup;
					default: {
						elf_error(R,module_instr,elf_tpf("'%s': not an object", tag2s[yy.tag]));
					} break;
				}
				_lookup:
				if (!metatable) {
					elf_error(R,module_instr,elf_tpf("'%s': invalid object, no metatable", tag2s[yy.tag]));
				}
				locals[BC_ARGX(byte)] = elf_table_get(metatable,locals[BC_ARGZ(byte)]);
			} break;
			/* todo: why are these two identical bro... */
			case BC_GETINDEX: case BC_GETFIELD: {
				xx=locals[BC_ARGY(byte)];
				yy=locals[BC_ARGZ(byte)];
				// todo: my preference, maybe this could be configured...
				if(yy.tag==elf_tag_nil) {
					elf_error(R,module_instr,"attempted to get nil field");
				}else switch (xx.tag){
					case elf_tag_tab:{
						locals[BC_ARGX(byte)]=elf_table_get(xx.x_tab,yy);
					}break;
					case elf_tag_userobj:{
						_callov(R,xx.x_obj,"__getfield",BC_ARGX(byte),1,&yy);
					}break;
					case elf_tag_str:{
						/* todo: allow for indexing for substrings,
						for instrance, "my name is"["name"] */
						tagcheck(R,module_instr,0,elf_tag_int,yy.tag);
						elf_String *string = xx.x_str;
						elf_i32 index = yy.x_i32;
						locals[BC_ARGX(byte)].tag   = elf_tag_int;
						locals[BC_ARGX(byte)].x_i32 = string->text[index];
					}break;
					case elf_tag_nil:{
						elf_error(R,module_instr,"attempted to get field of 'nil' value");
					}break;
					default:{
						elf_error(R,module_instr,elf_tpf("invalid object '%s' to perform this operator on", tag2s[yy.tag]));
					}break;
				}
			} break;
			case BC_SETINDEX: case BC_SETFIELD: {
				xx=locals[BC_ARGX(byte)];
				yy=locals[BC_ARGY(byte)];
				zz=locals[BC_ARGZ(byte)];
				if(xx.tag==elf_tag_tab){
					if(!xx.x_tab) elf_error(R,module_instr,elf_tpf("table is nil, how did this happen?"));
					if(yy.tag==elf_tag_nil) elf_error(R,module_instr,elf_tpf("key is nil..."));
					elf_table_set(xx.x_tab,yy,zz);
				}else if(xx.tag==elf_tag_userobj){
					elf_Value args[] = { yy, zz };
					_callov(R,xx.x_obj,"__setfield",BC_ARGX(byte),2,args);
				}else{
					elf_error(R,module_instr,elf_tpf("attempted to set field of '%s' value",tag2s[xx.tag]));
				}
			} break;
#if 0
			case BC_LEAVE: {
				delay = F.delay_list;
				if (delay != 0) {
					next_instr    = delay->j;
					F.delay_list = delay->n;
					dealloc_memory(GLOBAL_ALLOCATOR,delay);
				} else goto esc;
			} break;
			case BC_DELAY: {
				ASSERT(BC_ARGX(byte) >= 0);

				/* todo: make this better??? */
				delay = alloc_memory(GLOBAL_ALLOCATOR,sizeof(delaylist));
				delay->n = F.delay_list;
				delay->j = next_instr;
				F.delay_list = delay;

				next_instr = instr + BC_ARGX(byte);
			} break;
			case BC_YIELD: {
				__debugbreak();
				ASSERT(BC_ARGX(byte) >= 0);
				int reg,nrets;
				nrets = MIN(BC_ARGZ(byte),F.nrets);
				for (reg=0; reg<nrets; ++reg) {
					locals[reg-1] = locals[reg+BC_ARGY(byte)];
				}
				next_instr = instr+BC_ARGX(byte);

				F.nrets = nrets;
			} break;
			case BC_LOOP: {
			} break;
			case BC_ISNIL: {
				elf_Value xx;
				bool nan;

				xx = locals[BC_ARGY(byte)];
				nan = xx.tag != elf_tag_int && xx.tag != elf_tag_num;
				locals[BC_ARGX(byte)].tag   = elf_tag_int;
				locals[BC_ARGX(byte)].x_int = xx.tag == elf_tag_nil || (nan && xx.x_int == 0);
			} break;
#endif
			case BC_N2I: {
				xx=locals[BC_ARGY(byte)];
				locals[BC_ARGX(byte)].tag   = elf_tag_int;
				locals[BC_ARGX(byte)].x_int = VN2I(xx);
			} break;
			case BC_I2N: {
				xx=locals[BC_ARGY(byte)];
				locals[BC_ARGX(byte)].tag   = elf_tag_num;
				locals[BC_ARGX(byte)].x_num = VI2N(xx);
			} break;
			case BC_EQ: case BC_NEQ: {
				xx=locals[BC_ARGY(byte)];
				yy=locals[BC_ARGZ(byte)];
				bool eq=0;
				if ((xx.tag==elf_tag_nil)||(yy.tag==elf_tag_nil)) {
					eq=IS_NIL_VALUE(xx)==IS_NIL_VALUE(yy);
				} else if ((xx.tag==elf_tag_str)&&(yy.tag==elf_tag_str)) {
					eq=elf_get_strings_eq(xx.x_str,yy.x_str);
				} else if ((IS_INT_OR_NUM(xx.tag))&&(IS_INT_OR_NUM(yy.tag))) {
					eq=xx.x_int==yy.x_int;
				} else {
					eq=(xx.tag==yy.tag)&&(xx.x_int==yy.x_int);
				}
				if (BC_OP(byte)==BC_NEQ) {
					eq=!eq;
				}
				locals[BC_ARGX(byte)].tag  =elf_tag_int;
				locals[BC_ARGX(byte)].x_int=eq;
			} break;
#if 0
			case BC_POW: {
				xx=locals[BC_ARGY(byte)];
				yy=locals[BC_ARGZ(byte)];
				if (IS_OBJ_TAG(xx.tag)||IS_OBJ_TAG(yy.tag)) {
					NO_CODE;
				} else if ((xx.tag==elf_tag_num)||(yy.tag==elf_tag_num)) {
					if (!IS_INT_OR_NUM(yy.tag)) {
						elf_error(R,module_instr,elf_tpf("'%s': incompatible with '%s'",tag2s[xx.tag],tag2s[yy.tag]));
					}
					locals[BC_ARGX(byte)].tag   = elf_tag_num;
					locals[BC_ARGX(byte)].x_num = pow(VI2N(xx),VI2N(yy));
				} else if ((xx.tag==elf_tag_int)||(yy.tag==elf_tag_int)) {
					if (!IS_INT_OR_NUM(yy.tag)) {
						elf_error(R,module_instr,elf_tpf("'%s': incompatible with '%s'",tag2s[xx.tag],tag2s[yy.tag]));
					}
					locals[BC_ARGX(byte)].tag   = elf_tag_int;
					locals[BC_ARGX(byte)].x_int = pow(VN2I(xx),VN2I(yy));
				} else {
					elf_error(R,module_instr,elf_tpf("invalid types '%s' and '%s', for operator '%s'", tag2s[xx.tag],tag2s[yy.tag],XTEXT(OP)));
				}
			} break;
#endif
			case BC_MOD: {
				elf_Num x,y;
				xx=locals[BC_ARGY(byte)];
				yy=locals[BC_ARGZ(byte)];
				if (IS_OBJ_TAG(xx.tag)||IS_OBJ_TAG(yy.tag)) {
					NO_CODE;
				} else if ((xx.tag == elf_tag_num)||(yy.tag == elf_tag_num)) {
					if (!IS_INT_OR_NUM(yy.tag)) {
						elf_error(R,module_instr,elf_tpf("'%s': incompatible with '%s'",tag2s[xx.tag],tag2s[yy.tag]));
					}
					divcheck(R,&xx,&yy);
					x=VI2N(xx);
					y=VI2N(yy);
					locals[BC_ARGX(byte)].tag   = elf_tag_num;
					locals[BC_ARGX(byte)].x_num = x - (elf_Int)(x / y) * y;
				} else if ((xx.tag==elf_tag_int)||(yy.tag==elf_tag_int)) {
					if (!IS_INT_OR_NUM(yy.tag)) {
						elf_error(R,module_instr,elf_tpf("'%s': incompatible with '%s'",tag2s[xx.tag],tag2s[yy.tag]));
					}
					divcheck(R,&xx,&yy);
					locals[BC_ARGX(byte)].tag   = elf_tag_int;
					locals[BC_ARGX(byte)].x_int = VN2I(xx) % VN2I(yy);
				} else {
					elf_error(R,module_instr,elf_tpf("invalid types '%s' and '%s', for operator '%s'", tag2s[xx.tag],tag2s[yy.tag],XTEXT(OP)));
				}
			} break;


	#define INVALID_OPERANDS(X,Y,OP) elf_error(R,NO_BYTE,elf_tpf("invalid types '%s' and '%s', for operator '%s'", tag2s[X],tag2s[Y],XTEXT(OP)))

	/* todo: make this better */
	#define INTBOP(OPNAME,OP) \
			case OPNAME : {\
				xx=locals[BC_ARGY(byte)];\
				yy=locals[BC_ARGZ(byte)];\
				if((xx.tag==elf_tag_int) && (yy.tag==elf_tag_int)) {\
					if ((BC_OP(byte)==BC_DIV)||(BC_OP(byte)==BC_MOD)) {\
						divcheck(R,&xx,&yy);\
					}\
					locals[BC_ARGX(byte)].tag   = elf_tag_int;\
					locals[BC_ARGX(byte)].x_int = OP(VN2I(xx),VN2I(yy));\
				} else INVALID_OPERANDS(xx.tag,yy.tag,OP); \
			} break
#if 0
float x0,y0,x1,y1;\
if ((xx.tag==elf_tag_float2)||(yy.tag==elf_tag_float2)) {\
	\
	if (xx.tag==elf_tag_num)x0=y0=(float)xx.x_num;\
	else if(xx.tag==elf_tag_int)x0=y0=(float)xx.x_int;\
	else x0=xx.x_f32,y0=xx.y_f32;\
	\
	if (yy.tag==elf_tag_num)x1=y1=(float)yy.x_num;\
	else if(yy.tag==elf_tag_int)x1=y1=(float)yy.x_int;\
	else x1=yy.x_f32,y1=yy.y_f32;\
	\
	locals[BC_ARGX(byte)].tag   = elf_tag_float2;\
	locals[BC_ARGX(byte)].x_f32 = x0 OP x1;\
	locals[BC_ARGX(byte)].y_f32 = y0 OP y1;\
} else
#endif
	#define NUMBOP(OPCODE,OP,FN,FN1,PREOP_CHECK) \
			case OPCODE : {\
				xx=locals[BC_ARGY(byte)];\
				yy=locals[BC_ARGZ(byte)];\
				if (IS_OBJ_TAG(xx.tag)||IS_OBJ_TAG(yy.tag)) {\
					if (!IS_OBJ_TAG(xx.tag)) elf_error(R,NO_BYTE,"invalid ordering, object type must come first, (todo: call converter function on the object, __tonumber)");\
					_callov(R,xx.x_obj,IS_OBJ_TAG(yy.tag)?FN:FN1,BC_ARGX(byte),1,&yy);\
				} else if (IS_INT_OR_NUM(xx.tag) && IS_INT_OR_NUM(yy.tag)) {\
					PREOP_CHECK;\
					if((xx.tag==elf_tag_num) || (yy.tag==elf_tag_num)) {\
						locals[BC_ARGX(byte)].tag   = elf_tag_num;\
						locals[BC_ARGX(byte)].x_num = OP(VI2N(xx),VI2N(yy));\
					}else{\
						locals[BC_ARGX(byte)].tag   = elf_tag_int;\
						locals[BC_ARGX(byte)].x_int = OP(VN2I(xx),VN2I(yy));\
					}\
				} else INVALID_OPERANDS(xx.tag,yy.tag,OP); \
			} break
			case BC_LTEQ: {
				xx=locals[BC_ARGY(byte)];
				yy=locals[BC_ARGZ(byte)];
				if(IS_INT_OR_NUM(xx.tag) && IS_INT_OR_NUM(yy.tag)){
					if ((xx.tag==elf_tag_num)||(yy.tag==elf_tag_num)) {
						locals[BC_ARGX(byte)].tag   = elf_tag_int;
						locals[BC_ARGX(byte)].x_i64 = VI2N(xx) <= VI2N(yy);
					} else {
						locals[BC_ARGX(byte)].tag   = elf_tag_int;
						locals[BC_ARGX(byte)].x_i64 = VN2I(xx) <= VN2I(yy);
					}
				} else INVALID_OPERANDS(xx.tag,yy.tag,<=);
			} break;
			case BC_LT: {
				xx=locals[BC_ARGY(byte)];
				yy=locals[BC_ARGZ(byte)];
				if(IS_INT_OR_NUM(xx.tag) && IS_INT_OR_NUM(yy.tag)){
					if ((xx.tag==elf_tag_num)||(yy.tag==elf_tag_num)) {
						locals[BC_ARGX(byte)].tag   = elf_tag_int;
						locals[BC_ARGX(byte)].x_int = VI2N(xx) < VI2N(yy);
					} else {
						locals[BC_ARGX(byte)].tag   = elf_tag_int;
						locals[BC_ARGX(byte)].x_int = VN2I(xx) < VN2I(yy);
					}
				} else INVALID_OPERANDS(xx.tag,yy.tag,<);
			} break;

#define __shl(a,b) ((a) << (b))
#define __shr(a,b) ((a) >> (b))
#define __add(a,b) ((a) + (b))
#define __sub(a,b) ((a) - (b))
#define __mul(a,b) ((a) * (b))
#define __div(a,b) ((a) / (b))
#define __and(a,b) ((a) & (b))
#define __or(a,b)  ((a) | (b))
#define __xor(a,b) ((a) ^ (b))
#define __pow(a,b) (pow((a),(b)))

			INTBOP(BC_SHL,  	 __shl);
			INTBOP(BC_SHR,  	 __shr);
			INTBOP(BC_BIT_XOR, __xor);
			INTBOP(BC_BIT_AND, __and);
			INTBOP(BC_BIT_OR,   __or);
			NUMBOP(BC_ADD, __add, "__add", "__add1", (void)0);
			NUMBOP(BC_SUB, __sub, "__sub", "__sub1", (void)0);
			NUMBOP(BC_MUL, __mul, "__mul", "__mul1", (void)0);
			NUMBOP(BC_DIV, __div, "__div", "__div1", divcheck(R,&xx,&yy));
			NUMBOP(BC_POW, __pow, "__pow", "__pow1", (void)0);
	#undef NUMBOP
			default: {
				elf_error(R,module_instr,elf_tpf("unsupported instruction: %s", byte2s[BC_OP(byte)]));
			} break;
		}
	}

	esc:
	return numrets;
}

