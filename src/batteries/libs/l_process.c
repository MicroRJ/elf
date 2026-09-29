//
// Optional process execution and process-local control.
//

static void read_process_stream(day_Process *process, day_Process_Stream stream,
	elf_Buffer *buffer, u32 *os_error)
{
	for (;;)
	{
		if (!buffer_reserve(buffer, 64 * 1024)) return;
		day_u64 size;
		day_b32 end_of_stream;
		day_Result read = day_read_process(process, stream, buffer->data + buffer->size,
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
	day_Process_Options options = {
		.capture_output = 1,
		.capture_error = 1,
		.hide_window = 1,
	};
	day_Process process;
	day_Result started = day_start_process(day_string_from_cstring(lib_load_cstr(state, 1)), options, &process);
	day_b32 completed = 0;
	day_u32 exit_code = 0;
	u32 os_error = started.os_error;
	if (day_process_is_valid(process)) {
		for (;;) {
			read_process_stream(&process, DAY_PROCESS_OUTPUT, &standard_output, &os_error);
			read_process_stream(&process, DAY_PROCESS_ERROR, &standard_error, &os_error);
			day_Result waited = day_wait_process(process, 1, &completed, &exit_code);
			if (waited.error) {
				if (!os_error) os_error = waited.os_error;
				break;
			}
			if (completed) break;
		}
		read_process_stream(&process, DAY_PROCESS_OUTPUT, &standard_output, &os_error);
		read_process_stream(&process, DAY_PROCESS_ERROR, &standard_error, &os_error);
	}
	b32 process_started = day_process_is_valid(process);

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
		day_Scratch scratch = day_begin_scratch();
		day_String error;
		day_Result message = day_error_message(scratch.arena, os_error, &error);
		lib_set_string_field(state, result, "error", message.error ? "" : error.data,
			message.error ? 0 : (u32)error.size);
		day_end_scratch(scratch);
	}
	else {
		lib_set_nil_field(state, result, "error");
	}

	buffer_destroy(&standard_output);
	buffer_destroy(&standard_error);
	day_close_process(&process);
	return 1;
}

ELF_FUNCTION(lib_process_id)
{
	elf_push_int(S, (elf_i64)day_current_process_id());
	return 1;
}

ELF_FUNCTION(lib_process_exit)
{
	day_exit_process((day_i32)lib_load_integer(S, 1));
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
