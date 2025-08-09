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

#include "system.h"

#include "internal_utils.h"
#include "subsystem.h"
#include "logging.c"


#include "bytecode_metadata.h"


#include "internal_types.h"
#include "internal_api.h"
#include "internal_helpers.h"
#include "internal_helpers.c"
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
#include "l_vec.c"
#include "l_table.c"
#include "l_string.c"
#include "l_random.c"


static int _resume(elf_State *S);


int elf_get_global_slot(elf_State *S, elf_String *name) {

	if (name != 0) {
		elf_Value vname;
		vsetstr(&vname, name);
		return elf_table_get_index_always_(S->globals, vname);
	}

	return ARRAY_GROW(S->globals->array, 1);
}

int elf_set_global(elf_State *S, elf_String *name, elf_Value value) {

	int id = elf_get_global_slot(S, name);
	S->globals->array[id] = value;

	return id;
}



/* expects a table on the stack
todo: also, this functions is weird? */
static void install(elf_State *S, char *prefix, const elf_Binding *lib, int num) {
	for(int i = 0; i < num; i ++) {

		char *name = lib[i].name;

		// todo: ensure the symbol name is valid?
		if (prefix) name = elf_tpf("%s.%s",prefix,name);

		elf_pushstr(S, name);
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
	install(S,"elf" ,  lib_core  , COUNTOF(lib_core));
	install(S,"elf" ,  lib_random, COUNTOF(lib_random));
	install(S,"elf" ,  l_sys,      COUNTOF(l_sys));


	ASSERT(stackcursor(S) < S->frame.framesize);

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



// update the current frame for the given closure
static inline void
prepframeforclosure(elf_State *inter, elf_Closure *closure, int nargs, int nrets)
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



static inline int
callclosure(elf_State *S, elf_Closure *closure, int nargs, int nrets)
{
	elf_Proto proto = closure->proto;

	prepframeforclosure(S, closure, nargs, nrets);

	// update stack pointer to cover the entire frame
	S->stack_ptr = S->frame.framebase + S->frame.framesize;

	nrets = _resume(S);

	// put stack pointer right on top of all the returns
	S->stack_ptr = S->frame.framebase - 1 + nrets;
	return nrets;
}



static inline int
callfunction(elf_State *inter, elf_Function proc, int nargs, int nrets)
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
int elf_call(elf_State *S, int nargs, int nrets)
{
	if (nargs < 1) {
		// __call wants at least the 'this' arg
		pushnil(S);
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
static int callmetafield(elf_State *inter, elf_Object *obj, char *name, int nargs, elf_Value xx, elf_Value yy) {
	elf_pushstr(inter, name);

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


#define global_store_x(v) (globals->array[by.b_x] = v)
#define global_y() (globals->array[by.b_y])


#define local_store_x(v) (frame.framebase[by.b_x] = v)
#define local_store_x_int(v) (vsetint(&frame.framebase[by.b_x], v))
#define local_store_x_num(v) (vsetnum(&frame.framebase[by.b_x], v))

#define local_x(v) frame.framebase[by.b_x]
#define local_y(v) frame.framebase[by.b_y]
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
		local_store_x_num(nopfn(vitonum(xx), vitonum(yy)));  \
	}                                              \
	else {                                         \
		local_store_x_int(iopfn(vgetint(xx), vgetint(yy)));  \
	}                                              \
} while (0)


#define arithbranchobj(name)                                        \
do {                                                                \
	int nrets = callmetafield(inter, vgetobj(xx), name, 2, xx, yy);  \
	\
	local_store_x(inter->stack_ptr[-nrets]);                                 \
	\
	inter->stack_ptr = frame.framebase + frame.framesize;            \
} while (0)


#define arithcase(name, nopfn, iopfn) \
do { \
	xx=local_y(), yy=localz(); \
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
	xx=local_y(), yy=localz(); \
	\
	if (isint(xx)) { \
		\
		if (!isint(yy)) { \
			error_invalidoperandsforoperator(inter, name, xx, yy); \
		} \
		\
		local_store_x_int(opfn(vgetint(xx), vgetint(yy)));   \
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
	xx=local_y(), yy=localz(); \
	\
	if (isnumeric(xx)) { \
		\
		if (!isnumeric(yy)) error_invalidoperandsforoperator(inter, name, xx, yy); \
		\
		if (isnum(xx) || isnum(yy)) { \
			local_store_x_int(opfn(vitonum(xx), vitonum(yy))); \
		} \
		else { \
			local_store_x_int(opfn(vgetint(xx), vgetint(yy))); \
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
	Stack_Frame frame = inter->frame;
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

				xx = local_x();

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


				// note that 'framebase' is right below the function,
				// or at framebase - 1, so the returns are thus placed
				// at framebase - 1
				copy_memory(frame.framebase - 1, frame.framebase + by.b_x, inter->frame.nrets * sizeof(elf_Value));

				// restore stack frame
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

				int dst = BC_ARGX(byte);
				frame.nextinstr = instr + dst;

			} break;

			case BC_JZ: {

				if (local_y().x_i64 == 0) {
					int dst = by.b_x;
					frame.nextinstr = instr + dst;
				}

			} break;

			case BC_JNZ: {

				if (local_y().x_i64 != 0) {
					int dst = by.b_x;
					frame.nextinstr = instr + dst;
				}

			} break;

			VMCASE(BC_GETGLOBAL) { local_store_x(global_y()); } VMBREAK;

			VMCASE(BC_SETGLOBAL) { global_store_x(local_y());  } VMBREAK;

			VMCASE(BC_RELOAD)  { local_store_x(local_y()); } VMBREAK;

			VMCASE(BC_LOADNIL) { vsetnil(&local_x()); } VMBREAK;

			VMCASE(BC_GETKINT) { local_store_x_int(inter->integers[by.b_y]); } VMBREAK;

			VMCASE(BC_GETKNUM) { local_store_x_num(inter-> numbers[by.b_y]); } VMBREAK;

			VMCASE(BC_GETUPVAL) {
				ASSERT(WITHIN(BC_ARGY(byte), 0, frame.closuresize));
				local_store_x(frame.closureenv[by.b_y]);
			} VMBREAK;

			case BC_CLOSURE: {
				ASSERT(WITHIN(BC_ARGY(byte), 0, darr_l(inter->protos)));

				elf_Proto proto = inter->protos[BC_ARGY(byte)];

				elf_Closure *cls = elf_alloc_closure(inter, proto);
				copy_memory(cls->captures, &local_x(), proto.ncaptures * sizeof(elf_Value));

				vsetcls(&local_x(), cls);
			} break;
			case BC_TABLE: {

				elf_Table *tab = elf_alloc_table(R);
				vsettab(&local_x(), tab);

			} break;

			case BC_GETMETAFIELD: {
				yy = local_y();

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

				elf_Value metafield = elf_table_get_raw(metatable, localz());
				local_store_x(metafield);
			} break;


			// todo: INDEX should be array mode!
			case BC_GETINDEX: case BC_GETFIELD: {
				xx=local_y(), yy=localz();

				if (isnil(yy)) {
					elf_error(inter, minstr, "attempted to get nil field");
				}
				switch (xx.tag) {

					case ELF_TTABLE: {
						local_store_x(elf_table_get_raw(vgettab(xx), yy));
					}break;

					case ELF_TUSER: {
						// _callov(R,xx.x_obj,"__getfield",BC_ARGX(byte),1,&yy);
						__debugbreak();
					}break;

					case ELF_TSTRING: {

						if (isint(yy)) {
							int index = vgetint(yy);
							local_store_x_int(vgetstr(xx)->text[index]);
						}
						else if (isstr(yy)) {

							int index = find_subtext(vgetstr(xx)->text, vgetstr(yy)->text);
							local_store_x_int(index);
						}
						else {
							elf_error(inter, minstr, "invalid value for string:__index(of int)");
						}

					}break;
					case ELF_TNIL: {
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
				xx=local_x(), yy=local_y(), zz=localz();

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
				local_store_x_int(vntoint(local_x()));
			} break;

			case BC_I2N:
			{
				local_store_x_num(vitonum(local_x()));
			} break;

			case BC_NEQ: {
				bool eq = veq(inter, local_y(), localz());
				local_store_x_int(!eq);
			} break;
			case BC_EQ: {
				bool eq = veq(inter, local_y(), localz());
				local_store_x_int(eq);
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
					d_array_add(R->trace_buffer,byte);
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
					if(trace_value.tag==ELF_TNIL){
						if(R->track[minstr]>=64){
							R->flags |= FLAG_TRACING;
							R->trace_start_instr = trace_start_instr;
							R->trace_stop_instr  = minstr;
							R->active_trace_pos  = darr_l(R->trace_buffer);
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