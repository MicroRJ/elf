//
// See Copyright Notice In elf.h
//

//
// Introspection for the bytecode frame that called a core function.
//

static StackFrame *core_caller_frame(elf_State *state)
{
	if (state->frame_index == 0)
	{
		elf_report_runtime_error(state, RUNTIME_ERROR_GENERIC, -1,
			"caller introspection requires a bytecode caller");
	}
	return &state->frame_stack[state->frame_index - 1];
}

ELF_FUNCTION(l_core_nvargs)
{
	lib_check_arg_count(S, "nvargs", nargs, 0, 0);
	StackFrame *caller = core_caller_frame(S);
	i64 count = caller->variadic ? caller->nargs - caller->arity : 0;
	push_value(S, value_from_integer(MAX(count, 0)));
	return 1;
}

ELF_FUNCTION(l_core_varg)
{
	lib_check_arg_count(S, "varg", nargs, 1, 1);
	StackFrame *caller = core_caller_frame(S);
	i64 index = lib_load_integer(S, 1);
	i64 count = caller->variadic ? caller->nargs - caller->arity : 0;

	if (index < 0 || index >= count) push_value(S, value_nil());
	else push_value(S, caller->framebase[caller->arity + index]);
	return 1;
}

ELF_FUNCTION(l_core_nrets)
{
	lib_check_arg_count(S, "nrets", nargs, 0, 0);
	push_value(S, value_from_integer(core_caller_frame(S)->nrets));
	return 1;
}

ELF_FUNCTION(l_core_nargs)
{
	lib_check_arg_count(S, "nargs", nargs, 0, 0);
	push_value(S, value_from_integer(core_caller_frame(S)->nargs));
	return 1;
}

ELF_FUNCTION(l_core_arg)
{
	lib_check_arg_count(S, "arg", nargs, 1, 1);
	StackFrame *caller = core_caller_frame(S);
	i64 index = lib_load_integer(S, 1);
	if (index < 0 || index >= caller->nargs) push_value(S, value_nil());
	else push_value(S, caller->framebase[index]);
	return 1;
}
