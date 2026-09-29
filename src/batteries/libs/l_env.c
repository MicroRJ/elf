//
// Process environment variables.
//

static day_Result query_environment(day_Arena *arena, const char *name, day_String *value)
{
	return day_get_env_field(arena, day_string_from_cstring(name), value);
}

ELF_FUNCTION(lib_env_get)
{
	lib_check_arg_count(S, "env.get", nargs, 1, 1);
	const char *name = lib_load_cstr(S, 1);
	day_Scratch scratch = day_begin_scratch();
	day_String value;
	day_Result query = query_environment(scratch.arena, name, &value);
	if (query.error != DAY_ERROR_NONE && query.error != DAY_ERROR_NOT_FOUND) {
		day_end_scratch(scratch);
		elf_error(S, "unable to read environment variable");
	}
	if (query.error == DAY_ERROR_NOT_FOUND) {
		day_end_scratch(scratch);
		elf_push_nil(S);
		return 1;
	}
	lib_push_string(S, value.data, (u32)value.size);
	day_end_scratch(scratch);
	return 1;
}

ELF_FUNCTION(lib_env_has)
{
	lib_check_arg_count(S, "env.has", nargs, 1, 1);
	day_Scratch scratch = day_begin_scratch();
	day_String value;
	day_Result result = query_environment(scratch.arena, lib_load_cstr(S, 1), &value);
	if (result.error != DAY_ERROR_NONE && result.error != DAY_ERROR_NOT_FOUND) {
		day_end_scratch(scratch);
		elf_error(S, "unable to read environment variable");
	}
	day_end_scratch(scratch);
	elf_push_int(S, result.error != DAY_ERROR_NOT_FOUND);
	return 1;
}

ELF_FUNCTION(lib_env_set)
{
	lib_check_arg_count(S, "env.set", nargs, 2, 2);
	day_Result result = day_set_env_field(day_string_from_cstring(lib_load_cstr(S, 1)),
		day_string_from_cstring(lib_load_cstr(S, 2)));
	elf_push_int(S, result.error == DAY_ERROR_NONE);
	return 1;
}

ELF_FUNCTION(lib_env_unset)
{
	lib_check_arg_count(S, "env.unset", nargs, 1, 1);
	day_Result result = day_remove_env_field(day_string_from_cstring(lib_load_cstr(S, 1)));
	elf_push_int(S, result.error == DAY_ERROR_NONE);
	return 1;
}

static const Battery_Binding l_env[] = {
	{"get",   lib_env_get},
	{"has",   lib_env_has},
	{"set",   lib_env_set},
	{"unset", lib_env_unset},
};

static void elf_lib_env(elf_State *state)
{
	new_binding_table(state, l_env, battery_array_count(sizeof(l_env), sizeof(l_env[0])));
}
