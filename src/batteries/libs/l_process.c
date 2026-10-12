//
// Processes backed by dy-core.
//

typedef enum Battery_Process_State
{
	BATTERY_PROCESS_RUNNING,
	BATTERY_PROCESS_EXITED,
	BATTERY_PROCESS_FAILURE,
}
Battery_Process_State;

typedef struct Battery_Process
{
	dy_Process process;
}
Battery_Process;

static void battery_process_destroy(elf_State *state, void *data)
{
	(void)state;
	Battery_Process *process = data;
	if (dy_process_is_valid(process->process))
	{
		dy_process_close(&process->process);
	}
}

static const elf_UserType battery_process_type = {
	.name = "process",
	.destroy = battery_process_destroy,
};

static Battery_Process *battery_process_receiver(elf_State *state)
{
	Battery_Process *process = elf_to_user_object(state, 0, &battery_process_type);
	if (!process)
	{
		elf_error(state, "expected process receiver");
	}
	return process;
}

static Battery_Process *battery_push_process(elf_State *state, elf_i32 metatable, dy_Process process)
{
	Battery_Process *result = 0;
	elf_ErrorCode error = elf_push_user_object(state, &battery_process_type, metatable, sizeof(*result), (void **)&result);
	if (error != ELF_ERROR_NONE)
	{
		dy_process_close(&process);
		elf_error(state, "could not create process object");
	}
	result->process = process;
	return result;
}

static elf_i32 battery_process_metatable(elf_State *state)
{
	elf_get_global(state, "elf");
	b32 has_process = elf_get_field(state, -1, "process");
	b32 has_metatable = has_process && elf_get_field(state, -1, "Process");
	if (!has_metatable)
	{
		elf_error(state, "process metatable is unavailable");
	}
	return elf_abs_index(state, -1);
}

static u32 battery_process_validate_arguments(elf_State *state, elf_i32 index)
{
	u32 count = 0;
	if (elf_type(state, index) != ELF_VALUE_TYPE_ARRAY || !elf_length(state, index, &count) || count == 0)
	{
		elf_error(state, "process.start expected a non-empty argument array");
	}
	for (u32 argument_index = 0; argument_index < count; ++argument_index)
	{
		elf_StrSlice argument = {0};
		b32 loaded = elf_get_index(state, index, argument_index) && elf_to_str(state, -1, &argument);
		if (!loaded)
		{
			elf_error(state, "process.start arguments must be strings");
		}
		elf_pop(state, 1);
	}
	return count;
}

static dy_String_Array battery_process_load_arguments(elf_State *state, elf_i32 index, u32 count, dy_Arena *arena)
{
	dy_String_Array arguments = {0};
	arguments.items = dy_arena_push(arena, sizeof(*arguments.items) * count, _Alignof(dy_String));
	if (arguments.items)
	{
		arguments.count = count;
		for (u32 argument_index = 0; argument_index < count; ++argument_index)
		{
			elf_StrSlice argument = {0};
			elf_get_index(state, index, argument_index);
			elf_to_str(state, -1, &argument);
			arguments.items[argument_index] = (dy_String){ .data = argument.data, .size = argument.size };
			elf_pop(state, 1);
		}
	}
	return arguments;
}

static void battery_process_load_string_option(elf_State *state, elf_i32 options_index, const char *name, dy_String *value)
{
	elf_StrSlice string = {0};
	elf_get_field(state, options_index, name);
	if (!elf_is_nil(state, -1))
	{
		if (!elf_to_str(state, -1, &string))
		{
			elf_error(state, "process string option must be a string");
		}
		*value = (dy_String){ .data = string.data, .size = string.size };
	}
	elf_pop(state, 1);
}

static void battery_process_load_stream_option(elf_State *state, elf_i32 options_index, const char *name, dy_Stream **stream)
{
	elf_get_field(state, options_index, name);
	if (!elf_is_nil(state, -1))
	{
		Battery_Stream *value = elf_to_user_object(state, -1, &battery_stream_type);
		if (!value)
		{
			elf_error(state, "process stream option must be a stream");
		}
		if (!value->stream)
		{
			elf_error(state, "process stream option is closed");
		}
		*stream = value->stream;
	}
	elf_pop(state, 1);
}

static void battery_process_load_options(elf_State *state, elf_i32 index, dy_Process_Start_Options *options)
{
	if (elf_type(state, index) != ELF_VALUE_TYPE_TABLE)
	{
		elf_error(state, "process.start options must be a table");
	}
	battery_process_load_string_option(state, index, "working_directory", &options->working_directory);
	battery_process_load_stream_option(state, index, "stdin", &options->standard_input);
	battery_process_load_stream_option(state, index, "stdout", &options->standard_output);
	battery_process_load_stream_option(state, index, "stderr", &options->standard_error);
	elf_get_field(state, index, "hide_window");
	if (!elf_is_nil(state, -1))
	{
		options->hide_window = lib_load_integer(state, (u32)elf_abs_index(state, -1)) != 0;
	}
	elf_pop(state, 1);
}

static int battery_push_process_state(elf_State *state, dy_Result result, dy_Process_Status process_state, dy_u32 exit_code)
{
	int return_count = 1;
	if (result.error != DY_ERROR_NONE)
	{
		elf_push_int(state, BATTERY_PROCESS_FAILURE);
	}
	else if (process_state == DY_PROCESS_EXITED)
	{
		elf_push_int(state, BATTERY_PROCESS_EXITED);
		elf_push_int(state, (elf_Int)exit_code);
		return_count = 2;
	}
	else
	{
		elf_push_int(state, BATTERY_PROCESS_RUNNING);
	}
	return return_count;
}

ELF_FUNCTION(l_process_start)
{
	lib_check_arg_count(S, "process.start", nargs, 1, 2);
	elf_i32 arguments_index = elf_abs_index(S, 1);
	u32 argument_count = battery_process_validate_arguments(S, arguments_index);
	dy_Process_Start_Options options = {0};
	if (nargs > 2)
	{
		battery_process_load_options(S, elf_abs_index(S, 2), &options);
	}
	elf_i32 metatable = battery_process_metatable(S);
	dy_Scratch scratch = dy_begin_scratch();
	options.arguments = battery_process_load_arguments(S, arguments_index, argument_count, scratch.arena);
	dy_Process process = {0};
	dy_Result result = { .error = DY_ERROR_OUT_OF_MEMORY };
	if (options.arguments.items)
	{
		result = dy_process_start(options, &process);
	}
	dy_end_scratch(scratch);
	elf_push_int(S, battery_io_status(result.error));
	int return_count = 1;
	if (result.error == DY_ERROR_NONE)
	{
		battery_push_process(S, metatable, process);
		return_count = 2;
	}
	return return_count;
}

ELF_FUNCTION(l_process_poll)
{
	lib_check_arg_count(S, "process.poll", nargs, 0, 0);
	Battery_Process *process = battery_process_receiver(S);
	dy_Process_Status process_state = DY_PROCESS_RUNNING;
	dy_u32 exit_code = 0;
	dy_Result result = { .error = DY_ERROR_INVALID_ARGUMENT };
	if (dy_process_is_valid(process->process))
	{
		result = dy_process_poll(process->process, &process_state, &exit_code);
	}
	return battery_push_process_state(S, result, process_state, exit_code);
}

ELF_FUNCTION(l_process_wait)
{
	lib_check_arg_count(S, "process.wait", nargs, 0, 1);
	Battery_Process *process = battery_process_receiver(S);
	dy_u32 milliseconds = DY_WAIT_INFINITE;
	if (nargs > 1)
	{
		i64 value = lib_load_integer(S, 1);
		if (value < 0 || (u64)value > UINT32_MAX)
		{
			elf_error(S, "process wait duration is out of range");
		}
		milliseconds = (dy_u32)value;
	}
	dy_Process_Status process_state = DY_PROCESS_RUNNING;
	dy_u32 exit_code = 0;
	dy_Result result = { .error = DY_ERROR_INVALID_ARGUMENT };
	if (dy_process_is_valid(process->process))
	{
		result = dy_process_wait(process->process, milliseconds, &process_state, &exit_code);
	}
	return battery_push_process_state(S, result, process_state, exit_code);
}

ELF_FUNCTION(l_process_kill)
{
	lib_check_arg_count(S, "process.kill", nargs, 1, 1);
	Battery_Process *process = battery_process_receiver(S);
	i64 value = lib_load_integer(S, 1);
	if (value < 0 || (u64)value > UINT32_MAX)
	{
		elf_error(S, "process kill exit code is out of range");
	}
	dy_Result result = { .error = DY_ERROR_INVALID_ARGUMENT };
	if (dy_process_is_valid(process->process))
	{
		result = dy_process_kill(process->process, (dy_u32)value);
	}
	elf_push_int(S, battery_io_status(result.error));
	return 1;
}

ELF_FUNCTION(l_process_close)
{
	lib_check_arg_count(S, "process.close", nargs, 0, 0);
	Battery_Process *process = battery_process_receiver(S);
	dy_Result result = {0};
	if (dy_process_is_valid(process->process))
	{
		result = dy_process_close(&process->process);
	}
	elf_push_int(S, battery_io_status(result.error));
	return 1;
}

ELF_FUNCTION(l_process_is_closed)
{
	lib_check_arg_count(S, "process.is_closed", nargs, 0, 0);
	Battery_Process *process = battery_process_receiver(S);
	elf_push_int(S, !dy_process_is_valid(process->process));
	return 1;
}

ELF_FUNCTION(lib_process_id)
{
	lib_check_arg_count(S, "process.id", nargs, 0, 0);
	elf_push_int(S, (elf_i64)dy_current_process_id());
	return 1;
}

ELF_FUNCTION(lib_process_exit)
{
	lib_check_arg_count(S, "process.exit", nargs, 1, 1);
	i64 exit_code = lib_load_integer(S, 1);
	if (exit_code < INT32_MIN || exit_code > INT32_MAX)
	{
		elf_error(S, "process exit code is out of range");
	}
	dy_exit_process((dy_i32)exit_code);
	return 0;
}

static const Battery_Binding l_process_methods[] = {
	{"poll",      l_process_poll},
	{"wait",      l_process_wait},
	{"kill",      l_process_kill},
	{"close",     l_process_close},
	{"is_closed", l_process_is_closed},
};

static const Battery_Binding l_process[] = {
	{"start", l_process_start},
	{"id",    lib_process_id},
	{"exit",  lib_process_exit},
};

static void elf_lib_process(elf_State *state)
{
	new_binding_table(state, l_process, battery_array_count(sizeof(l_process), sizeof(l_process[0])));
	elf_i32 process = elf_abs_index(state, -1);
	new_binding_table(state, l_process_methods, battery_array_count(sizeof(l_process_methods), sizeof(l_process_methods[0])));
	elf_set_field(state, process, "Process");
	lib_set_integer_field(state, process, "RUNNING", BATTERY_PROCESS_RUNNING);
	lib_set_integer_field(state, process, "EXITED", BATTERY_PROCESS_EXITED);
	lib_set_integer_field(state, process, "FAILURE", BATTERY_PROCESS_FAILURE);
}
