//
// Optional process execution and process-local control.
//

static void read_process_stream(dy_Process *process, dy_Process_Stream stream,
	elf_Buffer *buffer, u32 *os_error)
{
	for (;;)
	{
		if (!buffer_reserve(buffer, 64 * 1024)) return;
		dy_u64 size;
		dy_b32 end_of_stream;
		dy_Result read = dy_read_process(process, stream, buffer->data + buffer->size,
			64 * 1024, &size, &end_of_stream);
		if (read.error) {
			if (!*os_error) *os_error = read.os_error;
			return;
		}
		buffer->size += (size_t)size;
		if (size == 0 || end_of_stream) return;
	}
}

ELF_FUNCTION(lib_process_run)
{
	elf_State *state = S;
	elf_Buffer standard_output = {0};
	elf_Buffer standard_error = {0};
	dy_Process_Options options = {
		.capture_output = 1,
		.capture_error = 1,
		.hide_window = 1,
	};
	dy_Process process;
	dy_Result started = dy_start_process(dy_string_from_cstring(lib_load_cstr(state, 1)), options, &process);
	dy_b32 completed = 0;
	dy_u32 exit_code = 0;
	u32 os_error = started.os_error;
	if (dy_process_is_valid(process)) {
		for (;;) {
			read_process_stream(&process, DY_PROCESS_OUTPUT, &standard_output, &os_error);
			read_process_stream(&process, DY_PROCESS_ERROR, &standard_error, &os_error);
			dy_Result waited = dy_wait_process(process, 1, &completed, &exit_code);
			if (waited.error) {
				if (!os_error) os_error = waited.os_error;
				break;
			}
			if (completed) break;
		}
		read_process_stream(&process, DY_PROCESS_OUTPUT, &standard_output, &os_error);
		read_process_stream(&process, DY_PROCESS_ERROR, &standard_error, &os_error);
	}
	b32 process_started = dy_process_is_valid(process);

	elf_new_table(state);
	elf_i32 result = elf_abs_index(state, -1);
	lib_set_integer_field(state, result, "success", completed && exit_code == 0);
	lib_set_integer_field(state, result, "started", process_started);
	lib_set_integer_field(state, result, "exit_code", completed ? (i32)exit_code : -1);
	lib_set_integer_field(state, result, "error_code", os_error);
	lib_set_string_field(state, result, "stdout", (char *)standard_output.data, (u32)standard_output.size);
	lib_set_string_field(state, result, "stderr", (char *)standard_error.data, (u32)standard_error.size);

	if (os_error)
	{
		dy_Scratch scratch = dy_begin_scratch();
		dy_String error;
		dy_Result message = dy_error_message(scratch.arena, os_error, &error);
		lib_set_string_field(state, result, "error", message.error ? "" : error.data,
			message.error ? 0 : (u32)error.size);
		dy_end_scratch(scratch);
	}
	else {
		lib_set_nil_field(state, result, "error");
	}

	buffer_destroy(&standard_output);
	buffer_destroy(&standard_error);
	dy_close_process(&process);
	return 1;
}

ELF_FUNCTION(lib_process_id)
{
	elf_push_int(S, (elf_i64)dy_current_process_id());
	return 1;
}

ELF_FUNCTION(lib_process_exit)
{
	dy_exit_process((dy_i32)lib_load_integer(S, 1));
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
