//
// Optional file-backed serialization operations.
//

ELF_FUNCTION(l_serialization_load_json_file)
{
	lib_check_arg_count(S, "serialization.load_json_file", nargs, 1, 1);
	const char *name = lib_load_cstr(S, 1);
	elf_PlatformFile file = elf_platform_open_file(name, ELF_PLATFORM_OPEN_READ, ELF_PLATFORM_OPEN_EXISTING);
	if (ELF_IS_HANDLE_INVALID(file))
	{
		elf_push_nil(S);
		return 1;
	}

	i64 file_size = elf_platform_file_size(file);
	if (file_size < 0 || file_size > INT_MAX)
	{
		elf_platform_close_file(file);
		elf_push_nil(S);
		return 1;
	}

	char *data = malloc((size_t)file_size + 1);
	if (!data) {
		elf_platform_close_file(file);
		elf_push_nil(S);
		return 1;
	}
	i64 size = elf_platform_read_file(file, data, file_size);
	elf_platform_close_file(file);
	if (size != file_size)
	{
		free(data);
		elf_push_nil(S);
		return 1;
	}

	data[size] = 0;
	elf_StrSlice source = {data, (u64)size};
	elf_push_json(S, name, source);
	free(data);
	return 1;
}

static const Battery_Binding l_serialization[] = {
	{"load_json_file", l_serialization_load_json_file},
};

static void elf_lib_serialization(elf_State *state)
{
	new_binding_table(state, l_serialization, battery_array_count(sizeof(l_serialization), sizeof(l_serialization[0])));
}
