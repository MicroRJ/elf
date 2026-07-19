//
// See Copyright Notice In elf.h
//

static inline void prepare_closure_stack_frame(elf_State *state, StackFrame *frame, elf_Closure *closure, int nargs, int nrets)
{
	ASSERT(nargs >= 0);
	ASSERT(nrets >= 0);

	BcFunction function = closure->function;
	u32 argument_count = (u32)nargs;
	u32 frame_size = MAX(argument_count, function.stack_size);

	elf_Value *frame_base = state->stack_ptr - argument_count;
	elf_Value *reference = frame_base;

	if (function.variadic && argument_count > function.arity)
	{
		reference += argument_count;
		frame_size += function.arity;
		value_copy_many(reference, frame_base, function.arity);
	}

	u32 copied_arg_count = MIN(function.arity, argument_count);
	u32 clear_start = (u32)(reference - frame_base) + copied_arg_count;
	ASSERT(clear_start <= frame_size);
	value_zero_many(frame_base + clear_start, frame_size - clear_start);

	frame->framesize = frame_size;
	frame->framebase = frame_base;
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

	elf_Value *stack_pointer = frame_base + frame_size;
	ASSERT(stack_pointer >= state->stack_ptr);
	state->stack_ptr = stack_pointer;
}

static u32 call_native_function(elf_State *state, elf_Function function, u32 nargs, u32 nrets)
{
	elf_Value *frame_base = state->stack_ptr - nargs;
	u32 frame_size = nargs;

	state->frame.framebase = frame_base;
	state->frame.reference = frame_base;
	state->frame.framesize = frame_size;
	state->frame.nargs = nargs;
	state->frame.nrets = nrets;

	// Native calls run directly above their arguments, so framebase + framesize
	// is exactly the current stack pointer. There is no frame slack to zero here.
	ASSERT(frame_base + frame_size == state->stack_ptr);

	i32 returned_count = function(state, nargs, nrets);
	i32 pushed_count = (i32)(state->stack_ptr - frame_base - frame_size);

	if (returned_count < 0 || pushed_count < returned_count)
	{
		report_runtime_error(state, RUNTIME_ERROR_GENERIC, NO_BYTE
		,	"invalid number of returns from function: %i, however the function pushed: %i"
		,	returned_count, pushed_count);
	}

	i32 copied_return_count = MIN(returned_count, (i32)nrets);

	if (returned_count < nrets) {
		value_zero_many(frame_base - 1 + returned_count, nrets - returned_count);
	}

	value_copy_many(frame_base - 1, state->stack_ptr - returned_count, copied_return_count);
	state->stack_ptr = frame_base - 1 + nrets;
	return copied_return_count;
}

int call_bytecode_closure(elf_State *state, elf_Closure *closure, int nargs, int nrets)
{
	StackFrame frame;
	prepare_closure_stack_frame(state, &frame, closure, nargs, nrets);
	int result = 0;
	PROF_BLOCK("vm.run_bytecode_frame")
	{
		result = run_bytecode_frame(state, frame);
	}
	return result;
}

u32 elf_do_tail_call(elf_State *state, u32 nargs, u32 nrets)
{
	ASSERT(state->stack_ptr - nargs - 1 >= state->stack);

	u32 frame_index = state->frame_index;
	elf_Value value = *(state->stack_ptr - nargs - 1);

	if (value_is_closure(value))
	{
		nrets = call_bytecode_closure(state, value_as_closure(value), nargs, nrets);
	}
	else if (value_is_function(value))
	{
		nrets = call_native_function(state, value_as_function(value), nargs, nrets);
	}
	else
	{
		report_runtime_error(state, RUNTIME_ERROR_EXPECTS_CALLABLE, NO_BYTE, "'%s' cannot be called", value_type_name(value.type));
	}

	ASSERT(state->frame_index == frame_index);
	return nrets;
}

static inline void push_stack_frame(elf_State *state)
{
	ASSERT(state->frame_index < state->frame_stack_size);
	state->frame_stack[state->frame_index++] = state->frame;
}

static inline void pop_stack_frame(elf_State *state)
{
	ASSERT(state->frame_index > 0);
	state->frame = state->frame_stack[--state->frame_index];
}

u32 elf_call(elf_State *state, u32 nargs, u32 nrets)
{
	if (nargs < 1) {
		report_runtime_error(state, RUNTIME_ERROR_INVALID_ARGUMENT_COUNT, NO_BYTE, "invalid number of arguments, expected at least one");
	}

	push_stack_frame(state);
	nrets = elf_do_tail_call(state, nargs, nrets);
	pop_stack_frame(state);
	return nrets;
}
