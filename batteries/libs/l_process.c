//
// Optional process execution and process-local control.
//

static void read_process_stream(Platform_Process *process, elf_Buffer *buffer, Platform_Process_Read_Result (*read_stream)(Platform_Process *, void *, U64), U32 *os_error)
{
	for (;;)
	{
		if (!buffer_reserve(buffer, 64 * 1024)) return;
		Platform_Process_Read_Result read = read_stream(process, buffer->data + buffer->size, 64 * 1024);
		if (read.error) {
			if (!*os_error) *os_error = read.os_error;
			return;
		}
		buffer->size += (size_t)read.size;
		if (read.size == 0) return;
	}
}

ELF_FUNCTION(lib_process_run)
{
	elf_State *state = S;
	elf_Buffer standard_output = {0};
	elf_Buffer standard_error = {0};
	Platform_Process_Options options = {
		.capture_standard_output = PLATFORM_TRUE,
		.capture_standard_error = PLATFORM_TRUE,
		.hide_window = PLATFORM_TRUE,
	};
	Platform_Process_Start_Result started = platform_start_process(lib_load_cstr(state, 1), options);
	Platform_Process_Wait_Result waited = {0};
	U32 os_error = started.os_error;
	if (platform_process_is_valid(started.process)) {
		for (;;) {
			read_process_stream(&started.process, &standard_output, platform_read_process_output, &os_error);
			read_process_stream(&started.process, &standard_error, platform_read_process_error, &os_error);
			waited = platform_wait_process(started.process, 1);
			if (waited.status != PLATFORM_PROCESS_WAIT_TIMED_OUT) break;
		}
		read_process_stream(&started.process, &standard_output, platform_read_process_output, &os_error);
		read_process_stream(&started.process, &standard_error, platform_read_process_error, &os_error);
		if (!os_error) os_error = waited.os_error;
	}
	B32 process_started = platform_process_is_valid(started.process);
	B32 completed = process_started && waited.status == PLATFORM_PROCESS_WAIT_COMPLETED;

	elf_new_table(state);
	elf_i32 result = elf_abs_index(state, -1);
	lib_set_integer_field(state, result, "success", completed && waited.exit_code == 0);
	lib_set_integer_field(state, result, "started", process_started);
	lib_set_integer_field(state, result, "exit_code", completed ? (i32)waited.exit_code : -1);
	lib_set_integer_field(state, result, "error_code", os_error);
	lib_set_string_field(state, result, "stdout", (char *)standard_output.data, (u32)standard_output.size);
	lib_set_string_field(state, result, "stderr", (char *)standard_error.data, (u32)standard_error.size);

	if (os_error)
	{
		char error[1024] = {0};
		Platform_String_Result message = platform_error_message(os_error, error, sizeof(error));
		if (message.error) error[0] = 0;
		lib_set_string_field(state, result, "error", error, (u32)strlen(error));
	}
	else {
		lib_set_nil_field(state, result, "error");
	}

	buffer_destroy(&standard_output);
	buffer_destroy(&standard_error);
	platform_close_process(&started.process);
	return 1;
}

ELF_FUNCTION(lib_process_id)
{
	elf_push_int(S, (elf_i64)platform_current_process_id());
	return 1;
}

ELF_FUNCTION(lib_process_exit)
{
	platform_exit_process((I32)lib_load_integer(S, 1));
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
