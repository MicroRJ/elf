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

#include "arena.c"

#include "runtime_error.c"

#include "l_math.c"
#include "l_core.c"
#include "l_sys.c"
#include "l_table.c"
#include "l_string.c"
#include "l_random.c"
#include "l_sockets.c"









int call_closure(elf_State *S, Closure closure, int nargs, int nrets);










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


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Note that the stack has to be cleared ... Because otherwise the GC trips ...
static inline void set_stack_pointer(elf_State *state, Value *pointer)
{
	if (pointer > state->stack_ptr) {
		zero_values(state->stack_ptr, pointer - state->stack_ptr);
	}

	state->stack_ptr = pointer;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ELF_PUBLIC elf_State *elf_create_state()
{
	elf_Arena arena = elf_create_arena(0);
	elf_Arena scratch_arena = elf_create_arena(0);

	elf_State *state = elf_arena_push_zero(& arena, sizeof(*state));

	state->arena_ = arena;
	state->scratch_arena_ = scratch_arena;

	state->arena = & state->arena_;
	state->scratch_arena = & state->scratch_arena_;

	u32 frame_stack_size = 4096;
	state->frame_stack_size = frame_stack_size;
	state->frame_stack = elf_arena_push_zero(state->arena, sizeof(* state->frame_stack) * frame_stack_size);

	u32 stack_size = 4096;
	state->stack_size = stack_size;
	state->stack = elf_arena_push_zero(state->arena, sizeof(*state->stack) * stack_size);
	state->stack_ptr = state->stack;

	state->record_max = 1024;
	state->record_min = 0;
	state->record = elf_arena_push_zero(state->arena, sizeof(*state->record) * state->record_max);

	// Todo, just allocate this dynamically ...
	u32 max_references = 1 << 10 << 2;

	state->collector_state.memory_thresh = GC_MEM_THRESHOLD_MIN;
	state->collector_state.counter_thresh = GC_OBJ_THRESHOLD_MIN;

	state->collector_state.references1 = elf_arena_push_zero(state->arena, sizeof(*state->collector_state.references1) * max_references);
	state->collector_state.references2 = elf_arena_push_zero(state->arena, sizeof(*state->collector_state.references1) * max_references);

	// this is to match the frame setup of a typical function ...
	push_nil(state);

	state->frame.framebase = state->stack;
	state->frame.framesize = 16;

	state->metatables.string = push_new_table(state);
	install(state, 0, l_string, COUNTOF(l_string));

	state->metatables.table = push_new_table(state);
	install(state, 0, l_table, COUNTOF(l_table));

	state->metatables.buffer = push_new_table(state);
	install(state, 0, l_buffer, COUNTOF(l_buffer));

	// state->strings = push_new_table(state);

	// todo: builtin intrinsics
	static elf_Binding lib_base[] = {
		{"ntoi", l_core_ntoi},
		{"iton", l_core_iton},
	};

	state->globals = push_new_table(state);
	install(state,"elf.sockets",  l_sockets  , COUNTOF(l_sockets));
	install(state,           0 ,  lib_base   , COUNTOF(lib_base));
	install(state,           0 ,  lib_math   , COUNTOF(lib_math));
	install(state, "elf"       ,  l_core     , COUNTOF(l_core));
	install(state, "elf"       ,  lib_random , COUNTOF(lib_random));
	install(state, "elf"       ,  l_sys      , COUNTOF(l_sys));


	ASSERT(stack2index(state) < state->frame.framesize);

	set_stack_pointer(state, state->frame.framebase + state->frame.framesize);

	return state;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static inline u32 call_function(elf_State *S, elf_Function function, u32 nargs, u32 nrets)
{
	Value *framebase = S->stack_ptr - nargs;
	Value *reference = framebase;

	u32 framesize = nargs;

	S->frame.framebase = framebase;
	S->frame.reference = reference;
	S->frame.framesize = framesize;
	S->frame.nargs = nargs;
	S->frame.nrets = nrets;


	set_stack_pointer(S, framebase + framesize);



	u32 ncallrets = function(S, nargs, nrets);

	u32 npushrets = S->stack_ptr - framebase - framesize;


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

u32 elf_do_tail_call(elf_State *state, u32 nargs, u32 nrets)
{
	ASSERT(state->stack_ptr - nargs - 1 >= state->stack);

	u32 frame_index = state->frame_index;

	Value value = * (state->stack_ptr - nargs - 1);

	if (is_closure(value))
	{
		nrets = call_closure(state, closure_from_value(value), nargs, nrets);
	}
	else if(is_function(value))
	{
		nrets = call_function(state, function_from_value(value), nargs, nrets);
	}
	else
	{
		report_runtime_error(state, RUNTIME_ERROR_EXPECTS_CALLABLE, "'%s' cannot be called", tag2s[value.tag]);
	}

	ASSERT(state->frame_index == frame_index);

	return nrets;
}

static inline void push_stack_frame(elf_State *state)
{
	ASSERT(state->frame_index < state->frame_stack_size);
	state->frame_stack[state->frame_index ++] = state->frame;
}

static inline void pop_stack_frame(elf_State *state)
{
	ASSERT(state->frame_index > 0);
	state->frame = state->frame_stack[-- state->frame_index];
}

u32 elf_call(elf_State *state, u32 nargs, u32 nrets)
{
	if (nargs < 1) {
		report_runtime_error(state, RUNTIME_ERROR_INVALID_ARGUMENT_COUNT, "invalid number of arguments, expected at least one");
	}

	push_stack_frame(state);

	nrets = elf_do_tail_call(state, nargs, nrets);

	pop_stack_frame(state);
	return nrets;
}

static inline Array check_array(elf_State *S, V v) {
	typecheck(S, v, ELF_VALUE_TYPE_TABLE);
	return table_from_value(v)->array;
}

// todo: onlycheck index type, _table_arrayset and _table_arrayget
// already check the index
static inline Index check_index(elf_State *S, V v, Index l) {
	typecheck(S, v, ELF_VALUE_TYPE_INTEGER);

	Index index = as_int(v);

	if (index < 0) {
		index += l;
	}

	if (index < 0 || index >= l) {
		reporterror(S, -1, "index out of bounds");
	}

	return index;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define VMCASE(OPCODE) case OPCODE:
#define VMBREAK break


#define global_x() (globals->array[by.b_x])
#define global_y() (globals->array[by.b_y])
#define LoadX() (frame.reference[by.b_x])
#define LoadY() (frame.reference[by.b_y])
#define LoadZ() (frame.reference[by.b_z])


static inline void init_stack_frame_for_closure(elf_State *S, StackFrame *frame, elf_Closure *closure, int nargs, int nrets)
{
	BytecodeFunction function = closure->function;

	// todo: do not grow frame size unless we care about vargs
	u32 framesize = nargs < function.stack_size ? function.stack_size : nargs;


	// todo: don't do this here, because when we call init_stack_frame_for_closure
	// from the closure, we have to increment the stackpointer to account
	// for this, when we already know this information
	// this should be done in call_closure which is outward facing!
	V *framebase = S->stack_ptr - nargs;


	V *reference = framebase;

	// Do we care about excess args? And do we have any?
	// todo: unlikely
	if (function.variadic && nargs > function.arity) {
		reference += nargs;
		framesize += function.arity;
		copy_values(reference, framebase, function.arity);
	}


	// todo: this is redundant, we already zero when we set the stack pointer!

	// prevent the collector from tripping on possibly garbage values
	// clear the remaining stack space
	zero_values(reference + MIN(function.arity, nargs), framesize - MIN(function.arity, nargs) - (reference - framebase));


	frame->framesize = framesize;
	frame->framebase = framebase;
	frame->reference = reference;
	frame->nargs = nargs;
	frame->nrets = nrets;
	frame->arity = function.arity;
	frame->variadic = function.variadic;
	frame->bytes = function.offset;
	frame->bytec = function.length;
	frame->closureenv = closure->captures;
	frame->closuresize = function.captures;
	frame->nextinstr = 0;

	// todo: this will zero too!
	set_stack_pointer(S, framebase + framesize);
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

#if 0
static void _tracerecords(elf_State *S, u32 x) {
	u32 mask = S->record_max - 1;
	for (u32 i = S->record_min - 1; i >= 0; -- i) {
		Record *rec = & S->record[i & mask];
		Bytecode b = rec->b;

		switch (b.b_type) {
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
					printf("%s(%i, %i, %i)\n", Static_StrFromBytecode[b.b_type], b.b_x, b.b_y, b.b_z);
				}
			} break;
		}
	}
}
#endif

// todo: outward facing root call should not write results to the function,
// this is only bytecode stuff!
int call_closure(elf_State *S, Closure closure, int nargs, int nrets)
{
	StackFrame frame;
	init_stack_frame_for_closure(S, &frame, closure, nargs, nrets);

	Table *globals = S->globals;
	Value *frameroot = frame.framebase;

	int subframecounter = 0;

	V xx,yy,zz;
	while (frame.nextinstr < frame.bytec)
	{
		BCPos instr = frame.nextinstr ++;

		BCPos minstr = frame.bytes + instr;
		S->byte = minstr;

		Bytecode byte = S->bytebuf[minstr];

		printf("%s(%i, %i, %i)\n", Static_StrFromBytecode[byte.b_type], byte.b_x, byte.b_y, byte.b_z);

		#define by byte


		switch (BYTECODE_TYPE(byte)) {

			VMCASE(BYTECODE_NOP)
			{
			}
			VMBREAK;

			VMCASE(BYTECODE_CALL)
			{
				// put stack pointer right above all the arguments
				// note how this will lead to the sub-function having
				// a framebase relative to our reference, so the results
				// get placed where we want them...
				S->stack_ptr = frame.reference + by.b_x + by.b_y + 1;

				xx = LoadX();

				// Note, push frame regardless of function type
				S->frame_stack[S->frame_index ++] = frame;
				if (is_closure(xx))
				{
					init_stack_frame_for_closure(S, &frame, closure_from_value(xx), by.b_y, by.b_z);

					subframecounter ++;
				}
				else if (is_function(xx))
				{
					call_function(S, function_from_value(xx), by.b_y, by.b_z);

					// note how we don't store nor restore S->frame, so
					// from this point onwards, S->frame will no longer
					// reflect the state of our local frame, we need to
					// ensure that when we exit the function, no-one assumes
					// that S->frame is the frame of call_closure
					S->frame_index --;

					// restore stack pointer
					set_stack_pointer(S, frame.framebase + frame.framesize);
				}
				else {
					reporterrorf(S, -1, "cannot call '%s'", tag2s[xx.tag]);
				}

				ASSERT(S->stack_ptr == frame.framebase + frame.framesize);
			} VMBREAK;

			VMCASE(BYTECODE_RETURN) {
				nrets = MIN(by.b_y, frame.nrets);

				// returns are placed right on the function
				copy_values(frame.framebase - 1, frame.reference + by.b_x, nrets);

				ASSERT(S->frame_index > 0);
				if (subframecounter) {
					frame = S->frame_stack[-- S->frame_index];

					set_stack_pointer(S, frame.framebase + frame.framesize);

					subframecounter --;
				}
				else {
					ASSERT(frame.framebase == frameroot);
					S->stack_ptr = frame.framebase - 1 + nrets;
					goto esc;
				}
			} VMBREAK;

			case BYTECODE_JUMP: {

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


			VMCASE(BYTECODE_GETGLOBAL) {
				vmove(&LoadX(), global_y());
				_pushrec1(S, byte, LoadX(), global_y());
			} VMBREAK;

			VMCASE(BYTECODE_SETGLOBAL)
			{
				vmove(&global_x(), LoadY());
			}
			VMBREAK;

			VMCASE(BYTECODE_RELOAD) {
				V x = LoadY();
				vmove(&LoadX(), x);
				_pushrec1(S, byte, LoadX(), x);
			} VMBREAK;

			VMCASE(BYTECODE_LOADCVAL)
			{
				ASSERT(WITHIN(BYTECODE_ARGY(byte), 0, frame.closuresize));
				vmove(&LoadX(), frame.closureenv[by.b_y]);
			}
			VMBREAK;

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

			VMCASE(BYTECODE_SETFIELD)
			{
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
			}
			VMBREAK;

			VMCASE(BYTECODE_ENFORCE)
			{
				typerulecheck(S, LoadX(), by.b_y);
			}
			VMBREAK;

			VMCASE(BYTECODE_CLOSURE)
			{
				ASSERT(WITHIN(BYTECODE_ARGY(byte), 0, heap_array_length(S->protos)));

				u32 proto_index = BYTECODE_ARGY(byte);

				BytecodeFunction function = S->protos[proto_index];

				Closure closure;

				u32 size = sizeof(*closure) + sizeof(closure->captures[0]) * function.captures;

				closure = (Closure) collector_alloc(S, GC_CLOSURE, size);
				closure->function = function;
				copy_values(closure->captures, &LoadX(), function.captures);

				to_cls(&LoadX(), closure);
			}
			VMBREAK;

			VMCASE(BYTECODE_TABLE)
			{
				Table *tab = new_table(S);
				to_tab(&LoadX(), tab);
			}
			VMBREAK;


			VMCASE(BYTECODE_GETLENGTH)
			{
				Array array  = check_array(S, LoadY());
				to_int(&LoadX(), heap_array_length(array));
			}
			VMBREAK;


			VMCASE(BYTECODE_ARRAYADD)
			{
				Value table = LoadX();
				Value value = LoadY();
				typecheck(S, table, ELF_VALUE_TYPE_TABLE);
				_table_arrayadd(S, table_from_value(table), value);
			}
			VMBREAK;

			VMCASE(BYTECODE_N2I)
			{
				to_int(&LoadX(), num_to_int(LoadY()));
			}
			VMBREAK;

			VMCASE(BYTECODE_I2N)
			{
				to_num(&LoadX(), int_to_num(LoadY()));
			}
			VMBREAK;

			VMCASE(BYTECODE_BIT_NOT)
			{
				V x = LoadY();
				typecheck(S, x, ELF_VALUE_TYPE_INTEGER);

				V o;
				to_int(&o, ~ as_int(x));

				vmove(&LoadX(), o);
			}
			VMBREAK;

			VMCASE(BYTECODE_EQ)
			{
				V x = LoadY(), y = LoadZ();
				v__eq(S, &LoadX(), x, y);
			}
			VMBREAK;

			VMCASE(BYTECODE_NEQ)
			{
				V x = LoadY(), y = LoadZ();
				v__neq(S, &LoadX(), x, y);
			}
			VMBREAK;

			VMCASE(BYTECODE_LT)
			{
				V x = LoadY(), y = LoadZ();
				v__lt(S, &LoadX(), x, y);
			}
			VMBREAK;

			VMCASE(BYTECODE_LTEQ)
			{
				V x = LoadY(), y = LoadZ();
				v__lteq(S, &LoadX(), x, y);
			}
			VMBREAK;

			VMCASE(BYTECODE_POW)
			{
				V x = LoadY(), y = LoadZ();
				v__pow(S, &LoadX(), x, y);
			}
			VMBREAK;

			VMCASE(BYTECODE_MOD)
			{
				V x = LoadY(), y = LoadZ();
				v__mod(S, &LoadX(), x, y);
			}
			VMBREAK;

			VMCASE(BYTECODE_MUL)
			{
				V x = LoadY(), y = LoadZ();
				v__mul(S, &LoadX(), x, y);
			}
			VMBREAK;

			VMCASE(BYTECODE_DIV)
			{
				V x = LoadY(), y = LoadZ();
				v__div(S, &LoadX(), x, y);
			}
			VMBREAK;

			VMCASE(BYTECODE_ADD)
			{
				V x = LoadY(), y = LoadZ();
				v__add(S, &LoadX(), x, y);
			}
			VMBREAK;

			VMCASE(BYTECODE_SUB)
			{
				V x = LoadY(), y = LoadZ();
				v__sub(S, &LoadX(), x, y);
			}
			VMBREAK;

			VMCASE(BYTECODE_BIT_XOR)
			{
				V x = LoadY(), y = LoadZ();
				v__eor(S, &LoadX(), x, y);
			}
			VMBREAK;

			VMCASE(BYTECODE_BIT_SHL)
			{
				V x = LoadY(), y = LoadZ();
				v__shl(S, &LoadX(), x, y);
			}
			VMBREAK;

			VMCASE(BYTECODE_BIT_SHR)
			{
				V x = LoadY(), y = LoadZ();
				v__shr(S, &LoadX(), x, y);
			}
			VMBREAK;

			VMCASE(BYTECODE_BIT_AND)
			{
				Value x = LoadY(), y = LoadZ();
				v__and(S, &LoadX(), x, y);
			}
			VMBREAK;

			VMCASE(BYTECODE_BIT_OR)
			{
				Value x = LoadY(), y = LoadZ();
				v__ior(S, &LoadX(), x, y);
			}
			VMBREAK;


			default:
			{
				report_runtime_error(S, RUNTIME_ERROR_UNKNOWN_BYTECODE, "'%s' unknown bytecode", Static_StrFromBytecode[BYTECODE_TYPE(byte)]);
			}
			break;
		}
	}

	esc:
	return nrets;
}






