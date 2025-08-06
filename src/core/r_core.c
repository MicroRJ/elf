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

#include "elf_internal.h"

#include "system.h"

#include "subsystem.h"

#include "logging.c"

#include "bytecode_metadata.h"
#include "r_auxilary.h"
#include "o_string.c"
#include "k_table.c"
#include "o_closure.c"
#include "r_diagnostics.c"
#include "r_collector.c"
#include "internal.c"
#include "public.c"

#include "elf_compiler.h"

#include "core_io.c"

#include "lib_math.c"
#include "lib_core.c"
#include "lib_sys.c"
#include "lib_vec.c"
#include "lib_table.c"
#include "lib_string.c"
#include "lib_random.c"

//
// todo: remove!
//

static int _resume(elf_State *S);


static inline void _debug_stack_push(elf_State *inter, elf_Value v) {
	ASSERT(inter->stack_ptr - inter->stack < inter->stack_max);
	* inter->stack_ptr ++ = v;
}


int elf_get_global_slot(elf_State *S, elf_String *name) {

	if (name != 0) {
		return elf_table_get_index_always_(S->globals, VALUE_STRING(name));
	}

	return ARRAY_GROW(S->globals->array, 1);
}

int elf_set_global(elf_State *S, elf_String *name, elf_Value value) {

	int id = elf_get_global_slot(S, name);
	S->globals->array[id] = value;

	return id;
}

// todo: remove, this is retarded, if the arguments are already
// on the stack, we're screwed!
elf_rawapi
int elf_raw_exec(elf_State *inter, int nargs, int nrets, bool asexpr, elf_String *name, elf_String *contents) {
	ASSERT(name);
	ASSERT(contents);

	ASSERT(nargs == 0);

	elf_Proto proto = elf_compile(inter, name, contents, asexpr);

	elf_Closure *closure = elf_alloc_closure(inter, proto);

	// push closure and 'this' from the current frame
	vSetClosure(&inter->stack_ptr[0], closure);
	inter->stack_ptr[1] = inter->frame.framebase[0];
	inter->stack_ptr += 2;

	int ntruerets = elf_call(inter, nargs + 1, nrets);
	return ntruerets;
}


/* expects a table on the stack
todo: also, this functions is weird? */
static void install(elf_State *S, char *prefix, const elf_Binding *lib, int num) {
	for(int i = 0; i < num; i ++) {

		char *name = lib[i].name;

		// todo: ensure the symbol name is valid?
		if (prefix) name = elf_tpf("%s.%s",prefix,name);

		elf_push_string(S, name);
		elf_push_function(S, lib[i].function);

		elf_table_set(S);
	}
}


//
// so here we rely on the virtual memory system for things
// to work properly, otherwise, we're screwed...
// todo: proper allocations
//
elf_rawapi
void elf_init_raw(elf_State *inter) {
	inter->gc.open_object_slots = sys_virtual_alloc(GIGABYTES(1));
	inter->gc.close_object_slots = sys_virtual_alloc(GIGABYTES(1));

#if 0
	inter->exec_trail_capacity = 128;
	inter->exec_trail = sys_virtual_alloc(inter->exec_trail_capacity * sizeof(*inter->exec_trail));
#endif

	inter->frame_stack = sys_virtual_alloc(GIGABYTES(1));
	inter->frame_stack_max = GIGABYTES(1) / sizeof(*inter->frame_stack);

	inter->stack = sys_virtual_alloc(GIGABYTES(1));
	inter->stack_max = GIGABYTES(1) / sizeof(elf_Value);
	inter->stack_ptr = inter->stack;

	inter->frame.framebase = inter->stack;

#if 1
	inter->trace_table = elf_new_table(inter);
#endif


	// todo: metatables should be per module?
	inter->metatables.string = elf_new_table(inter);
	install(inter, 0, string_metafuncs, COUNTOF(string_metafuncs));

	inter->metatables.table = elf_new_table(inter);
	install(inter, 0, table_metafuncs, COUNTOF(table_metafuncs));

	// inter->strings = elf_new_table(inter);

	// todo: builtin instrinsics
	static elf_Binding lib_base[] = {
		{"ntoi", l_core_ntoi},
		{"iton", l_core_iton},
	};

	inter->globals = elf_new_table(inter);
	install(inter,     0,  lib_base  , COUNTOF(lib_base));
	install(inter,     0,  lib_math  , COUNTOF(lib_math));
	install(inter,"elf" ,  lib_core  , COUNTOF(lib_core));
	install(inter,"elf" ,  lib_random, COUNTOF(lib_random));
	install(inter,"elf" ,  l_sys,      COUNTOF(l_sys));
}

static inline void _push_stack_frame(elf_State *S, elf_Stack_Frame frame){
	ASSERT(S->frame_index < S->frame_stack_max);
	S->frame_stack[S->frame_index ++] = S->frame;
	S->frame = frame;
}

static inline void _pop_stack_frame(elf_State *S){
	ASSERT(S->frame_index > 0);
	S->frame = S->frame_stack[-- S->frame_index];
}

elf_rawapi
inline int elf_call_closure(elf_State *inter, elf_Closure *closure, int nargs, int nrets)
{
	elf_Proto proto = closure->proto;

	int framesize = nargs < proto.stacksize ? proto.stacksize : nargs;
	elf_Value *framebase = inter->stack_ptr - nargs;

	elf_Stack_Frame frame = {};
	frame.nargs = nargs;
	frame.nrets = nrets;
	frame.bytes = proto.bytes;
	frame.bytec = proto.numbytes;
	frame.framesize = framesize;
	frame.framebase = framebase;
	frame.closureenv = closure->captures;
	frame.closuresize = proto.ncaptures;

	_push_stack_frame(inter, frame);

	int unset = framesize - nargs;
	clear_memory(framebase + nargs, unset * sizeof(elf_Value));

	inter->stack_ptr = framebase + framesize;

	nrets = _resume(inter);

	_pop_stack_frame(inter);

	// restore stack pointer
	inter->stack_ptr = framebase - 1 + nrets;
	return nrets;
}

elf_rawapi
int inline elf_call_procedure(elf_State *inter, elf_Function proc, int nargs, int nrets)
{
	elf_Value *framebase = inter->stack_ptr - nargs;
	int framesize = nargs;

	elf_Stack_Frame frame = {};
	frame.framebase = framebase;
	frame.framesize = framesize;
	frame.nargs = nargs;
	frame.nrets = nrets;

	_push_stack_frame(inter, frame);

	inter->stack_ptr = framebase + framesize;

	int nuserrets = proc(inter);

	int nretpushed = inter->stack_ptr - framebase - framesize;

	// Ensure that the number of returns is consistent with the stack state.
	// The user could have used the stack for other things, so we only enforce
	// that the stack pointer incremented by the at least the number of
	// the returns the user said
	if (nuserrets < 0 || nretpushed < nuserrets)
	{
		elf_error(inter, NO_BYTE
		, elf_tpf("invalid number of returns from procedure: %i, however the procedure only pushed %i"
		, 				nuserrets, nretpushed));
	}

	// clear return space (todo: only clear the part that's doesn't overlap with the user rets)
	memset(framebase - 1, 0, nrets * sizeof(elf_Value));

	// the number of returns cannot exceed the return space
	int ntruerets = MIN(nuserrets, nrets);

	// copy results to return space
	memcpy(framebase - 1, inter->stack_ptr - nuserrets, ntruerets * sizeof(elf_Value));

	_pop_stack_frame(inter);

	inter->stack_ptr = framebase - 1 + ntruerets;
	return ntruerets;
}


elf_pubapi
int elf_call(elf_State *inter, int nargs, int nrets)
{
	elf_Value value = inter->stack_ptr[- nargs - 1];

	if (value.tag == elf_tag_Closure) {
		nrets = elf_call_closure(inter, value.x_closure, nargs, nrets);
	}
	else if(value.tag == elf_tag_Function) {
		nrets = elf_call_procedure(inter, value.x_proc, nargs, nrets);
	}
	else {
		elf_error(inter, NO_BYTE, elf_tpf("cannot call '%s'", tag2s[value.tag]));
	}
	return nrets;
}

static int tagcheck(elf_State *R, Instr id, int stk, elf_Tag x, elf_Tag y) {
	if (x != y) {
		elf_error(R,id,elf_tpf("$%i, expected %s, instead got %s",stk,tag2s[x],tag2s[y]));
	}
	return x == y;
}

// todo: just take the string instead?
static elf_Value findmetafunc(elf_State *inter, elf_Table *metatable, char const *name) {

	elf_String *sname;
	elf_IndexInt slot;

	// todo: don't do this here!
	sname = elf_new_string(inter, name);
	slot = elf_table_try_(metatable, VALUE_STRING(sname));
	inter->stack_ptr --;
	if (slot < 0 || IS_DEAD_TAG(metatable->entries[slot].key.tag)) {
		elf_error(inter, NO_BYTE, "no such meta field");
		// todo: as a courtesy, look for similar strings if any and
		// ask them if he meant ...
	}


	elf_Value value = metatable->array[metatable->entries[slot].idx];
	if ((value.tag != elf_tag_Closure) && (value.tag != elf_tag_Function)) {
		elf_error(inter, NO_BYTE, "cannot call meta field, because it is not a function");
		// todo: as a courtesy, look for similar strings if any and
		// ask them if he meant ...
	}

	return value;
}

// static inline void premetacall(elf_State *inter, elf_Object *obj, char const *name) {
	//	if (!obj->meta) {
	//		elf_error(inter, NO_BYTE, "object does not have a metatable");
	//	}
	//	findmetafunc(inter, obj->meta, name);
	//	* inter->stack_ptr = VALUE_OBJECT(obj);
	//	incstackptr(inter);
// }


static int _callov(elf_State  *inter
,                  elf_Object *obj
,                  char const *name
,                  int         reg
,                  int         nargs
,                  elf_Value  *args)
{
	__debugbreak();
	return 0;
	// elf_Value *top = GET_TOP(inter);
	// premetacall(inter, obj, name);

	// copy_memory(GET_TOP(inter),args,sizeof(elf_Value)*nargs);

	// GET_TOP(inter) += nargs;
	// int num_rets = elf_call(inter,nargs+1,1);
	// inter->frame.framebase[reg] = *top;
	// // GET_LOCAL(inter,reg)=*top;
	// return num_rets;
}


#define __fmod(a,b) ((a) -  ((elf_Int) ((a) / (b))) * (b) )
#define __mod(a,b) ((a) % (b))
#define __lteq(a,b) ((a) <= (b))
#define __lt(a,b)   ((a) <  (b))
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




#define __INSERT_NO_CHECK__(A, B, C) 0


#define divisionsafetycheck(S,  A,  B) \
do {\
	if (((B).tag == elf_tag_Num) && ((B).x_num == 0)) elf_error(S, NO_BYTE, "division by zero"); else\
	if (((B).tag == elf_tag_Int) && ((B).x_int == 0)) elf_error(S, NO_BYTE, "integer division by zero");\
} while (0)

#define __INSERT_OBJECT_OVERLOAD_ARITHMETIC__(OP, FN, FN1) \
do {\
	if (!tisobject(xx.tag)) {\
		elf_error(R, NO_BYTE, "invalid ordering, object type must come first");\
	}\
	_callov(R, xx.x_obj, tisobject(yy.tag) ? FN : FN1, BC_ARGX(byte), 1, &yy);\
} while(0)


#define BINOP_TEMPLATE(OP, FN, FN1, PREOPCODE) \
do {\
	xx = frame.framebase[ BC_ARGY(byte) ];\
	yy = frame.framebase[ BC_ARGZ(byte) ];\
	if (isnumeric(xx) && isnumeric(yy)) {\
		PREOPCODE(R, xx, yy);\
		if((xx.tag == elf_tag_Num) || (yy.tag == elf_tag_Num)) {\
			frame.framebase[BC_ARGX(byte)].tag   = elf_tag_Num;\
			frame.framebase[BC_ARGX(byte)].x_num = OP(vitonum(xx), vitonum(yy));\
		} else {\
			frame.framebase[BC_ARGX(byte)].tag   = elf_tag_Int;\
			frame.framebase[BC_ARGX(byte)].x_int = OP(vntoint(xx), vntoint(yy));\
		}\
	} else if (tisobject(xx.tag) || tisobject(yy.tag)) {\
		__INSERT_OBJECT_OVERLOAD_ARITHMETIC__(OP, FN, FN1);\
	} else {\
		INVALID_OPERANDS(xx.tag, yy.tag, OP);\
	}\
} while(0)



#define VMCASE(OPCODE) case OPCODE:
#define VMBREAK break


#define push(v) (*inter->stack_ptr ++ = v)

#define lget(i) (frame.framebase[i])

#define lset(i, v) (frame.framebase[i]=v)
#define lsetint(i, x) (vsetint(&frame.framebase[i], x))
#define lsetnum(i, x) (vsetnum(&frame.framebase[i], x))

#define lsetx(v) lset(by.b_x, v)
#define lsetxint(v) lsetint(by.b_x, v)
#define lsetxnum(v) lsetnum(by.b_x, v)


#define lgetx(v) lget(by.b_x)
#define lgety(v) lget(by.b_y)
#define lgetz(v) lget(by.b_z)


static void error_expectednumericrightoperand(elf_State *inter, char *name, elf_Value x, elf_Value y)
{
	elf_errorf(inter, -1, "'%s': invalid right-operand, for operator %s", tag2s[x.tag], name);
}

static void error_invalidoperandsforoperator(elf_State *inter, char *name, elf_Value x, elf_Value y)
{
	elf_errorf(inter, -1, "'%s': invalid operands for operator %s", tag2s[x.tag], name);
}


/* =====================================================
	Main Interpreter Loop
======================================================== */
int _resume(elf_State *inter) {
	int numrets = -1;

	elf_Table *globals = inter->globals;
	elf_Stack_Frame frame = inter->frame;

	elf_State *R = inter;

	elf_Value xx,yy,zz;

	int next_instr = 0;

	while (next_instr < frame.bytec) {
		int instr = next_instr ++;
		int minstr = frame.bytes + instr;
		elf_Bytec byte = R->bytes[minstr];
		elf_Bytec by = byte;

		// todo: in case we crash, preferably do this right
		// before or pass it in to the function?
		R->byte = minstr;

		ASSERT(inter->stack_ptr == frame.framebase + frame.framesize);

		switch (BC_OP(byte)) {

			case BC_CALL:
			{
				// put stack pointer right above all the arguments
				inter->stack_ptr = frame.framebase + by.b_x + by.b_y + 1;

				elf_call(R, by.b_y, by.b_z);

				// restore stack
				inter->stack_ptr = frame.framebase + frame.framesize;
			} break;

			case BC_RET:
			{
				numrets = MIN(BC_ARGY(byte), frame.nrets);

				for(int i = 0; i < numrets; i ++) {
					frame.framebase[i - 1] = frame.framebase[i + BC_ARGX(byte)];
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
				if (frame.framebase[BC_ARGY(byte)].x_i64 == 0) {
					int x = BC_ARGX(byte);
					next_instr = instr + x;
				}
			} break;
			case BC_JNZ: {
				if (frame.framebase[BC_ARGY(byte)].x_i64 != 0) {
					int x = BC_ARGX(byte);
					next_instr = instr + x;
				}
			} break;
			case BC_RELOAD: {
				frame.framebase[BC_ARGX(byte)] = frame.framebase[BC_ARGY(byte)];
			} break;
			/* todo: prob cache 'globals->array' as globals
			instead... */
			case BC_GETGLOBAL: {
				frame.framebase[BC_ARGX(byte)]=globals->array[BC_ARGY(byte)];
			} break;
			case BC_SETGLOBAL: {
				globals->array[BC_ARGX(byte)]=frame.framebase[BC_ARGY(byte)];
			} break;
			case BC_LOADNIL: {
				frame.framebase[BC_ARGX(byte)].tag    = elf_tag_Nil;
				frame.framebase[BC_ARGX(byte)].x_int  = 0;
			} break;
			case BC_GETKINT: {
				frame.framebase[BC_ARGX(byte)].tag   = elf_tag_Int;
				frame.framebase[BC_ARGX(byte)].x_int = R->integers[BC_ARGY(byte)];
			} break;
			case BC_GETKNUM: {
				frame.framebase[BC_ARGX(byte)].tag   = elf_tag_Num;
				frame.framebase[BC_ARGX(byte)].x_num = R->numbers[BC_ARGY(byte)];
			} break;
			case BC_GETUPVAL: {
				ASSERT(WITHIN(BC_ARGY(byte), 0, frame.closuresize));
				frame.framebase[BC_ARGX(byte)] = frame.closureenv[BC_ARGY(byte)];
			} break;
			case BC_CLOSURE: {
				ASSERT(WITHIN(BC_ARGY(byte),0,ARRAY_LENGTH(R->protos)));
				elf_Proto proto = R->protos[BC_ARGY(byte)];
				elf_Closure *new_closure = elf_alloc_closure(R,proto);
				copy_memory(new_closure->captures,frame.framebase+BC_ARGX(byte),proto.ncaptures*sizeof(elf_Value));
				frame.framebase[BC_ARGX(byte)].tag   = elf_tag_Closure;
				frame.framebase[BC_ARGX(byte)].x_closure = new_closure;
			} break;
			case BC_TABLE: {
				elf_Table *tab = elf_alloc_table(R);
				frame.framebase[BC_ARGX(byte)].tag   = elf_tag_Table;
				frame.framebase[BC_ARGX(byte)].x_tab = tab;
			} break;
			case BC_TYPEGUARD: {
				tagcheck(R,minstr,BC_ARGX(byte),BC_ARGY(byte),frame.framebase[BC_ARGX(byte)].tag);
			} break;
			case BC_GETMETAFIELD: {
				elf_Value yy = frame.framebase[BC_ARGY(byte)];
				elf_Table *metatable = {0};
				switch (yy.tag) {
					case elf_tag_String:
					case elf_tag_Table:
					case elf_tag_UserObject:
					case elf_tag_Closure: {
						metatable = yy.x_obj->meta;
					} goto _lookup;
					case elf_tag_Num: {
						metatable = R->metatables.number;
						elf_error(R,minstr,"this feature is not implemented yet, metatables for numeric types");
					} goto _lookup;
					case elf_tag_Int: {
						metatable = R->metatables.integer;
						elf_error(R,minstr,"this feature is not implemented yet, metatables for numeric types");
					} goto _lookup;
					default: {
						elf_error(R,minstr,elf_tpf("'%s': not an object", tag2s[yy.tag]));
					} break;
				}
				_lookup:
				if (!metatable) {
					elf_error(R,minstr,elf_tpf("'%s': invalid object, no metatable", tag2s[yy.tag]));
				}
				frame.framebase[BC_ARGX(byte)] = elf_table_get_raw(metatable,frame.framebase[BC_ARGZ(byte)]);
			} break;
			// todo: INDEX should be array mode!
			case BC_GETINDEX: case BC_GETFIELD: {
				xx=lgety(), yy=lgetz();

				if (isnil(yy)) {
					elf_error(inter, minstr, "attempted to get nil field");
				}
				switch (xx.tag) {

					case elf_tag_Table: {
						lsetx(elf_table_get_raw(vgettab(xx), yy));
					}break;

					case elf_tag_UserObject: {
						// _callov(R,xx.x_obj,"__getfield",BC_ARGX(byte),1,&yy);
						__debugbreak();
					}break;

					case elf_tag_String: {

						if (isint(yy)) {
							int index = vgetint(yy);
							lsetxint(vgetstr(xx)->text[index]);
						}
						else if (isstr(yy)) {

							int index = find_subtext(vgetstr(xx)->text, vgetstr(yy)->text);
							lsetxint(index);
						}
						else {
							elf_error(inter, minstr, "invalid value for string:__index(of int)");
						}

					}break;
					case elf_tag_Nil: {
						elf_error(inter, minstr, "attempted to get field of 'nil' value");
					} break;
					default: {
						elf_errorf(inter, minstr, "invalid object '%s' to perform this operator on", tag2s[yy.tag]);
					} break;
				}
			} break;
			// todo: INDEX should be array mode!
			case BC_SETINDEX:
			case BC_SETFIELD: {
				xx=lgetx(), yy=lgety(), zz=lgetz();

				if (istab(xx)) {
					if (isnil(yy)) elf_error(inter, minstr, "key is nil...");

					elf_raw_table_set(xx.x_tab, yy, zz);
				}
				else if (isusr(xx)) {
					elf_error(inter, minstr, "overload not implemented");
				}
				else if (isstr(xx)) {
					elf_error(inter, minstr, "strings are constant, you may not change them");
				}
				else {
					elf_errorf(inter, minstr, "attempted to set field of '%s' value", tag2s[xx.tag]);
				}
			} break;

			case BC_N2I:
			{
				lsetxint(vntoint(lgetx()));
			} break;

			case BC_I2N:
			{
				lsetxnum(vitonum(lgetx()));
			} break;

#define INVALID_OPERANDS(X,Y,OP) elf_error(R,NO_BYTE,elf_tpf("invalid types '%s' and '%s', for operator '%s'", tag2s[X],tag2s[Y],XTEXT(OP)))

			case BC_EQ: case BC_NEQ: {
				xx=frame.framebase[BC_ARGY(byte)];
				yy=frame.framebase[BC_ARGZ(byte)];
				bool eq=0;
				if (isnumeric(xx)) {
					if (!isnumeric(yy)) elf_error(inter, NO_BYTE, "invalid right-operand");
					eq = xx.x_int == yy.x_int;
				} else if ((xx.tag == elf_tag_Nil) || (yy.tag == elf_tag_Nil)) {
					eq = isnil(xx) == isnil(yy);
				} else if ((xx.tag == elf_tag_String) && (yy.tag == elf_tag_String)) {
					elf_String *x = xx.x_str, *y = yy.x_str;
					eq = (x == y) || ((x->hash == y->hash) && (x->length == y->length) && text_eq(x->text, y->text));
				} else {
					eq = (xx.tag == yy.tag) && (xx.x_int == yy.x_int);
				}
				if (BC_OP(byte)==BC_NEQ) {
					eq=!eq;
				}
				frame.framebase[BC_ARGX(byte)].tag  =elf_tag_Int;
				frame.framebase[BC_ARGX(byte)].x_int=eq;
			} break;
			case BC_POW: {
				xx=frame.framebase[BC_ARGY(byte)];
				yy=frame.framebase[BC_ARGZ(byte)];
				if (tisobject(xx.tag)||tisobject(yy.tag)) {
					NO_CODE;
				} else if ((xx.tag==elf_tag_Num)||(yy.tag==elf_tag_Num)) {
					if (!isnumeric(yy)) {
						elf_error(R,minstr,elf_tpf("'%s': incompatible with '%s'",tag2s[xx.tag],tag2s[yy.tag]));
					}
					frame.framebase[BC_ARGX(byte)].tag   = elf_tag_Num;
					frame.framebase[BC_ARGX(byte)].x_num = pow(vitonum(xx),vitonum(yy));
				} else if ((xx.tag==elf_tag_Int)||(yy.tag==elf_tag_Int)) {
					if (!isnumeric(yy)) {
						elf_error(R,minstr,elf_tpf("'%s': incompatible with '%s'",tag2s[xx.tag],tag2s[yy.tag]));
					}
					frame.framebase[BC_ARGX(byte)].tag   = elf_tag_Int;
					frame.framebase[BC_ARGX(byte)].x_int = pow(vntoint(xx),vntoint(yy));
				} else {
					elf_error(R,minstr,elf_tpf("invalid types '%s' and '%s', for operator '%s'", tag2s[xx.tag],tag2s[yy.tag],XTEXT(OP)));
				}
			} break;

			case BC_MOD: {
				xx=lgety(), yy=lgetz();


				if (isnumeric(xx)) {
					if (!isnumeric(yy)) {
						error_expectednumericrightoperand(inter, "__mod", xx, yy);
					}

					if (isnum(xx) || isnum(yy)) {
						elf_Number x=vitonum(xx), y=vitonum(yy);
						lsetxnum(x - (elf_Int)(x / y) * y);
					} else {
						lsetxint(vntoint(xx) % vntoint(yy));
					}
				}
				else if (isobject(xx)) {

					elf_error(R, minstr, "__pow operator not over-loadable");

				} else {
					error_invalidoperandsforoperator(inter, "__mod", xx, yy);
				}
			} break;



	/* todo: make this better */
	#define INTBOP(OPNAME,OP) \
			case OPNAME : {\
				xx=frame.framebase[BC_ARGY(byte)];\
				yy=frame.framebase[BC_ARGZ(byte)];\
				if((xx.tag==elf_tag_Int) && (yy.tag==elf_tag_Int)) {\
					if ((BC_OP(byte)==BC_DIV)||(BC_OP(byte)==BC_MOD)) {\
						divisionsafetycheck(R,  xx , yy);\
					}\
					frame.framebase[BC_ARGX(byte)].tag   = elf_tag_Int;\
					frame.framebase[BC_ARGX(byte)].x_int = OP(vntoint(xx),vntoint(yy));\
				} else INVALID_OPERANDS(xx.tag,yy.tag,OP); \
			} break
#if 0
float x0,y0,x1,y1;\
if ((xx.tag==elf_tag_float2)||(yy.tag==elf_tag_float2)) {\
	\
	if (xx.tag==elf_tag_Num)x0=y0=(float)xx.x_num;\
	else if(xx.tag==elf_tag_Int)x0=y0=(float)xx.x_int;\
	else x0=xx.x_f32,y0=xx.y_f32;\
	\
	if (yy.tag==elf_tag_Num)x1=y1=(float)yy.x_num;\
	else if(yy.tag==elf_tag_Int)x1=y1=(float)yy.x_int;\
	else x1=yy.x_f32,y1=yy.y_f32;\
	\
	frame.framebase[BC_ARGX(byte)].tag   = elf_tag_float2;\
	frame.framebase[BC_ARGX(byte)].x_f32 = x0 OP x1;\
	frame.framebase[BC_ARGX(byte)].y_f32 = y0 OP y1;\
} else
#endif
			INTBOP(BC_SHL,  	 __shl);
			INTBOP(BC_SHR,  	 __shr);
			INTBOP(BC_BIT_XOR, __xor);
			INTBOP(BC_BIT_AND, __and);
			INTBOP(BC_BIT_OR,   __or);


			case BC_MUL: {
				xx=lgety(),yy=lgetz();

				if (isnumeric(xx)) {
					if (!isnumeric(yy)) elf_error(inter, minstr, "invalid right operand for __mul");

					if (isnum(xx) || isnum(yy)) {
						lsetxnum(vitonum(xx) * vitonum(yy));
					}
					// otherwise both of them are integers
					else {
						lsetxint(vgetint(xx) * vgetint(yy));
					}
				}
				else if (isobject(xx)) {
				}
				else {
					elf_error(inter, minstr, elf_tpf("invalid operands for __mul, %s, %s", tag2s[xx.tag], tag2s[yy.tag]));
				}
			} break;

			case BC_DIV: {
				xx=lgety(),yy=lgetz();

				if (isnumeric(xx)) {
					if (!isnumeric(yy)) elf_error(inter, minstr, "invalid right operand for __div");

					if (isnum(xx) || isnum(yy)) {
						lsetxnum(vitonum(xx) / vitonum(yy));
					}
					// otherwise both of them are integers
					else {
						lsetxint(vgetint(xx) / vgetint(yy));
					}
				}
				else if (isobject(xx)) {
				}
				else {
					elf_error(inter, minstr, elf_tpf("invalid operands for __div, %s, %s", tag2s[xx.tag], tag2s[yy.tag]));
				}
			} break;

			case BC_ADD: {
				xx=lgety(),yy=lgetz();

				if (isnumeric(xx)) {
					if (!isnumeric(yy)) elf_error(inter, minstr, "invalid right operand for __add");

					if (isnum(xx) || isnum(yy)) {
						lsetxnum(vitonum(xx) + vitonum(yy));
					}
					// otherwise both of them are integers
					else {
						lsetxint(vgetint(xx) + vgetint(yy));
					}
				}
				else if (isobject(xx)) {
					// push the [function, left-operand ('this'), right-operand]
					*inter->stack_ptr ++ = findmetafunc(inter, vgetobj(xx)->meta, "__add");
					*inter->stack_ptr ++ = xx;
					*inter->stack_ptr ++ = yy;

					int nrets = elf_call(inter, 2, 1);

					if (nrets != 1) {
						elf_error(inter, minstr, "invalid number of returns for overload __add, expected only 1");
					}

					lsetx(inter->stack_ptr[-nrets]);

					// restore stack pointer
					inter->stack_ptr = frame.framebase + frame.framesize;
				}
				else {
					elf_error(inter, minstr, elf_tpf("invalid operands for __add, %s, %s", tag2s[xx.tag], tag2s[yy.tag]));
				}
			} break;

			case BC_SUB: {
				xx=lgety(),yy=lgetz();

				if (isnumeric(xx)) {
					if (!isnumeric(yy)) elf_error(inter, minstr, "invalid right operand for __sub");

					if (isnum(xx) || isnum(yy)) {
						lsetxnum(vitonum(xx) - vitonum(yy));
					}
					// otherwise both of them are integers
					else {
						lsetxint(vgetint(xx) - vgetint(yy));
					}
				}
				else if (isobject(xx)) {
				}
				else {
					elf_error(inter, minstr, elf_tpf("invalid operands for __sub, %s, %s", tag2s[xx.tag], tag2s[yy.tag]));
				}
			} break;


			case BC_LT: {
				xx=frame.framebase[by.b_y], yy=frame.framebase[by.b_z];
				if (isnumeric(xx)) {
					if (!isnumeric(yy)) elf_error(inter, minstr, "invalid right operand for __lt");
					if (isnum(xx) || isnum(yy)) {
						lsetint(by.b_x, vitonum(xx) < vitonum(yy));
					} else {
						lsetint(by.b_x, vgetint(xx) < vgetint(yy));
					}
				} else if (isobject(xx)) {
					elf_error(inter, minstr, "__lt not over-loadable");
				} else {
					elf_error(inter, minstr, elf_tpf("invalid operands for __lt, %s, %s", tag2s[xx.tag], tag2s[yy.tag]));
				}
			} break;
			case BC_LTEQ: {
				xx=frame.framebase[by.b_y], yy=frame.framebase[by.b_z];
				if (isnumeric(xx)) {
					if (!isnumeric(yy)) elf_error(inter, minstr, "invalid right operand for __lteq");
					if (isnum(xx) || isnum(yy)) {
						lsetint(by.b_x, vitonum(xx) <= vitonum(yy));
					} else {
						lsetint(by.b_x, vgetint(xx) <= vgetint(yy));
					}
				} else if (isobject(xx)) {
					elf_error(inter, minstr, "__lteq not over-loadable");
				} else {
					elf_error(inter, minstr, "invalid operands for __lteq");
				}
			} break;

			default: {
				elf_error(R,minstr,elf_tpf("unsupported instruction: %s", byte2s[BC_OP(byte)]));
			} break;
		}

		ASSERT(inter->stack_ptr == frame.framebase + frame.framesize);
	}

	esc:
	return numrets;
}

// if (R->flags & FLAG_BYTELOGGING || frame.logging)
		// {
		// 	fpf_byte(stdout,M,-1,minstr,byte);
		// }
		// if (R->flags & FLAG_DEBUGGER) {
		// 	elf_debugger("debugger 'FLAG_DEBUGGER'");
		// }
		#if 0
		if(!R->disable_tracing){
			if(R->flags & FLAG_TRACING){
				elf_Value trace_value = elf_table_get_raw(R->trace_table,VALUE_INTEGER(minstr));
				if(trace_value.tag!=elf_tag_Nil){
					if(minstr!=R->trace_start_instr){
						ASSERT(BC_OP(byte) != BC_J && BC_OP(byte) != BC_JZ && BC_OP(byte) != BC_JNZ);
						ASSERT(trace_value.x_i64 != R->trace_start_instr);
						ASSERT(trace_value.x_i64 != R->trace_stop_instr);
						R->trace_inner_loop_stack[R->trace_inner_loop_counter] = minstr;
						R->trace_inner_loop_counter ++;
					}else{
						ASSERT(R->active_trace_len==0);
					}
					ASSERT(minstr!=R->trace_stop_instr);
				}
				if(R->trace_inner_loop_counter==0){
					ARRAY_ADD(R->trace_buffer,byte);
					R->active_trace_len += 1;
				}
			}
			// trace start / end condition
			if((BC_OP(byte)==BC_J||BC_OP(byte)==BC_JZ||BC_OP(byte)==BC_JNZ)&&(BC_ARGX(byte)<0)){
				int trace_start_instr = minstr + BC_ARGX(byte);

				if(R->flags&FLAG_TRACING){
					if(R->trace_stop_instr!=minstr){
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
					elf_Value trace_value = elf_table_get_raw(R->trace_table,VALUE_INTEGER(trace_start_instr));
					if(trace_value.tag==elf_tag_Nil){
						if(R->track[minstr]>=64){
							R->flags |= FLAG_TRACING;
							R->trace_start_instr = trace_start_instr;
							R->trace_stop_instr  = minstr;
							R->active_trace_pos  = ARRAY_LENGTH(R->trace_buffer);
							R->active_trace_len  = 0;
							elf_raw_table_set(R->trace_table,VALUE_INTEGER(trace_start_instr),VALUE_INTEGER(R->active_trace_pos));
						}else{
							R->track[minstr]+=1;
						}
					}
				}
			}
		}
		#endif
		#if 0
		{
			elf_i32 index = R->exec_trail_index ++;
			index &= (R->exec_trail_capacity - 1);
			R->exec_trail[index] = (elf_trail_entry){
				.address = minstr,
				.bytecode = byte,
			};
		}
		#endif