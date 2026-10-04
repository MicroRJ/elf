//
// Process environment variables.
//

static dy_Result query_environment(dy_Arena *arena, const char *name, dy_String *value)
{
	return dy_get_env_field(arena, dy_string_from_cstring(name), value);
}

ELF_FUNCTION(lib_env_get)
{
	lib_check_arg_count(S, "env.get", nargs, 1, 1);
	const char *name = lib_load_cstr(S, 1);
	dy_Scratch scratch = dy_begin_scratch();
	dy_String value;
	dy_Result query = query_environment(scratch.arena, name, &value);
	if (query.error != DY_ERROR_NONE && query.error != DY_ERROR_NOT_FOUND) {
		dy_end_scratch(scratch);
		elf_error(S, "unable to read environment variable");
	}
	if (query.error == DY_ERROR_NOT_FOUND) {
		dy_end_scratch(scratch);
		elf_push_nil(S);
		return 1;
	}
	lib_push_string(S, value.data, (u32)value.size);
	dy_end_scratch(scratch);
	return 1;
}

ELF_FUNCTION(lib_env_has)
{
	lib_check_arg_count(S, "env.has", nargs, 1, 1);
	dy_Scratch scratch = dy_begin_scratch();
	dy_String value;
	dy_Result result = query_environment(scratch.arena, lib_load_cstr(S, 1), &value);
	if (result.error != DY_ERROR_NONE && result.error != DY_ERROR_NOT_FOUND) {
		dy_end_scratch(scratch);
		elf_error(S, "unable to read environment variable");
	}
	dy_end_scratch(scratch);
	elf_push_int(S, result.error != DY_ERROR_NOT_FOUND);
	return 1;
}

ELF_FUNCTION(lib_env_set)
{
	lib_check_arg_count(S, "env.set", nargs, 2, 2);
	dy_Result result = dy_set_env_field(dy_string_from_cstring(lib_load_cstr(S, 1)),
		dy_string_from_cstring(lib_load_cstr(S, 2)));
	elf_push_int(S, result.error == DY_ERROR_NONE);
	return 1;
}

ELF_FUNCTION(lib_env_unset)
{
	lib_check_arg_count(S, "env.unset", nargs, 1, 1);
	dy_Result result = dy_remove_env_field(dy_string_from_cstring(lib_load_cstr(S, 1)));
	elf_push_int(S, result.error == DY_ERROR_NONE);
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
