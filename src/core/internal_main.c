//
// See Copyright Notice In elf.h
//
#define _CRT_SECURE_NO_WARNINGS


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

#include "subsystem.h"
#include "system.h"

#include "logging.c"


#include "internal_types.h"
#include "internal_helpers.h"
#include "internal_metadata.c"
#include "internal_diagnostics.c"

#include "o_string.c"
#include "k_table.c"
#include "o_closure.c"
#include "r_collector.c"

#include "elf_compiler.h"

#include "l_math.c"
#include "l_core.c"
#include "l_sys.c"
#include "l_table.c"
#include "l_string.c"
#include "l_random.c"


static int _resume(elf_State *S);



/* expects a table on the stack
todo: also, this functions is weird? */
static void install(elf_State *S, char *prefix, const elf_Binding *lib, int num) {
	for(int i = 0; i < num; i ++) {

		char *name = lib[i].name;

		// todo: ensure the symbol name is valid?
		if (prefix) name = elf_tpf("%s.%s",prefix,name);

		elf_pushtext(S, name);
		elf_pushfun(S, lib[i].function);

		elf_setfield(S);
	}
}

//
// todo: we rely on the virtual memory system
//
elf_rawapi
void elf_init_raw(elf_State *S) {
	S->gc.open_object_slots = sys_virtual_alloc(GIGABYTES(1));
	S->gc.close_object_slots = sys_virtual_alloc(GIGABYTES(1));

	// todo: can we use the value regular stack instead
	S->frame_stack = sys_virtual_alloc(GIGABYTES(1));
	S->frame_stack_max = GIGABYTES(1) / sizeof(*S->frame_stack);

	S->stack = sys_virtual_alloc(GIGABYTES(1));
	S->stack_max = GIGABYTES(1) / sizeof(elf_Value);
	S->stack_ptr = S->stack;


	// it literally doesn't matter, but techinically, framebase-1
	// is where the function is at, we don't have one, so nil.
	pushnil(S);

	S->frame.framebase = S->stack;
	S->frame.framesize = 16;

#if 0
	S->exec_trail_capacity = 128;
	S->exec_trail = sys_virtual_alloc(S->exec_trail_capacity * sizeof(*S->exec_trail));
	S->trace_table = pushtable(S);
#endif


	// todo: metatables should be per module?
	S->metatables.string = pushtable(S);
	install(S, 0, string_metafuncs, COUNTOF(string_metafuncs));

	S->metatables.table = pushtable(S);
	install(S, 0, table_metafuncs, COUNTOF(table_metafuncs));

	// S->strings = pushtable(S);

	// todo: builtin instrinsics
	static elf_Binding lib_base[] = {
		{"ntoi", l_core_ntoi},
		{"iton", l_core_iton},
	};

	S->globals = pushtable(S);
	install(S,     0,  lib_base  , COUNTOF(lib_base));
	install(S,     0,  lib_math  , COUNTOF(lib_math));
	install(S,"elf" ,  l_core    , COUNTOF(l_core));
	install(S,"elf" ,  lib_random, COUNTOF(lib_random));
	install(S,"elf" ,  l_sys,      COUNTOF(l_sys));


	ASSERT(stack2index(S) < S->frame.framesize);

	S->stack_ptr = S->frame.framebase + S->frame.framesize;
}



// save the current frame
static inline void
pushstackframe(elf_State *inter)
{

	ASSERT(inter->frame_index < inter->frame_stack_max);
	inter->frame_stack[inter->frame_index ++] = inter->frame;

}



// restore the previous frame
static inline void
pullstackframe(elf_State *inter)
{

	ASSERT(inter->frame_index > 0);
	inter->frame = inter->frame_stack[-- inter->frame_index];

}



// update the given frame to match the given closure's needs
static inline void
prepframeforclosure(elf_State *E, Stack_Frame *frame, elf_Closure *closure, int nargs, int nrets)
{
	Proto proto = closure->proto;

	// todo: do not grow frame size less we care about vargs
	int framesize = nargs < proto.stacksize ? proto.stacksize : nargs;

	V *framebase = E->stack_ptr - nargs;

	V *reference = framebase;

	// Do we care about excess args? And do we have any?
	// todo: unlikely
	if (proto.variadic && nargs > proto.arity) {
		reference += nargs;
		framesize += proto.arity;
		copy_values(reference, framebase, proto.arity);
	}

	// prevent the collector from tripping on possibly garbage values
	// clear the remaining stack space
	zero_values(reference + MIN(proto.arity, nargs), framesize - MIN(proto.arity, nargs) - (reference - framebase));


	frame->framesize = framesize;
	frame->framebase = framebase;
	frame->reference = reference;
	frame->nargs = nargs;
	frame->nrets = nrets;
	frame->arity = proto.arity;
	frame->variadic = proto.variadic;
	frame->bytes = proto.bytes;
	frame->bytec = proto.numbytes;
	frame->closureenv = closure->captures;
	frame->closuresize = proto.ncaptures;
	frame->nextinstr = 0;

}


// is is still a call function, the only difference is that it happens
// to be the interpreter's resume and it has access to a closure but who
// doesn't these days
static inline int
callclosure(elf_State *E, elf_Closure *closure, int nargs, int nrets)
{
	prepframeforclosure(E, &E->frame, closure, nargs, nrets);

	// update stack pointer to cover the entire frame
	E->stack_ptr = E->frame.framebase + E->frame.framesize;

	nrets = _resume(E);

	// put stack pointer right on top of all the returns
	E->stack_ptr = E->frame.framebase - 1 + nrets;
	return nrets;
}



// nrets would not matter for the frame size because returns are pushed
// regardless
static inline int
callfunction(elf_State *E, elf_Function proc, int nargs, int nrets)
{
	V *framebase = E->stack_ptr - nargs;
	V *reference = framebase;

	int framesize = nargs;

	E->frame.framebase = framebase;
	E->frame.reference = reference;
	E->frame.framesize = framesize;
	E->frame.nargs = nargs;
	E->frame.nrets = nrets;

	E->stack_ptr = framebase + framesize;

	int nuserrets = proc(E, nargs, nrets);

	int nretpushed = E->stack_ptr - framebase - framesize;

	// Ensure that the number of returns is consistent with the stack state.
	// The user could have used the stack for other things, so we only enforce
	// that the stack pointer incremented by the at least the number of
	// the returns the user said
	if (nuserrets < 0 || nretpushed < nuserrets)
	{
		elf_error(E, NO_BYTE
		, elf_tpf("invalid number of returns from procedure: %i, however the procedure pushed: %i"
		, 				nuserrets, nretpushed));
	}

	// the number of returns cannot exceed the return space
	int ntruerets = MIN(nuserrets, nrets);

	// clear return space (todo: only clear the part that's doesn't overlap with the user rets)
	zero_values(framebase - 1, nrets);

	// copy results to return space
	copy_values(framebase - 1, E->stack_ptr - nuserrets, ntruerets);


	// put stack pointer right on top of all the returns
	E->stack_ptr = framebase - 1 + ntruerets;
	return ntruerets;
}



elf_pubapi
int elf_call(elf_State *S, int nargs, int nrets)
{
	if (nargs < 1) {
		elf_errorf(S, -1, "'call': expects at least one argument, got: %i", nargs);
	}

	pushstackframe(S);

	elf_Value value = S->stack_ptr[- nargs - 1];

	if (iscls(value)) {
		nrets = callclosure(S, vgetcls(value), nargs, nrets);
	}
	else if(isfnc(value)) {
		nrets = callfunction(S, vgetfnc(value), nargs, nrets);
	}
	else {
		elf_errorf(S, NO_BYTE, "'%s': __call expects callable value", tag2s[value.tag]);
	}

	pullstackframe(S);
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
// todo: why are we using char *!
static int callmetafield(elf_State *inter, elf_Object *obj, char *name, int nargs, elf_Value xx, elf_Value yy) {
	pushtext(inter, name);

	V vv = elf_table_get_raw(obj->meta, inter->stack_ptr[-1]);

	if (visnil(vv)) {
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
	if (visnumeric(xx) && visnumeric(yy)) {
		if (isnum(xx) || isnum(yy)) {
			eq = vitonum(xx) == vitonum(yy);
		} else {
			eq = vgetint(xx) == vgetint(yy);
		}
	}
	else if (vtagof(xx) == vtagof(yy)) {
		if (visstr(xx)) {
			eq = streq(vgetstr(xx), vgetstr(yy));
		}
		else {
			// raw compare
			eq = xx.x_int == yy.x_int;
		}
	}

	return eq;
}



/* all these are macros for generating the interpreter
so they only work within that function ya heard */


#define VMCASE(OPCODE) case OPCODE:
#define VMBREAK break


#define global_store_x(v) (globals->array[by.b_x] = v)
#define global_y() (globals->array[by.b_y])


#define vm_store_x(v) (frame.reference[by.b_x] = v)
#define rstoreXi(v) (vsetint(&frame.reference[by.b_x], v))
#define rstoreXn(v) (vsetnum(&frame.reference[by.b_x], v))

#define rvalueX(v) (frame.reference[by.b_x])
#define rvalueY(v) (frame.reference[by.b_y])
#define rvalueZ(v) (frame.reference[by.b_z])



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
		rstoreXn(nopfn(vitonum(xx), vitonum(yy)));  \
	}                                              \
	else {                                         \
		rstoreXi(iopfn(vgetint(xx), vgetint(yy)));  \
	}                                              \
} while (0)


#define arithbranchobj(name)                                        \
do {                                                                \
	int nrets = callmetafield(inter, vgetobj(xx), name, 2, xx, yy);  \
	\
	vm_store_x(inter->stack_ptr[-nrets]);                         \
	\
	inter->stack_ptr = frame.framebase + frame.framesize;            \
} while (0)


#define arithcase(name, nopfn, iopfn) \
do { \
	xx=rvalueY(), yy=rvalueZ(); \
	\
	if (visnumeric(xx)) { \
		\
		if (!visnumeric(yy)) { \
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
	xx=rvalueY(), yy=rvalueZ(); \
	\
	if (visint(xx)) { \
		\
		if (!visint(yy)) { \
			error_invalidoperandsforoperator(inter, name, xx, yy); \
		} \
		\
		rstoreXi(opfn(vgetint(xx), vgetint(yy)));   \
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
	xx=rvalueY(), yy=rvalueZ(); \
	\
	if (visnumeric(xx)) { \
		\
		if (!visnumeric(yy)) error_invalidoperandsforoperator(inter, name, xx, yy); \
		\
		if (isnum(xx) || isnum(yy)) { \
			rstoreXi(opfn(vitonum(xx), vitonum(yy))); \
		} \
		else { \
			rstoreXi(opfn(vgetint(xx), vgetint(yy))); \
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
		Interpreter Function
======================================================== */
int _resume(elf_State *inter) {
	int nrets = 0;

	Stack_Frame frame = inter->frame;
	Tab globals = inter->globals;


	// we do this here because we don't store reference in the frame
	// essentially we just have to:


	// ensure that we got enough space
	ASSERT(frame.framesize >= frame.nargs);

	// ensure the stack pointer is properly set
	ASSERT(inter->stack_ptr == frame.framebase + frame.framesize);

	// ensure that the caller set this properly
	ASSERT(frame.nextinstr == 0);

	int framecounter = 0;
	int loopcounter = 0;


	V xx,yy,zz;
	while (frame.nextinstr < frame.bytec) {
		BCPos instr = frame.nextinstr ++;

		BCPos minstr = frame.bytes + instr;
		inter->byte = minstr;

		Bytec byte = inter->bytebuf[minstr];
		// printf("%s(%i, %i, %i)\n", byte2s[byte.b_k], byte.b_x, byte.b_y, byte.b_z);

		#define by byte

		if (inter->trace_counter) {
			// we're looping
			if (!loopcounter) {
				darr_add(inter->trace, byte);
			}
		}


		switch (BC_OP(byte)) {

			VMCASE(BC_NOP)
			{
			} VMBREAK;

			VMCASE(BC_CALL)
			{
				// put stack pointer right above all the arguments
				// note how this will lead to the sub-function having
				// a framebase relative to our reference, so the results
				// get placed where we want them...
				inter->stack_ptr = frame.reference + by.b_x + by.b_y + 1;

				xx = rvalueX();

				if (iscls(xx)) {

					// save our frame
					inter->frame_stack[inter->frame_index ++] = frame;

					// update frame for new closure
					prepframeforclosure(inter, &frame, vgetcls(xx), by.b_y, by.b_z);

					inter->stack_ptr = frame.framebase + frame.framesize;

					// track the number of sub-frames locally
					framecounter ++;
				}
				else if (isfnc(xx)) {

					// we wouldn't need to do this if sub-functions didn't
					// need access the the caller's frame, for core API
					inter->frame_stack[inter->frame_index ++] = frame;

					callfunction(inter, vgetfnc(xx), by.b_y, by.b_z);

					inter->frame_index --;

					// restore stack pointer
					inter->stack_ptr = frame.framebase + frame.framesize;
				}
				else {
					elf_error(inter, -1, "cannot call");
				}

				ASSERT(inter->stack_ptr == frame.framebase + frame.framesize);
			} VMBREAK;

			VMCASE(BC_RET) {
				// we can set nrets regardless of whether we're
				// the main function or a subfunction, because
				// if we're the main function we'll exit right
				// after we set this...
				nrets = MIN(by.b_y, frame.nrets);


				// returns are placed right on the function
				copy_values(frame.framebase - 1, frame.reference + by.b_x, nrets);

				// restore stack frame
				frame = inter->frame_stack[-- inter->frame_index];
				//
				// now we're officially a different function
				//
				// sub-frame zero means we're the top call, we can exit
				// the routine function now
				if (!framecounter) {
					// set stack pointer so that our caller can pop the
					// results
					inter->stack_ptr = frame.framebase + nrets;
					goto esc;
				}
				//
				//
				framecounter --;
				//
				//
				// restore stack pointer to cover the entire frame.
				inter->stack_ptr = frame.framebase + frame.framesize;
				//
			} VMBREAK;

			case BC_J: {

				int dst = BC_ARGX(byte);
				frame.nextinstr = instr + dst;

			} break;

			case BC_JZ: {

				if (rvalueY().x_i64 == 0) {
					int dst = by.b_x;
					frame.nextinstr = instr + dst;
				}

			} break;

			case BC_JNZ: {

				if (rvalueY().x_i64 != 0) {
					int dst = by.b_x;
					frame.nextinstr = instr + dst;
				}

			} break;

			VMCASE(BC_GETGLOBAL) {
				vm_store_x(global_y());
			} VMBREAK;

			VMCASE(BC_SETGLOBAL) {
				global_store_x(rvalueY());
			} VMBREAK;

			VMCASE(BC_RELOAD)
			{
				vm_store_x(rvalueY());

				VMBREAK;
			}

			VMCASE(BC_LOADNIL) {
				vsetnil(&rvalueX());
			} VMBREAK;

			VMCASE(BC_GETKINT) {
				rstoreXi(inter->integers[by.b_y]);
			} VMBREAK;

			VMCASE(BC_GETKNUM) {
				rstoreXn(inter->numbers[by.b_y]);
			} VMBREAK;


			VMCASE(BC_BIT_NOT) {
				if (!visint(rvalueY())) {
					elf_errorf(inter, -1, "'~': expects integer");
				}
				rstoreXi( ~ vgetint(rvalueY()));
			} VMBREAK;


			VMCASE(BC_GETUPVAL) {
				ASSERT(WITHIN(BC_ARGY(byte), 0, frame.closuresize));
				vm_store_x(frame.closureenv[by.b_y]);
			} VMBREAK;

			case BC_CLOSURE: {
				ASSERT(WITHIN(BC_ARGY(byte), 0, darr_l(inter->protos)));

				int proto_index = BC_ARGY(byte);

				Proto proto = inter->protos[proto_index];

				Closure closure;

				int size = sizeof(*closure) + sizeof(closure->captures[0]) * proto.ncaptures;

				closure = (Closure) gcalloc(inter, GC_CLS, size);
				closure->proto_index = proto_index;
				closure->proto = proto;
				copy_values(closure->captures, &rvalueX(), proto.ncaptures);

				vsetcls(&rvalueX(), closure);
			} break;


			case BC_TABLE: {

				elf_Table *tab = new_table(inter);
				vsettab(&rvalueX(), tab);

			} break;

			case BC_GETMETAFIELD: {
				yy = rvalueY();

				elf_Table *metatable = 0;

				switch (yy.tag) {
					case ELF_TSTRING:
					case ELF_TTABLE:
					case ELF_TUSER:
					case ELF_TCLOSURE: {
						metatable = vgetobj(yy)->meta;
					} break;

					case ELF_TNUMBER: {
						metatable = inter->metatables.number;
					} break;

					case ELF_TINTEGER: {
						metatable = inter->metatables.integer;
					} break;

					default: {
						elf_errorf(inter, minstr, "'%s': not an object", tag2s[yy.tag]);
					} break;
				}

				if (!metatable) {
					elf_errorf(inter, minstr, "'%s': invalid object, no meta-table", tag2s[yy.tag]);
				}

				V metafield = elf_table_get_raw(metatable, rvalueZ());
				vm_store_x(metafield);
			} break;

			VMCASE(BC_SETINDEX) {
				V table_v = rvalueX();
				V index_v = rvalueY();
				V value = rvalueZ();

				if (!vistab(table_v)) {
					elf_error(inter, minstr, "invalid left operand");
				}
				if (!visint(index_v)) {
					elf_error(inter, minstr, "invalid index type");
				}

				V *array = vgettab(table_v)->array;
				Index index = vgetint(index_v);

				if (index < 0) {
					index += darr_l(array);
				}
				if (index < 0 || index >= darr_l(array)) {
					elf_error(inter, minstr, "index out of bounds");
				}

				array[index] = value;
			} VMBREAK;

			VMCASE(BC_GETINDEX) {
				V table_v = rvalueY();
				V index_v = rvalueZ();

				if (!vistab(table_v)) {
					elf_error(inter, minstr, "invalid left operand");
				}
				if (!visint(index_v)) {
					elf_error(inter, minstr, "invalid index type");
				}

				V *array = vgettab(table_v)->array;
				Index index = vgetint(index_v);

				if (index < 0) {
					index += darr_l(array);
				}
				if (index < 0 || index >= darr_l(array)) {
					elf_error(inter, minstr, "index out of bounds");
				}

				vm_store_x(array[index]);
			} VMBREAK;

			case BC_GETFIELD: {
				xx=rvalueY(), yy=rvalueZ();

				if (visnil(yy)) {
					elf_error(inter, minstr, "attempted to get nil field");
				}
				switch (xx.tag) {

					case ELF_TTABLE: {
						vm_store_x(elf_table_get_raw(vgettab(xx), yy));
					}break;

					case ELF_TUSER: {
						// _callov(R,xx.x_obj,"__getfield",BC_ARGX(byte),1,&yy);
						__debugbreak();
					}break;

					case ELF_TSTRING: {

						if (visint(yy)) {
							int index = vgetint(yy);
							rstoreXi(vgetstr(xx)->text[index]);
						}
						else if (visstr(yy)) {

							int index = find_subtext(vgetstr(xx)->text, vgetstr(yy)->text);
							rstoreXi(index);
						}
						else {
							elf_error(inter, minstr, "invalid value for string:__index(of int)");
						}

					}break;
					case ELF_TNIL: {
						elf_error(inter, minstr
						, "attempted to get field of 'nil' value");
					} break;
					default: {
						elf_errorf(inter, minstr
						, "'%s': invalid x operand for instruction", vtag2s(xx));
					} break;
				}
			} break;

			case BC_SETFIELD: {
				xx=rvalueX(), yy=rvalueY(), zz=rvalueZ();

				if (vistab(xx)) {
					if (visnil(yy)) elf_error(inter, minstr, "key is nil...");

					tableset(xx.x_tab, yy, zz);
				}
				else if (isusr(xx)) {
					elf_error(inter, minstr, "overload not implemented");
				}
				else if (visstr(xx)) {
					elf_error(inter, minstr, "strings are constant, you may not change them");
				}
				else {
					elf_errorf(inter, minstr, "attempted to set field of '%s' value", tag2s[xx.tag]);
				}
			} break;

			case BC_N2I:
			{
				rstoreXi(vntoint(rvalueX()));
			} break;

			case BC_I2N:
			{
				rstoreXn(vitonum(rvalueX()));
			} break;

			case BC_NEQ: {
				bool eq = veq(inter, rvalueY(), rvalueZ());
				rstoreXi(!eq);
			} break;
			case BC_EQ: {
				bool eq = veq(inter, rvalueY(), rvalueZ());
				rstoreXi(eq);
			} break;


			VMCASE(BC_BIT_SHL)  { bwcase("__shl", __shl); } VMBREAK;
			VMCASE(BC_BIT_SHR)  { bwcase("__shr", __shr); } VMBREAK;
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
				if(trace_value.tag!=ELF_TNIL){
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
					darr_add(R->trace_buffer,byte);
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
						elf_ldebug("finished recording trace, %i bytes recorded", R->active_trace_len);
					}
				}else{
					ASSERT(R->trace_inner_loop_counter == 0);
					elf_Value trace_value = elf_table_get_raw(R->trace_table,VALUE_INTEGER(trace_start_instr));
					if(trace_value.tag==ELF_TNIL){
						if(R->track[minstr]>=64){
							R->flags |= FLAG_TRACING;
							R->trace_start_instr = trace_start_instr;
							R->trace_stop_instr  = minstr;
							R->active_trace_pos  = darr_l(R->trace_buffer);
							R->active_trace_len  = 0;
							tableset(R->trace_table,VALUE_INTEGER(trace_start_instr),VALUE_INTEGER(R->active_trace_pos));
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