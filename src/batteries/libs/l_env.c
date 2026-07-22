//
// Process environment variables.
//

static Platform_Environment_Result query_environment(elf_State *state, const char *name)
{
	Platform_Environment_Result result = platform_get_environment(name, NULL, 0);
	if (result.error) elf_error(state, "unable to read environment variable");
	return result;
}

ELF_FUNCTION(lib_env_get)
{
	lib_check_arg_count(S, "env.get", nargs, 1, 1);
	const char *name = lib_load_cstr(S, 1);
	Platform_Environment_Result query = query_environment(S, name);
	if (!query.found) {
		elf_push_nil(S);
		return 1;
	}
	char *value = malloc((size_t)query.required_capacity);
	if (!value) elf_error(S, "unable to allocate environment variable");
	Platform_Environment_Result read = platform_get_environment(name, value, query.required_capacity);
	if (read.error) {
		free(value);
		elf_error(S, "unable to read environment variable");
	}
	lib_push_string(S, value, (u32)read.size);
	free(value);
	return 1;
}

ELF_FUNCTION(lib_env_has)
{
	lib_check_arg_count(S, "env.has", nargs, 1, 1);
	Platform_Environment_Result result = query_environment(S, lib_load_cstr(S, 1));
	elf_push_int(S, result.found);
	return 1;
}

ELF_FUNCTION(lib_env_set)
{
	lib_check_arg_count(S, "env.set", nargs, 2, 2);
	Platform_Result result = platform_set_environment(lib_load_cstr(S, 1), lib_load_cstr(S, 2));
	elf_push_int(S, result.error == PLATFORM_ERROR_NONE);
	return 1;
}

ELF_FUNCTION(lib_env_unset)
{
	lib_check_arg_count(S, "env.unset", nargs, 1, 1);
	Platform_Result result = platform_set_environment(lib_load_cstr(S, 1), NULL);
	elf_push_int(S, result.error == PLATFORM_ERROR_NONE);
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
