//
// Process environment variables.
//

static dy_Result query_environment(Arena *arena, dy_String name, dy_String *value)
{
	return dy_get_env_field(arena, name, value);
}

ELF_FUNCTION(lib_env_get)
{
	lib_check_arg_count(S, "env.get", nargs, 1, 1);
	dy_String name = lib_load_string_view(S, 1);
	Scratch scratch = scratch_begin();
	dy_String value;
	dy_Result query = query_environment(scratch.arena, name, &value);
	if (query.error != DY_ERROR_NONE && query.error != DY_ERROR_NOT_FOUND) {
		scratch_end(scratch);
		elf_error(S, "unable to read environment variable");
	}
	if (query.error == DY_ERROR_NOT_FOUND) {
		scratch_end(scratch);
		elf_push_nil(S);
		return 1;
	}
	elf_push_str(S, value.data, (int)value.size);
	scratch_end(scratch);
	return 1;
}

ELF_FUNCTION(lib_env_has)
{
	lib_check_arg_count(S, "env.has", nargs, 1, 1);
	Scratch scratch = scratch_begin();
	dy_String value;
	dy_Result result = query_environment(scratch.arena, lib_load_string_view(S, 1), &value);
	if (result.error != DY_ERROR_NONE && result.error != DY_ERROR_NOT_FOUND) {
		scratch_end(scratch);
		elf_error(S, "unable to read environment variable");
	}
	scratch_end(scratch);
	elf_push_int(S, result.error != DY_ERROR_NOT_FOUND);
	return 1;
}

ELF_FUNCTION(lib_env_set)
{
	lib_check_arg_count(S, "env.set", nargs, 2, 2);
	dy_Result result = dy_set_env_field(lib_load_string_view(S, 1), lib_load_string_view(S, 2));
	elf_push_int(S, result.error == DY_ERROR_NONE);
	return 1;
}

ELF_FUNCTION(lib_env_unset)
{
	lib_check_arg_count(S, "env.unset", nargs, 1, 1);
	dy_Result result = dy_remove_env_field(lib_load_string_view(S, 1));
	elf_push_int(S, result.error == DY_ERROR_NONE);
	return 1;
}

static const elf_Binding l_env[] = {
	{"get",   lib_env_get},
	{"has",   lib_env_has},
	{"set",   lib_env_set},
	{"unset", lib_env_unset},
};

static void elf_lib_env(elf_State *state)
{
	new_binding_table(state, l_env, ARRAY_COUNT(l_env));
}
