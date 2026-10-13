//
// Processes backed by dy-core.
//

typedef enum Lib_Process_State
{
	LIB_PROCESS_RUNNING,
	LIB_PROCESS_EXITED,
	LIB_PROCESS_FAILURE,
}
Lib_Process_State;

typedef struct Lib_Process
{
	dy_Process process;
}
Lib_Process;

static void lib_process_destroy(elf_State *state, void *data)
{
	(void)state;
	Lib_Process *process = data;
	if (dy_process_is_valid(process->process))
	{
		dy_process_close(&process->process);
	}
}

static const elf_UserType lib_process_type = {
	.name = "process",
	.destroy = lib_process_destroy,
};

static Lib_Process *lib_process_receiver(elf_State *state)
{
	Lib_Process *process = elf_to_user_object(state, 0, &lib_process_type);
	if (!process)
	{
		elf_error(state, "expected process receiver");
	}
	return process;
}

static Lib_Process *lib_push_process(elf_State *state, elf_i32 metatable, dy_Process process)
{
	Lib_Process *result = 0;
	elf_ErrorCode error = elf_push_user_object(state, &lib_process_type, metatable, sizeof(*result), (void **)&result);
	if (error != ELF_ERROR_NONE)
	{
		dy_process_close(&process);
		elf_error(state, "could not create process object");
	}
	result->process = process;
	return result;
}

static elf_i32 lib_process_metatable(elf_State *state)
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

static u32 lib_process_validate_arguments(elf_State *state, elf_i32 index)
{
	u32 count = 0;
	if (elf_type(state, index) != ELF_VALUE_TYPE_ARRAY || !elf_length(state, index, &count))
	{
		elf_error(state, "process.start expected an argument array");
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

static dy_String_Array lib_process_load_arguments(elf_State *state, elf_i32 index, u32 count, dy_Arena *arena)
{
	dy_String_Array arguments = {0};
	if (count != 0)
	{
		arguments.items = dy_arena_push(arena, sizeof(*arguments.items) * count, _Alignof(dy_String));
	}
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

static void lib_process_load_string_option(elf_State *state, elf_i32 options_index, const char *name, dy_String *value)
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

static void lib_process_load_stream_option(elf_State *state, elf_i32 options_index, const char *name, dy_Stream **stream)
{
	elf_get_field(state, options_index, name);
	if (!elf_is_nil(state, -1))
	{
		Lib_Stream *value = elf_to_user_object(state, -1, &lib_stream_type);
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

static void lib_process_load_options(elf_State *state, elf_i32 index, dy_Process_Start_Options *options)
{
	if (elf_type(state, index) != ELF_VALUE_TYPE_TABLE)
	{
		elf_error(state, "process.start options must be a table");
	}
	lib_process_load_string_option(state, index, "working_directory", &options->working_directory);
	lib_process_load_stream_option(state, index, "stdin", &options->standard_input);
	lib_process_load_stream_option(state, index, "stdout", &options->standard_output);
	lib_process_load_stream_option(state, index, "stderr", &options->standard_error);
	elf_get_field(state, index, "hide_window");
	if (!elf_is_nil(state, -1))
	{
		options->hide_window = lib_load_stack_integer(state, elf_abs_index(state, -1)) != 0;
	}
	elf_pop(state, 1);
}

static int lib_push_process_state(elf_State *state, dy_Result result, dy_Process_Status process_state, dy_u32 exit_code)
{
	int return_count = 1;
	if (result.error != DY_ERROR_NONE)
	{
		elf_push_int(state, LIB_PROCESS_FAILURE);
	}
	else if (process_state == DY_PROCESS_EXITED)
	{
		elf_push_int(state, LIB_PROCESS_EXITED);
		elf_push_int(state, (elf_Int)exit_code);
		return_count = 2;
	}
	else
	{
		elf_push_int(state, LIB_PROCESS_RUNNING);
	}
	return return_count;
}

ELF_FUNCTION(l_process_start)
{
	lib_check_arg_count(S, "process.start", nargs, 1, 3);
	elf_StrSlice executable_value = lib_load_string_slice(S, 1);
	dy_String executable = { .data = executable_value.data, .size = executable_value.size };
	elf_i32 arguments_index = nargs > 2 ? elf_abs_index(S, 2) : 0;
	u32 argument_count = nargs > 2 ? lib_process_validate_arguments(S, arguments_index) : 0;
	elf_i32 options_index = nargs > 3 ? elf_abs_index(S, 3) : 0;
	elf_i32 metatable = lib_process_metatable(S);
	dy_Scratch scratch = dy_scratch_begin();
	dy_String_Array arguments = lib_process_load_arguments(S, arguments_index, argument_count, scratch.arena);
	dy_Process_Start_Options options = {
		.executable = executable,
		.arguments = arguments,
	};
	if (nargs > 3)
	{
		lib_process_load_options(S, options_index, &options);
	}
	dy_Process process = {0};
	dy_Result result = { .error = DY_ERROR_OUT_OF_MEMORY };
	if (argument_count == 0 || arguments.items)
	{
		result = dy_process_start(options, &process);
	}
	dy_scratch_end(scratch);
	elf_push_int(S, lib_io_status(result.error));
	int return_count = 1;
	if (result.error == DY_ERROR_NONE)
	{
		lib_push_process(S, metatable, process);
		return_count = 2;
	}
	return return_count;
}

ELF_FUNCTION(l_process_poll)
{
	lib_check_arg_count(S, "process.poll", nargs, 0, 0);
	Lib_Process *process = lib_process_receiver(S);
	dy_Process_Status process_state = DY_PROCESS_RUNNING;
	dy_u32 exit_code = 0;
	dy_Result result = { .error = DY_ERROR_INVALID_ARGUMENT };
	if (dy_process_is_valid(process->process))
	{
		result = dy_process_poll(process->process, &process_state, &exit_code);
	}
	return lib_push_process_state(S, result, process_state, exit_code);
}

ELF_FUNCTION(l_process_wait)
{
	lib_check_arg_count(S, "process.wait", nargs, 0, 1);
	Lib_Process *process = lib_process_receiver(S);
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
	return lib_push_process_state(S, result, process_state, exit_code);
}

ELF_FUNCTION(l_process_kill)
{
	lib_check_arg_count(S, "process.kill", nargs, 1, 1);
	Lib_Process *process = lib_process_receiver(S);
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
	elf_push_int(S, lib_io_status(result.error));
	return 1;
}

ELF_FUNCTION(l_process_close)
{
	lib_check_arg_count(S, "process.close", nargs, 0, 0);
	Lib_Process *process = lib_process_receiver(S);
	dy_Result result = {0};
	if (dy_process_is_valid(process->process))
	{
		result = dy_process_close(&process->process);
	}
	elf_push_int(S, lib_io_status(result.error));
	return 1;
}

ELF_FUNCTION(l_process_is_closed)
{
	lib_check_arg_count(S, "process.is_closed", nargs, 0, 0);
	Lib_Process *process = lib_process_receiver(S);
	elf_push_int(S, !dy_process_is_valid(process->process));
	return 1;
}

ELF_FUNCTION(lib_process_id)
{
	lib_check_arg_count(S, "process.id", nargs, 0, 0);
	elf_push_int(S, (elf_i64)dy_process_current_id());
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
	dy_process_exit((dy_i32)exit_code);
	return 0;
}

static const elf_Binding l_process_methods[] = {
	{"poll",      l_process_poll},
	{"wait",      l_process_wait},
	{"kill",      l_process_kill},
	{"close",     l_process_close},
	{"is_closed", l_process_is_closed},
};

static const elf_Binding l_process[] = {
	{"start", l_process_start},
	{"id",    lib_process_id},
	{"exit",  lib_process_exit},
};

static void elf_lib_process(elf_State *state)
{
	new_binding_table(state, l_process, ARRAY_COUNT(l_process));
	elf_i32 process = elf_abs_index(state, -1);
	new_binding_table(state, l_process_methods, ARRAY_COUNT(l_process_methods));
	elf_set_field(state, process, "Process");
	lib_set_integer_field(state, process, "RUNNING", LIB_PROCESS_RUNNING);
	lib_set_integer_field(state, process, "EXITED", LIB_PROCESS_EXITED);
	lib_set_integer_field(state, process, "FAILURE", LIB_PROCESS_FAILURE);
}
