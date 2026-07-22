//
// Optional process execution and process-local control.
//

ELF_FUNCTION(lib_process_run)
{
	elf_State *state = S;
	elf_Buffer standard_output = {0};
	elf_Buffer standard_error = {0};
	elf_PlatformProcessResult process = elf_platform_run_process(lib_load_cstr(state, 1),
		&standard_output, &standard_error);

	elf_new_table(state);
	elf_i32 result = elf_abs_index(state, -1);
	lib_set_integer_field(state, result, "success", process.started && process.exit_code == 0);
	lib_set_integer_field(state, result, "started", process.started);
	lib_set_integer_field(state, result, "exit_code", process.exit_code);
	lib_set_integer_field(state, result, "error_code", process.error_code);
	lib_set_string_field(state, result, "stdout", (char *)standard_output.data,
		(u32)standard_output.size);
	lib_set_string_field(state, result, "stderr", (char *)standard_error.data,
		(u32)standard_error.size);

	if (process.error_code)
	{
		char error[1024] = {0};
		elf_platform_error_message(process.error_code, error, sizeof(error));
		lib_set_string_field(state, result, "error", error, (u32)strlen(error));
	}
	else {
		lib_set_nil_field(state, result, "error");
	}

	buffer_destroy(&standard_output);
	buffer_destroy(&standard_error);
	return 1;
}

ELF_FUNCTION(lib_process_id)
{
	elf_push_int(S, elf_platform_process_id());
	return 1;
}

ELF_FUNCTION(lib_process_exit)
{
	battery_exit_process((i32)lib_load_integer(S, 1));
	return 0;
}

static const Battery_Binding l_process[] = {
	{"run",  lib_process_run},
	{"id",   lib_process_id},
	{"exit", lib_process_exit},
};

static void elf_lib_process(elf_State *state)
{
	new_binding_table(state, l_process, battery_array_count(sizeof(l_process), sizeof(l_process[0])));
}
