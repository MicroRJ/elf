//
// Optional file-backed serialization operations.
//

ELF_FUNCTION(l_serialization_load_json_file)
{
	lib_check_arg_count(S, "serialization.load_json_file", nargs, 1, 1);
	const char *name = lib_load_cstr(S, 1);
	Platform_File_Info info;
	if (!platform_get_file_info(name, &info) || info.is_directory || info.size > INT_MAX) {
		elf_push_nil(S);
		return 1;
	}
	Platform_File file = platform_access_file(name, PLATFORM_FILE_OPEN_EXISTING, PLATFORM_FILE_READ | PLATFORM_FILE_SHARE_READ);
	if (!platform_file_is_valid(file)) {
		elf_push_nil(S);
		return 1;
	}

	char *data = malloc((size_t)info.size + 1);
	if (!data) {
		platform_close_file(file);
		elf_push_nil(S);
		return 1;
	}
	U64 size = 0;
	B32 success = platform_read_file(file, data, info.size, &size);
	platform_close_file(file);
	if (!success || size != info.size) {
		free(data);
		elf_push_nil(S);
		return 1;
	}

	data[size] = 0;
	elf_StrSlice source = {data, size};
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
