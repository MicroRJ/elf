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


static int _resume(elf_State *S);


//
// todo: remove!
//
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

// save the current frame
static inline void pushstackframe(elf_State *inter) {
	ASSERT(inter->frame_index < inter->frame_stack_max);
	inter->frame_stack[inter->frame_index ++] = inter->frame;
}

// restore the previous frame
static inline void pullstackframe(elf_State *inter) {
	ASSERT(inter->frame_index > 0);
	inter->frame = inter->frame_stack[-- inter->frame_index];
}



// update the current frame for the given closure
static inline
void prepframeforclosure(elf_State *inter, elf_Closure *closure, int nargs, int nrets)
{

	elf_Proto proto = closure->proto;

	int framesize = nargs < proto.stacksize ? proto.stacksize : nargs;

	elf_Value *framebase = inter->stack_ptr - nargs;

	inter->frame.framesize = framesize;
	inter->frame.framebase = framebase;
	inter->frame.nargs = nargs;
	inter->frame.nrets = nrets;
	inter->frame.bytes = proto.bytes;
	inter->frame.bytec = proto.numbytes;
	inter->frame.closureenv = closure->captures;
	inter->frame.closuresize = proto.ncaptures;

	// prevent the collector from tripping on possibly garbage values
	int restofstack = framesize - nargs;
	clear_memory(framebase + nargs, restofstack * sizeof(elf_Value));
}



static inline
int callclosure(elf_State *inter, elf_Closure *closure, int nargs, int nrets)
{
	elf_Proto proto = closure->proto;

	prepframeforclosure(inter, closure, nargs, nrets);

	// update stack pointer to cover the entire frame
	inter->stack_ptr = inter->frame.framebase + inter->frame.framesize;

	nrets = _resume(inter);

	// put stack pointer right on top of all the returns
	inter->stack_ptr = inter->frame.framebase - 1 + nrets;
	return nrets;
}



static inline
int callfunction(elf_State *inter, elf_Function proc, int nargs, int nrets)
{
	elf_Value *framebase = inter->stack_ptr - nargs;
	int framesize = nargs;
	inter->frame.framebase = framebase;
	inter->frame.framesize = framesize;
	inter->frame.nargs = nargs;
	inter->frame.nrets = nrets;


	inter->stack_ptr = framebase + framesize;

	int args = framebase - inter->stack;

	int nuserrets = proc(inter, args, nargs, nrets);

	int nretpushed = inter->stack_ptr - framebase - framesize;

	// Ensure that the number of returns is consistent with the stack state.
	// The user could have used the stack for other things, so we only enforce
	// that the stack pointer incremented by the at least the number of
	// the returns the user said
	if (nuserrets < 0 || nretpushed < nuserrets)
	{
		elf_error(inter, NO_BYTE
		, elf_tpf("invalid number of returns from procedure: %i, however the procedure pushed: %i"
		, 				nuserrets, nretpushed));
	}

	// clear return space (todo: only clear the part that's doesn't overlap with the user rets)
	memset(framebase - 1, 0, nrets * sizeof(elf_Value));

	// the number of returns cannot exceed the return space
	int ntruerets = MIN(nuserrets, nrets);

	// copy results to return space
	memcpy(framebase - 1, inter->stack_ptr - nuserrets, ntruerets * sizeof(elf_Value));


	// put stack pointer right on top of all the returns
	inter->stack_ptr = framebase - 1 + ntruerets;
	return ntruerets;
}



elf_pubapi
int elf_call(elf_State *inter, int nargs, int nrets)
{
	pushstackframe(inter);

	elf_Value value = inter->stack_ptr[- nargs - 1];

	if (iscls(value)) {
		nrets = callclosure(inter, vgetcls(value), nargs, nrets);
	}
	else if(isfnc(value)) {
		nrets = callfunction(inter, vgetfnc(value), nargs, nrets);
	}
	else {
		elf_errorf(inter, NO_BYTE, "cannot call '%s'", tag2s[value.tag]);
	}

	pullstackframe(inter);
	return nrets;
}


static void error_expectednumericrightoperand(elf_State *inter, char *name, elf_Value x, elf_Value y)
{
	elf_errorf(inter, -1, "'%s': invalid right-operand, for operator %s", tag2s[x.tag], name);
}

static void error_invalidoperandsforoperator(elf_State *inter, char *name, elf_Value x, elf_Value y)
{
	elf_errorf(inter, -1, "'%s': invalid operands for operator %s, (%s, %s)", tag2s[x.tag], name, tag2s[x.tag], tag2s[y.tag]);
}


// the result is on the stack
static int callmetafield(elf_State *inter, elf_Object *obj, char *name, int nargs, elf_Value xx, elf_Value yy) {
	elf_push_string(inter, name);

	elf_Value vv = elf_table_get_raw(obj->meta, inter->stack_ptr[-1]);

	if (isnil(vv)) {
						// todo: as a courtesy, look for similar strings if any and
						// ask them if he meant ...
		elf_errorf(inter, NO_BYTE, "'%s': no such meta field", name);
	}

	if (!iscallable(vv)) {
		elf_errorf(inter, NO_BYTE, "'%s': is not a function, got: %s", name, tag2s[vv.tag]);
	}

	// push the [function, left-operand ('this'), right-operand]
	*inter->stack_ptr ++ = vv;
	*inter->stack_ptr ++ = xx;
	*inter->stack_ptr ++ = yy;

	int nrets = elf_call(inter, nargs, 1);

	if (nrets != 1) {
		elf_errorf(inter, -1, "'%s': invalid number of returns for overload, expected only 1", name);
	}

	return nrets;
}



static inline bool streq(elf_String *x, elf_String *y) {
	bool eq = (x == y) || ((x->hash == y->hash) && (x->length == y->length) && text_eq(x->text, y->text));
	return eq;
}

static inline bool veq(elf_State *inter, elf_Value xx, elf_Value yy) {
	bool eq = 0;

	// comparison between numeric operands
	if (isnumeric(xx) && isnumeric(yy)) {
		if (isnum(xx) || isnum(yy)) {
			eq = vitonum(xx) == vitonum(yy);
		} else {
			eq = vgetint(xx) == vgetint(yy);
		}
	}
	else if (vgettag(xx) == vgettag(yy)) {
		if (isstr(xx)) {
			eq = streq(vgetstr(xx), vgetstr(yy));
		}
		else {
			// raw compare
			eq = xx.x_int == yy.x_int;
		}
	}

	return eq;
}



/* =====================================================
	Macros For Generating Code
======================================================== */


#define VMCASE(OPCODE) case OPCODE:
#define VMBREAK break


#define moveX(v) (frame.framebase[by.b_x] = v)
#define moveXi(v) (vsetint(&frame.framebase[by.b_x], v))
#define moveXn(v) (vsetnum(&frame.framebase[by.b_x], v))

#define localx(v) frame.framebase[by.b_x]
#define localy(v) frame.framebase[by.b_y]
#define localz(v) frame.framebase[by.b_z]



#define __lteq(a,b) ((a) <= (b))
#define __lt(a,b)  ((a) <  (b))

#define __shl(a,b) ((a) << (b))
#define __shr(a,b) ((a) >> (b))
#define __and(a,b) ((a) & (b))
#define __or(a,b)  ((a) | (b))
#define __xor(a,b) ((a) ^ (b))

#define __add(a,b) ((a) + (b))
#define __sub(a,b) ((a) - (b))
#define __mul(a,b) ((a) * (b))
#define __div(a,b) ((a) / (b))
#define __pow(a,b) (pow((a),(b)))
#define __fmod(a,b) ((a) -  ((elf_Integer) ((a) / (b))) * (b) )
#define __mod(a,b)  ((a) % (b))



#define arithbranchnum2(nopfn, iopfn)             \
do {                                              \
	if (isnum(xx) || isnum(yy)) {                  \
		moveXn(nopfn(vitonum(xx), vitonum(yy)));  \
	}                                              \
	else {                                         \
		moveXi(iopfn(vgetint(xx), vgetint(yy)));  \
	}                                              \
} while (0)


#define arithbranchobj(name)                                        \
do {                                                                \
	int nrets = callmetafield(inter, vgetobj(xx), name, 2, xx, yy);  \
	\
	moveX(inter->stack_ptr[-nrets]);                                 \
	\
	inter->stack_ptr = frame.framebase + frame.framesize;            \
} while (0)


#define arithcase(name, nopfn, iopfn) \
do { \
	xx=localy(), yy=localz(); \
	\
	if (isnumeric(xx)) { \
		\
		if (!isnumeric(yy)) { \
			error_invalidoperandsforoperator(inter, name, xx, yy); \
		} \
		\
		arithbranchnum2(nopfn, iopfn); \
	} \
	else if (isobj(xx)) { \
		\
		arithbranchobj(name); \
		\
	} \
	else { \
		error_invalidoperandsforoperator(inter, name, xx, yy); \
	} \
} while (0)


#define bwcase(name, opfn) \
do { \
	xx=localy(), yy=localz(); \
	\
	if (isint(xx)) { \
		\
		if (!isint(yy)) { \
			error_invalidoperandsforoperator(inter, name, xx, yy); \
		} \
		\
		moveXi(opfn(vgetint(xx), vgetint(yy)));   \
	} \
	else if (isobj(xx)) { \
		\
		arithbranchobj(name); \
		\
	} \
	else { \
		error_invalidoperandsforoperator(inter, name, xx, yy); \
	} \
} while (0)


#define relcase(name, opfn) \
do { \
	xx=localy(), yy=localz(); \
	\
	if (isnumeric(xx)) { \
		\
		if (!isnumeric(yy)) error_invalidoperandsforoperator(inter, name, xx, yy); \
		\
		if (isnum(xx) || isnum(yy)) { \
			moveXi(opfn(vitonum(xx), vitonum(yy))); \
		} \
		else { \
			moveXi(opfn(vgetint(xx), vgetint(yy))); \
		} \
	} \
	else if (isobj(xx)) { \
		elf_errorf(inter, minstr, "'%s': not over-loadable", name); \
	} \
	else { \
		elf_errorf(inter, minstr, "'%s': invalid operands, %s, %s", name, tag2s[xx.tag], tag2s[yy.tag]); \
	} \
} while (0) \


/* =====================================================
	Main Interpreter Loop
======================================================== */
int _resume(elf_State *inter) {

	elf_State *R = inter;
	elf_Table *globals = inter->globals;
	elf_Stack_Frame frame = inter->frame;
	elf_Value xx,yy,zz;

	ASSERT(inter->stack_ptr == frame.framebase + frame.framesize);

	int subframes = 0;

	int nrets = 0;

	// todo: ensure that the caller sets this properly!
	frame.nextinstr = 0;

	while (frame.nextinstr < frame.bytec) {
		int instr = frame.nextinstr ++;

		int minstr = frame.bytes + instr;
		elf_Bytec byte = inter->bytes[minstr];
		elf_Bytec by = byte;

		inter->byte = minstr;


		switch (BC_OP(byte)) {

			case BC_CALL:
			{
				// put stack pointer right above all the arguments
				inter->stack_ptr = frame.framebase + by.b_x + by.b_y + 1;

				xx = localx();

				if (iscls(xx)) {

					// update next instr from our local version
					inter->frame.nextinstr = frame.nextinstr;

					// save call frame
					pushstackframe(inter);

					// update call frame from closure
					// todo: this also sets the stack pointer, but we can do that here
					prepframeforclosure(inter, vgetcls(xx), by.b_y, by.b_z);

					// load frame copy
					frame = inter->frame;

					inter->stack_ptr = frame.framebase + frame.framesize;

					// reset next instruction pointer
					frame.nextinstr = 0;

					// track the number of sub-frames locally
					subframes ++;
				}
				else if (isfnc(xx)) {

					// there's no need to push the stack frame because
					// we've already got a local copy of it here
					callfunction(inter, vgetfnc(xx), by.b_y, by.b_z);

					// restore call frame from our local copy
					inter->frame = frame;

					// restore stack
					inter->stack_ptr = frame.framebase + frame.framesize;
				}
				else {
					elf_error(inter, -1, "cannot call");
				}

				ASSERT(inter->stack_ptr == frame.framebase + frame.framesize);
			} VMBREAK;

			VMCASE(BC_RET) {
				inter->frame.nrets = MIN(by.b_y, inter->frame.nrets);

				// restore stack frame
				copy_memory(frame.framebase - 1, frame.framebase + by.b_x, inter->frame.nrets * sizeof(elf_Value));

				pullstackframe(inter);

				// restore stack pointer to proper state regardless of
				// whether we exit or not
				inter->stack_ptr = inter->frame.framebase + inter->frame.framesize;

				// sub-frame zero means we're the main frame, we can exit the routine
				// on return
				if (!subframes) goto esc;
				subframes --;

				// restore our local frame
				frame = inter->frame;

			} VMBREAK;

			case BC_NOP: {
			} break;

			case BC_J: {
				int x = BC_ARGX(byte);
				frame.nextinstr = instr + x;
			} break;
			case BC_JZ: {
				if (frame.framebase[BC_ARGY(byte)].x_i64 == 0) {
					int x = BC_ARGX(byte);
					frame.nextinstr = instr + x;
				}
			} break;
			case BC_JNZ: {
				if (frame.framebase[BC_ARGY(byte)].x_i64 != 0) {
					int x = BC_ARGX(byte);
					frame.nextinstr = instr + x;
				}
			} break;
			case BC_GETGLOBAL: {
				frame.framebase[BC_ARGX(byte)]=globals->array[BC_ARGY(byte)];
			} break;
			case BC_SETGLOBAL: {
				globals->array[BC_ARGX(byte)]=frame.framebase[BC_ARGY(byte)];
			} break;

			case BC_RELOAD: {
				moveX(localy());
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
				ASSERT(WITHIN(BC_ARGY(byte), 0, arrlen(inter->protos)));

				elf_Proto proto = inter->protos[BC_ARGY(byte)];

				elf_Closure *cls = elf_alloc_closure(inter, proto);
				copy_memory(cls->captures, frame.framebase+BC_ARGX(byte), proto.ncaptures * sizeof(elf_Value));

				vSetClosure(&localx(), cls);
			} break;
			case BC_TABLE: {

				elf_Table *tab = elf_alloc_table(R);
				vsettab(&localx(), tab);

			} break;

			case BC_GETMETAFIELD: {
				yy = localy();

				elf_Table *metatable = 0;

				switch (yy.tag) {
					case elf_tag_String:
					case elf_tag_Table:
					case elf_tag_UserObject:
					case elf_tag_Closure: {
						metatable = vgetobj(yy)->meta;
					} break;

					case elf_tag_Num: {
						metatable = inter->metatables.number;
					} break;

					case elf_tag_Int: {
						metatable = inter->metatables.integer;
					} break;

					default: {
						elf_errorf(inter, minstr, "'%s': not an object", tag2s[yy.tag]);
					} break;
				}

				if (!metatable) {
					elf_errorf(inter, minstr, "'%s': invalid object, no meta-table", tag2s[yy.tag]);
				}

				elf_Value metafield = elf_table_get_raw(metatable, localz());
				moveX(metafield);
			} break;
			// todo: INDEX should be array mode!
			case BC_GETINDEX: case BC_GETFIELD: {
				xx=localy(), yy=localz();

				if (isnil(yy)) {
					elf_error(inter, minstr, "attempted to get nil field");
				}
				switch (xx.tag) {

					case elf_tag_Table: {
						moveX(elf_table_get_raw(vgettab(xx), yy));
					}break;

					case elf_tag_UserObject: {
						// _callov(R,xx.x_obj,"__getfield",BC_ARGX(byte),1,&yy);
						__debugbreak();
					}break;

					case elf_tag_String: {

						if (isint(yy)) {
							int index = vgetint(yy);
							moveXi(vgetstr(xx)->text[index]);
						}
						else if (isstr(yy)) {

							int index = find_subtext(vgetstr(xx)->text, vgetstr(yy)->text);
							moveXi(index);
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
				xx=localx(), yy=localy(), zz=localz();

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
				moveXi(vntoint(localx()));
			} break;

			case BC_I2N:
			{
				moveXn(vitonum(localx()));
			} break;

			case BC_NEQ: {
				bool eq = veq(inter, localy(), localz());
				moveXi(!eq);
			} break;
			case BC_EQ: {
				bool eq = veq(inter, localy(), localz());
				moveXi(eq);
			} break;


			VMCASE(BC_SHL)      { bwcase("__shl", __shl); } VMBREAK;
			VMCASE(BC_SHR)      { bwcase("__shr", __shr); } VMBREAK;
			VMCASE(BC_BIT_XOR)  { bwcase("__xor", __xor); } VMBREAK;
			VMCASE(BC_BIT_AND)  { bwcase("__and", __and); } VMBREAK;
			VMCASE(BC_BIT_OR)   { bwcase("__or" , __or ); } VMBREAK;


			VMCASE(BC_MUL) { arithcase("__mul",  __mul, __mul ); } VMBREAK;
			VMCASE(BC_DIV) { arithcase("__div",  __div, __div ); } VMBREAK;
			VMCASE(BC_ADD) { arithcase("__add",  __add, __add ); } VMBREAK;
			VMCASE(BC_SUB) { arithcase("__sub",  __sub, __sub ); } VMBREAK;
			VMCASE(BC_MOD) { arithcase("__mod", __fmod, __mod ); } VMBREAK;
			VMCASE(BC_POW) { arithcase("__pow",  __pow, __pow ); } VMBREAK;


			VMCASE(BC_LT)   { relcase("__lt"  , __lt   ); } VMBREAK;
			VMCASE(BC_LTEQ) { relcase("__lteq", __lteq ); } VMBREAK;

			default: {
				elf_errorf(inter, minstr, "unsupported instruction: %s", byte2s[BC_OP(byte)]);
			} break;
		}

		ASSERT(inter->stack_ptr == frame.framebase + frame.framesize);
	}

	esc:
	return nrets;
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
					arradd(R->trace_buffer,byte);
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
							R->active_trace_pos  = arrlen(R->trace_buffer);
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