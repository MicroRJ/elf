//
// Process execution and process-local control.
//

ELF_FUNCTION(lib_process_run)
{
	elf_State *state = S;
	Arena standard_output = create_arena(0);
	Arena standard_error = create_arena(0);
	Sys_Process_Result process = sys_run_process(lib_load_cstr(state, 1),
		&standard_output, &standard_error);

	elf_Table *result = elf_push_new_table(state);
	lib_set_integer_field(state, result, "success", process.started && process.exit_code == 0);
	lib_set_integer_field(state, result, "started", process.started);
	lib_set_integer_field(state, result, "exit_code", process.exit_code);
	lib_set_integer_field(state, result, "error_code", process.error_code);
	lib_set_string_field(state, result, "stdout", (char *)standard_output.data,
		(u32)standard_output.in_use);
	lib_set_string_field(state, result, "stderr", (char *)standard_error.data,
		(u32)standard_error.in_use);

	if (process.error_code)
	{
		char error[1024] = {0};
		sys_get_error_msg(process.error_code, error, sizeof(error));
		lib_set_string_field(state, result, "error", error, (u32)strlen(error));
	}
	else {
		lib_set_nil_field(state, result, "error");
	}

	destroy_arena(&standard_output);
	destroy_arena(&standard_error);
	return 1;
}

ELF_FUNCTION(lib_process_id)
{
	push_value(S, value_from_integer(sys_get_this_process_id()));
	return 1;
}

ELF_FUNCTION(lib_process_exit)
{
	sys_exit_this_process((i32)lib_load_integer(S, 1));
	return 0;
}

static const elf_Binding l_process[] = {
	{"run",  lib_process_run},
	{"id",   lib_process_id},
	{"exit", lib_process_exit},
};

static elf_Table *elf_lib_process(elf_State *state)
{
	return new_binding_table(state, l_process, ARRAY_COUNT(l_process));
}
