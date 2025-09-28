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

#include "unparse.c"
#include "visitor.c"

#include "string.c"
#include "table.c"
#include "buffer.c"

#include "internal_gc.c"

#include "elf_compiler.h"

#include "instructions.c"


#include "l_math.c"
#include "l_core.c"
#include "l_sys.c"
#include "l_table.c"
#include "l_string.c"
#include "l_random.c"









int callclosure(elf_State *S, Closure closure, int nargs, int nrets);










/* todo: remove this! */
static void install(elf_State *S, char *prefix, const elf_Binding *lib, int num) {
	for(int i = 0; i < num; i ++) {

		char *name = lib[i].name;

		// todo: ensure the symbol name is valid?
		if (prefix) name = tpf("%s.%s",prefix,name);

		elf_pushtext(S, name);
		elf_pushfun(S, lib[i].function);

		elf_setfield(S);
	}
}












//
// todo: we over rely on the virtual memory system
//
void elf_init_raw(elf_State *S) {
	// todo: can we use the value regular stack instead
	S->frame_stack = sys_virtual_alloc(GIGABYTES(1));
	S->frame_stack_max = GIGABYTES(1) / sizeof(*S->frame_stack);

	S->stack = sys_virtual_alloc(GIGABYTES(1));
	S->stack_max = GIGABYTES(1) / sizeof(*S->stack);
	S->stack_ptr = S->stack;

	S->gc.memthreshold = GC_MEM_THRESHOLD_MIN;
	S->gc.objthreshold = GC_OBJ_THRESHOLD_MIN;
	S->gc.tail = & S->gc.head;


	// it literally doesn't matter, but techinically, framebase-1
	// is where the function is at, we don't have one, so nil.
	pushnil(S);

	S->frame.framebase = S->stack;
	S->frame.framesize = 16;

#if 0
	S->exec_trail_capacity = 128;
	S->exec_trail = sys_virtual_alloc(S->exec_trail_capacity * sizeof(*S->exec_trail));
	S->trace_table = pushnewtable(S);
#endif



	// todo: metatables should be per module?
	S->metatables.string = pushnewtable(S);
	install(S, 0, string_metafuncs, COUNTOF(string_metafuncs));

	S->metatables.table = pushnewtable(S);
	install(S, 0, l_table, COUNTOF(l_table));


	S->metatables.buffer = pushnewtable(S);
	install(S, 0, l_buffer, COUNTOF(l_buffer));

	// S->strings = pushnewtable(S);

	// todo: builtin instrinsics
	static elf_Binding lib_base[] = {
		{"ntoi", l_core_ntoi},
		{"iton", l_core_iton},
	};

	S->globals = pushnewtable(S);
	install(S,     0,  lib_base  , COUNTOF(lib_base));
	install(S,     0,  lib_math  , COUNTOF(lib_math));
	install(S,"elf" ,  l_core    , COUNTOF(l_core));
	install(S,"elf" ,  lib_random, COUNTOF(lib_random));
	install(S,"elf" ,  l_sys,      COUNTOF(l_sys));


	ASSERT(stack2index(S) < S->frame.framesize);

	setstackptr(S, S->frame.framebase + S->frame.framesize);
}












// nrets would not matter for the frame size because returns are pushed
// regardless
static inline int callfunction(elf_State *S, elf_Function function, int nargs, int nrets)
{
	V *framebase = S->stack_ptr - nargs;
	V *reference = framebase;

	int framesize = nargs;

	S->frame.framebase = framebase;
	S->frame.reference = reference;
	S->frame.framesize = framesize;
	S->frame.nargs = nargs;
	S->frame.nrets = nrets;


	setstackptr(S, framebase + framesize);



	int ncallrets = function(S, nargs, nrets);

	int npushrets = S->stack_ptr - framebase - framesize;


	// Ensure that the number of returns is consistent with the stack state.
	// The user could have used the stack for other things, so we only enforce
	// that the stack pointer incremented by the at least the number of
	// the returns the user said
	if (ncallrets < 0 || npushrets < ncallrets)
	{
		reporterrorf(S, NO_BYTE
		,	"invalid number of returns from function: %i, however the function pushed: %i"
		,   ncallrets, npushrets);
	}


	int ntruerets = MIN(ncallrets, nrets);

	if (ncallrets < nrets) {
		zero_values(framebase - 1 + ncallrets, nrets - ncallrets);
	}

	// copy results to return space
	copy_values(framebase - 1, S->stack_ptr - ncallrets, ntruerets);


	// put stack pointer right on top of all the returns
	S->stack_ptr = framebase - 1 + nrets;
	return ntruerets;
}








int elf_tailcall(elf_State *S, int nargs, int nrets) {

	int frame_index = S->frame_index;

	V value = S->stack_ptr[- nargs - 1];

	if (is_closure(value)) {
		nrets = callclosure(S, as_closure(value), nargs, nrets);
	}
	else if(is_function(value)) {
		nrets = callfunction(S, as_function(value), nargs, nrets);
	}
	else {
		reporterrorf(S, NO_BYTE, "'%s': __call expects callable value", tag2s[value.tag]);
	}

	ASSERT(S->frame_index == frame_index);

	return nrets;
}






static inline void pushstackframe(elf_State *S) {
	ASSERT(S->frame_index < S->frame_stack_max);
	S->frame_stack[S->frame_index ++] = S->frame;
}


static inline void pullstackframe(elf_State *S) {
	ASSERT(S->frame_index > 0);
	S->frame = S->frame_stack[-- S->frame_index];
}


int elf_call(elf_State *S, int nargs, int nrets)
{
	if (nargs < 1) {
		reporterrorf(S, -1, "'call': expects at least one argument, got: %i", nargs);
	}

	pushstackframe(S);

	nrets = elf_tailcall(S, nargs, nrets);

	pullstackframe(S);
	return nrets;
}











// todo: proper error codes!
static void error_expectednumericrightoperand(elf_State *S, char *name, elf_Value x, elf_Value y)
{
	reporterrorf(S, -1, "'%s': invalid right-operand, for operator %s", tag2s[x.tag], name);
}








static void error_invalidoperandsforoperator(elf_State *S, char *name, elf_Value x, elf_Value y)
{
	reporterrorf(S, -1, "'%s': invalid operands for operator %s, (%s, %s)", tag2s[x.tag], name, tag2s[x.tag], tag2s[y.tag]);
}













static inline Array check_array(elf_State *S, V v) {
	typecheck(S, v, ELF_TTABLE);
	return as_table(v)->array;
}








// todo: onlycheck index type, _table_arrayset and _table_arrayget
// already check the index
static inline Index check_index(elf_State *S, V v, Index l) {
	typecheck(S, v, ELF_TINTEGER);

	Index index = as_int(v);

	if (index < 0) {
		index += l;
	}

	if (index < 0 || index >= l) {
		reporterror(S, -1, "index out of bounds");
	}

	return index;
}

















































/* all these are macros for generating the interpreter
so they only work within that function ya heard */


#define VMCASE(OPCODE) case OPCODE:
#define VMBREAK break


#define global_store_x(v) (globals->array[by.b_x] = v)
#define global_x() (globals->array[by.b_x])
#define global_y() (globals->array[by.b_y])


#define StoreX(v) (frame.reference[by.b_x] = v)
#define StoreXInt(v) (to_int(&frame.reference[by.b_x], v))
#define StoreXNum(v) (to_num(&frame.reference[by.b_x], v))

#define LoadX() (frame.reference[by.b_x])
#define LoadY() (frame.reference[by.b_y])
#define LoadZ() (frame.reference[by.b_z])

#define LoadYInt() as_int(LoadY())


























/* =====================================================
		Interpreter Function
======================================================== */




// update the given frame to match the given closure's needs
static inline void prepframeforclosure(elf_State *S, Stack_Frame *frame, elf_Closure *closure, int nargs, int nrets)
{
	Proto proto = closure->proto;

	// todo: do not grow frame size less we care about vargs
	int framesize = nargs < proto.stacksize ? proto.stacksize : nargs;


	// todo: don't do this here, because when we call prepframeforclosure
	// from the closure, we have to increment the stackpointer to account
	// for this, when we already know this information
	// this should be done in callclosure which is outward facing!
	V *framebase = S->stack_ptr - nargs;


	V *reference = framebase;

	// Do we care about excess args? And do we have any?
	// todo: unlikely
	if (proto.variadic && nargs > proto.arity) {
		reference += nargs;
		framesize += proto.arity;
		copy_values(reference, framebase, proto.arity);
	}


	// todo: this is redundant, we already zero when we set the stack pointer!
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

	// todo: this will zero too!
	setstackptr(S, framebase + framesize);
}














// todo: outward facing root call should not write results to the function,
// this is only bytecode stuff!
int callclosure(elf_State *S, Closure closure, int nargs, int nrets) {

	Tab globals = S->globals;


	Stack_Frame frame;
	prepframeforclosure(S, &frame, closure, nargs, nrets);


	V *frameroot = frame.framebase;

	int subframecounter = 0;
	int loopcounter     = 0;


	V xx,yy,zz;
	while (frame.nextinstr < frame.bytec) {
		BCPos instr = frame.nextinstr ++;

		BCPos minstr = frame.bytes + instr;
		S->byte = minstr;

		Bytec byte = S->bytebuf[minstr];
		// printf("%s(%i, %i, %i)\n", byte2s[byte.b_k], byte.b_x, byte.b_y, byte.b_z);

		#define by byte


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
				S->stack_ptr = frame.reference + by.b_x + by.b_y + 1;

				xx = LoadX();

				if (is_closure(xx)) {

					// save our frame, this is restored on return instruction
					S->frame_stack[S->frame_index ++] = frame;

					// update frame for new closure
					prepframeforclosure(S, &frame, as_closure(xx), by.b_y, by.b_z);


					// track the number of sub-frames locally
					subframecounter ++;
				}
				else if (is_function(xx)) {

					// we only do this to make our frame visible to
					// the core API, eventually maybe remove this to
					// avoid having to do this.
					S->frame_stack[S->frame_index ++] = frame;

					callfunction(S, as_function(xx), by.b_y, by.b_z);

					// note how we don't store nor restore S->frame, so
					// from this point onwards, S->frame will no longer
					// reflect the state of our local frame, we need to
					// ensure that when we exit the function, no-one assumes
					// that S->frame is the frame of callclosure
					S->frame_index --;

					// restore stack pointer
					setstackptr(S, frame.framebase + frame.framesize);
				}
				else {
					reporterrorf(S, -1, "cannot call '%s'", tag2s[xx.tag]);
				}

				ASSERT(S->stack_ptr == frame.framebase + frame.framesize);
			} VMBREAK;

			VMCASE(BC_RET) {
				nrets = MIN(by.b_y, frame.nrets);

				// returns are placed right on the function
				copy_values(frame.framebase - 1, frame.reference + by.b_x, nrets);

				ASSERT(S->frame_index > 0);
				if (subframecounter) {
					frame = S->frame_stack[-- S->frame_index];

					setstackptr(S, frame.framebase + frame.framesize);

					subframecounter --;
				}
				else {
					ASSERT(frame.framebase == frameroot);
					S->stack_ptr = frame.framebase - 1 + nrets;
					goto esc;
				}
			} VMBREAK;

			case BC_J: {

				int dst = BC_ARGX(byte);
				frame.nextinstr = instr + dst;

			} break;

			case BC_JZ: {

				if (LoadYInt() == 0) {
					int dst = by.b_x;
					frame.nextinstr = instr + dst;
				}

			} break;

			case BC_JNZ: {

				if (LoadYInt() != 0) {
					int dst = by.b_x;
					frame.nextinstr = instr + dst;
				}

			} break;




			VMCASE(BC_ENFORCE) {

				typerulecheck(S, LoadX(), by.b_y);

			} VMBREAK;




			VMCASE(BC_LOADGLOBAL) {
				StoreX(global_y());
			} VMBREAK;

			VMCASE(BC_LOADNIL) {
				to_nil(&LoadX());
			} VMBREAK;

			VMCASE(BC_LOADKINT) {
				to_int(&LoadX(), S->integers[by.b_y]);
			} VMBREAK;

			VMCASE(BC_LOADKNUM) {
				to_num(&LoadX(), S->numbers[by.b_y]);
			} VMBREAK;

			VMCASE(BC_SETGLOBAL) {
				vmove(&global_x(), &LoadY());
			} VMBREAK;

			VMCASE(BC_RELOAD) {
				vmove(&LoadX(), &LoadY());
			} VMBREAK;


			VMCASE(BC_BIT_NOT) {
				if (!is_int(LoadY())) {
					reporterrorf(S, -1, "'~': expects integer");
				}
				StoreXInt( ~ as_int(LoadY()));
			} VMBREAK;


			VMCASE(BC_LOADCVAL) {
				ASSERT(WITHIN(BC_ARGY(byte), 0, frame.closuresize));
				StoreX(frame.closureenv[by.b_y]);
			} VMBREAK;

			case BC_CLOSURE: {
				ASSERT(WITHIN(BC_ARGY(byte), 0, heap_array_length(S->protos)));

				int proto_index = BC_ARGY(byte);

				Proto proto = S->protos[proto_index];

				Closure closure;

				int size = sizeof(*closure) + sizeof(closure->captures[0]) * proto.ncaptures;

				closure = (Closure) gcalloc(S, GC_CLS, size);
				closure->proto_index = proto_index;
				closure->proto = proto;
				copy_values(closure->captures, &LoadX(), proto.ncaptures);

				to_cls(&LoadX(), closure);
			} break;


			case BC_TABLE: {

				elf_Table *tab = new_table(S);
				to_tab(&LoadX(), tab);

			} break;





			VMCASE(BC_GETMETAFIELD)
			{

				V metafield = _get_metafield(S, LoadY(), LoadZ());
				StoreX(metafield);

			} VMBREAK;




			VMCASE(BC_GETLENGTH)
			{

				Array array  = check_array(S, LoadY());
				StoreXInt(heap_array_length(array));

			} VMBREAK;




			VMCASE(BC_SETINDEX)
			{

				Array array  = check_array(S, LoadX());
				Index index  = check_index(S, LoadY(), heap_array_length(array));
				array[index] = LoadZ();

			} VMBREAK;



			VMCASE(BC_GETINDEX)
			{

				Array array = check_array(S, LoadY());
				Index index = check_index(S, LoadZ(), heap_array_length(array));
				StoreX(array[index]);

			} VMBREAK;

			case BC_GETFIELD: {

				_get_field(S, &LoadX(), LoadY(), LoadZ());

			} break;

			case BC_SETFIELD: {
				xx=LoadX(), yy=LoadY(), zz=LoadZ();

				if (is_tab(xx)) {
					if (is_nil(yy)) reporterror(S, minstr, "key is nil...");

					tableset(S,  xx.x_tab, yy, zz);
				}
				else if (isusr(xx)) {
					reporterror(S, minstr, "overload not implemented");
				}
				else if (is_str(xx)) {
					reporterror(S, minstr, "strings are readonly, you may not change them");
				}
				else {
					reporterrorf(S, minstr, "attempted to set field of '%s' value", tag2s[xx.tag]);
				}
			} break;

			case BC_N2I:
			{
				to_int(&LoadX(), num_to_int(LoadY()));
			} break;

			case BC_I2N:
			{
				to_num(&LoadX(), int_to_num(LoadY()));
			} break;


			VMCASE(BC_NEQ) {
				v__neq(S, &LoadX(), LoadY(), LoadZ());
			} VMBREAK;

			VMCASE(BC_EQ) {
				v__eq(S, &LoadX(), LoadY(), LoadZ());
			} VMBREAK;


			VMCASE(BC_BIT_SHL) {
				v__shl(S, &LoadX(), LoadY(), LoadZ());
			} VMBREAK;

			VMCASE(BC_BIT_SHR) {
				v__shr(S, &LoadX(), LoadY(), LoadZ());
			} VMBREAK;

			VMCASE(BC_BIT_XOR) {
				v__eor(S, &LoadX(), LoadY(), LoadZ());
			} VMBREAK;

			VMCASE(BC_BIT_OR) {
				v__ior(S, &LoadX(), LoadY(), LoadZ());
			} VMBREAK;

			VMCASE(BC_BIT_AND) {
				v__and(S, &LoadX(), LoadY(), LoadZ());
			} VMBREAK;


			VMCASE(BC_MUL) {
				v__mul(S, &LoadX(), LoadY(), LoadZ());
			} VMBREAK;
			VMCASE(BC_DIV) {
				v__div(S, &LoadX(), LoadY(), LoadZ());
			} VMBREAK;
			VMCASE(BC_ADD) {
				v__add(S, &LoadX(), LoadY(), LoadZ());
			} VMBREAK;
			VMCASE(BC_SUB) {
				v__sub(S, &LoadX(), LoadY(), LoadZ());
			} VMBREAK;
			VMCASE(BC_MOD) {
				v__mod(S, &LoadX(), LoadY(), LoadZ());
			} VMBREAK;
			VMCASE(BC_POW) {
				v__pow(S, &LoadX(), LoadY(), LoadZ());
			} VMBREAK;


			VMCASE(BC_LT) {
				v__lt(S, &LoadX(), LoadY(), LoadZ());
			} VMBREAK;

			VMCASE(BC_LTEQ) {
				v__lteq(S, &LoadX(), LoadY(), LoadZ());
			} VMBREAK;

			default: {
				reporterrorf(S, minstr, "unsupported instruction: %s", byte2s[BC_OP(byte)]);
			} break;
		}
	}

	esc:
	return nrets;
}






