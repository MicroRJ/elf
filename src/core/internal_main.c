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

#include "gc.c"

#include "compiler.h"

#include "intrinsics.c"


#include "l_math.c"
#include "l_core.c"
#include "l_sys.c"
#include "l_table.c"
#include "l_string.c"
#include "l_random.c"
#include "l_sockets.c"









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

void _initstate(elf_State *S) {
	// todo: we over rely on the virtual memory system

	// todo: to be removed
	{
		S->frame_stack_max = GIGABYTES(1) / sizeof(*S->frame_stack);
		S->frame_stack = sys_virtual_alloc(sizeof(*S->frame_stack) * S->frame_stack_max);
	}

	{
		S->stack_max = GIGABYTES(1) / sizeof(*S->stack);
		S->stack = sys_virtual_alloc(sizeof(*S->stack) * S->stack_max);
		S->stack_ptr = S->stack;
	}

	{
		// todo: isn't there a way to create native circular buffer using
		// some form of paged addressing
		S->record_max = 1024;
		S->record_min = 0;
		S->record = sys_virtual_alloc(sizeof(*S->record) * S->record_max);
	}

	S->gc.memthreshold = GC_MEM_THRESHOLD_MIN;
	S->gc.objthreshold = GC_OBJ_THRESHOLD_MIN;
	S->gc.tail = & S->gc.head;


	// it literally doesn't matter, but technically, framebase-1
	// is where the function is at, we don't have one, so nil.
	pushnil(S);

	S->frame.framebase = S->stack;
	S->frame.framesize = 16;

	// todo: metatables should be per module?
	S->metatables.string = pushnewtable(S);
	install(S, 0, string_metafuncs, COUNTOF(string_metafuncs));

	S->metatables.table = pushnewtable(S);
	install(S, 0, l_table, COUNTOF(l_table));


	S->metatables.buffer = pushnewtable(S);
	install(S, 0, l_buffer, COUNTOF(l_buffer));

	// S->strings = pushnewtable(S);

	// todo: builtin intrinsics
	static elf_Binding lib_base[] = {
		{"ntoi", l_core_ntoi},
		{"iton", l_core_iton},
	};

	S->globals = pushnewtable(S);
	install(S,"elf.sockets",  l_sockets  , COUNTOF(l_sockets));
	install(S,           0 ,  lib_base   , COUNTOF(lib_base));
	install(S,           0 ,  lib_math   , COUNTOF(lib_math));
	install(S, "elf"       ,  l_core     , COUNTOF(l_core));
	install(S, "elf"       ,  lib_random , COUNTOF(lib_random));
	install(S, "elf"       ,  l_sys      , COUNTOF(l_sys));


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


#define global_x() (globals->array[by.b_x])
#define global_y() (globals->array[by.b_y])
#define LoadX() (frame.reference[by.b_x])
#define LoadY() (frame.reference[by.b_y])
#define LoadZ() (frame.reference[by.b_z])



/* =====================================================
		Interpreter Function
======================================================== */




// update the given frame to match the given closure's needs
static inline void prepframeforclosure(elf_State *S, Stack_Frame *frame, elf_Closure *closure, int nargs, int nrets)
{
	BytecodeFunction proto = closure->proto;

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


#define _pushrec0(S,b,o) _pushrec(S,b,o,NIL_VALUE,NIL_VALUE)
#define _pushrec1(S,b,o,x) _pushrec(S,b,o,x,NIL_VALUE)
static inline void _pushrec(elf_State *S, Bytecode b, V o, V x, V y)
{
	Record *rec = & S->record[S->record_min ++ & (S->record_max - 1)];
	rec->b = b;
	rec->o = o;
	rec->x = x;
	rec->y = y;
}

static void _tracerecords(elf_State *S, u32 x) {
	u32 mask = S->record_max - 1;
	for (u32 i = S->record_min - 1; i >= 0; -- i) {
		Record *rec = & S->record[i & mask];
		Bytecode b = rec->b;

		switch (b.b_k) {
			case BYTECODE_LT:
			case BYTECODE_LTEQ:
			case BYTECODE_EQ:
			case BYTECODE_NEQ:
			case BYTECODE_MOD:
			case BYTECODE_POW:
			case BYTECODE_MUL:
			case BYTECODE_DIV:
			case BYTECODE_ADD:
			case BYTECODE_SUB:
			case BYTECODE_BIT_XOR:
			case BYTECODE_BIT_SHL:
			case BYTECODE_BIT_SHR:
			case BYTECODE_BIT_OR: {
				if (b.b_x == x) {
					printf("instr write to: \n");
					printf("%s(%i, %i, %i)\n", Static_StrFromBytecode[b.b_k], b.b_x, b.b_y, b.b_z);
				}
			} break;
		}
	}
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

		Bytecode byte = S->bytebuf[minstr];

		// printf("%s(%i, %i, %i)\n", Static_StrFromBytecode[byte.b_k], byte.b_x, byte.b_y, byte.b_z);

		#define by byte


		switch (BYTECODE_OP(byte)) {

			VMCASE(BYTECODE_NOP)
			{
			} VMBREAK;

			VMCASE(BYTECODE_CALL)
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

			VMCASE(BYTECODE_RET) {
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

			case BYTECODE_J: {

				int dst = BYTECODE_ARGX(byte);
				frame.nextinstr = instr + dst;

			} break;

			case BYTECODE_JZ: {

				if (as_int(LoadY()) == 0) {
					int dst = by.b_x;
					frame.nextinstr = instr + dst;
				}

			} break;

			case BYTECODE_JNZ: {

				if (as_int(LoadY()) != 0) {
					int dst = by.b_x;
					frame.nextinstr = instr + dst;
				}

			} break;


			VMCASE(BYTECODE_LOADNIL) {
				to_nil(&LoadX());
				_pushrec0(S, byte, LoadX());
			} VMBREAK;

			VMCASE(BYTECODE_LOADKINT) {
				to_int(&LoadX(), S->integers[by.b_y]);
				_pushrec0(S, byte, LoadX());
			} VMBREAK;

			VMCASE(BYTECODE_LOADKNUM) {
				to_num(&LoadX(), S->numbers[by.b_y]);
				_pushrec0(S, byte, LoadX());
			} VMBREAK;


			VMCASE(BYTECODE_LOADGLOBAL) {
				vmove(&LoadX(), global_y());
				_pushrec1(S, byte, LoadX(), global_y());
			} VMBREAK;

			VMCASE(BYTECODE_SETGLOBAL) {
				vmove(&global_x(), LoadY());
				_pushrec1(S, byte, global_x(), LoadY());
			} VMBREAK;

			VMCASE(BYTECODE_RELOAD) {
				V x = LoadY();
				vmove(&LoadX(), x);
				_pushrec1(S, byte, LoadX(), x);
			} VMBREAK;

			VMCASE(BYTECODE_LOADCVAL) {
				ASSERT(WITHIN(BYTECODE_ARGY(byte), 0, frame.closuresize));
				vmove(&LoadX(), frame.closureenv[by.b_y]);
			} VMBREAK;

			VMCASE(BYTECODE_GETMETAFIELD)
			{
				V x = LoadY(), y = LoadZ();
				V o = _get_metafield(S, x, y);

				vmove(&LoadX(), o);

				_pushrec(S, byte, o, x, y);
			} VMBREAK;


			VMCASE(BYTECODE_SETINDEX)
			{
				V x = LoadY(), y = LoadZ();
				V o = LoadZ();

				Array array  = check_array(S, x);
				Index index  = check_index(S, y, heap_array_length(array));
				array[index] = o;

				_pushrec(S, byte, o, x, y);
			} VMBREAK;

			VMCASE(BYTECODE_GETINDEX)
			{
				V x = LoadY(), y = LoadZ();

				Array array = check_array(S, x);
				Index index = check_index(S, y, heap_array_length(array));

				V o = array[index];

				vmove(&LoadX(), o);

				_pushrec(S, byte, o, x, y);
			} VMBREAK;


			case BYTECODE_GETFIELD: {

				_get_field(S, &LoadX(), LoadY(), LoadZ());

			} break;

			case BYTECODE_SETFIELD: {
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


			// todo: replace with table intrinsics!
			// _table_arraylen
			// _table_arrayset
			// _table_arrayget


			VMCASE(BYTECODE_ENFORCE) {

				typerulecheck(S, LoadX(), by.b_y);

			} VMBREAK;

			case BYTECODE_CLOSURE: {
				ASSERT(WITHIN(BYTECODE_ARGY(byte), 0, heap_array_length(S->protos)));

				int proto_index = BYTECODE_ARGY(byte);

				BytecodeFunction proto = S->protos[proto_index];

				Closure closure;

				int size = sizeof(*closure) + sizeof(closure->captures[0]) * proto.ncaptures;

				closure = (Closure) gcalloc(S, GC_CLS, size);
				closure->proto_index = proto_index;
				closure->proto = proto;
				copy_values(closure->captures, &LoadX(), proto.ncaptures);

				to_cls(&LoadX(), closure);
			} break;


			case BYTECODE_TABLE: {

				elf_Table *tab = new_table(S);
				to_tab(&LoadX(), tab);

			} break;


			VMCASE(BYTECODE_GETLENGTH)
			{
				Array array  = check_array(S, LoadY());
				to_int(&LoadX(), heap_array_length(array));
			} VMBREAK;


			VMCASE(BYTECODE_ARRAYADD) {
				Value table = LoadX();
				Value value = LoadY();
				typecheck(S, table, ELF_TTABLE);
				_table_arrayadd(S, as_table(table), value);
			} VMBREAK;



			case BYTECODE_N2I:
			{
				to_int(&LoadX(), num_to_int(LoadY()));
			} break;

			case BYTECODE_I2N:
			{
				to_num(&LoadX(), int_to_num(LoadY()));
			} break;


			VMCASE(BYTECODE_BIT_NOT) {

				V x = LoadY();
				typecheck(S, x, ELF_TINTEGER);

				V o;
				to_int(&o, ~ as_int(x));

				vmove(&LoadX(), o);

				_pushrec1(S, byte, o, x);
			} VMBREAK;

			VMCASE(BYTECODE_EQ) {
				V x = LoadY(), y = LoadZ();
				v__eq(S, &LoadX(), x, y);
				_pushrec(S, byte, LoadX(), x, y);
			} VMBREAK;

			VMCASE(BYTECODE_NEQ) {
				V x = LoadY(), y = LoadZ();
				v__neq(S, &LoadX(), x, y);
				_pushrec(S, byte, LoadX(), x, y);
			} VMBREAK;

			VMCASE(BYTECODE_LT) {
				V x = LoadY(), y = LoadZ();
				v__lt(S, &LoadX(), x, y);
				_pushrec(S, byte, LoadX(), x, y);
			} VMBREAK;

			VMCASE(BYTECODE_LTEQ) {
				V x = LoadY(), y = LoadZ();
				v__lteq(S, &LoadX(), x, y);
				_pushrec(S, byte, LoadX(), x, y);
			} VMBREAK;

			VMCASE(BYTECODE_POW) {
				V x = LoadY(), y = LoadZ();
				v__pow(S, &LoadX(), x, y);
				_pushrec(S, byte, LoadX(), x, y);
			} VMBREAK;

			VMCASE(BYTECODE_MOD) {
				V x = LoadY(), y = LoadZ();
				v__mod(S, &LoadX(), x, y);
				_pushrec(S, byte, LoadX(), x, y);
			} VMBREAK;

			VMCASE(BYTECODE_MUL) {
				V x = LoadY(), y = LoadZ();
				v__mul(S, &LoadX(), x, y);
				_pushrec(S, byte, LoadX(), x, y);
			} VMBREAK;

			VMCASE(BYTECODE_DIV) {
				V x = LoadY(), y = LoadZ();
				v__div(S, &LoadX(), x, y);
				_pushrec(S, byte, LoadX(), x, y);
			} VMBREAK;

			VMCASE(BYTECODE_ADD) {
				V x = LoadY(), y = LoadZ();
				v__add(S, &LoadX(), x, y);
				_pushrec(S, byte, LoadX(), x, y);
			} VMBREAK;

			VMCASE(BYTECODE_SUB) {
				V x = LoadY(), y = LoadZ();
				v__sub(S, &LoadX(), x, y);
				_pushrec(S, byte, LoadX(), x, y);
			} VMBREAK;

			VMCASE(BYTECODE_BIT_XOR) {
				V x = LoadY(), y = LoadZ();
				v__eor(S, &LoadX(), x, y);
				_pushrec(S, byte, LoadX(), x, y);
			} VMBREAK;

			VMCASE(BYTECODE_BIT_SHL) {
				V x = LoadY(), y = LoadZ();
				v__shl(S, &LoadX(), x, y);
				_pushrec(S, byte, LoadX(), x, y);
			} VMBREAK;

			VMCASE(BYTECODE_BIT_SHR) {
				V x = LoadY(), y = LoadZ();
				v__shr(S, &LoadX(), x, y);
				_pushrec(S, byte, LoadX(), x, y);
			} VMBREAK;

			VMCASE(BYTECODE_BIT_AND) {
				V x = LoadY(), y = LoadZ();
				v__and(S, &LoadX(), x, y);
				_pushrec(S, byte, LoadX(), x, y);
			} VMBREAK;

			VMCASE(BYTECODE_BIT_OR) {
				V x = LoadY(), y = LoadZ();
				v__ior(S, &LoadX(), x, y);
				_pushrec(S, byte, LoadX(), x, y);
			} VMBREAK;


			default: {
				reporterrorf(S, minstr, "unsupported instruction: %s", Static_StrFromBytecode[BYTECODE_OP(byte)]);
			} break;
		}
	}

	esc:
	return nrets;
}






