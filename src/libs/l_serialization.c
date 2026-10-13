//
// Optional file-backed serialization operations.
//

ELF_FUNCTION(l_serialization_load_json_file)
{
	lib_check_arg_count(S, "serialization.load_json_file", nargs, 1, 1);
	const char *name = lib_load_cstr(S, 1);
	dy_File_Info info;
	if (dy_get_file_info(dy_string_from_cstring(name), &info).error || info.is_directory || info.size > INT_MAX) {
		elf_push_nil(S);
		return 1;
	}
	dy_File file;
	if (dy_access_file(dy_string_from_cstring(name), DY_FILE_OPEN_EXISTING,
		DY_FILE_READ | DY_FILE_SHARE_READ, &file).error) {
		elf_push_nil(S);
		return 1;
	}

	u64 allocation_size = info.size + 1;
	char *data = elf_alloc(S, allocation_size);
	if (!data) {
		dy_close_file(file);
		elf_push_nil(S);
		return 1;
	}
	dy_u64 size = 0;
	dy_Result read = dy_read_file(file, data, info.size, &size);
	dy_close_file(file);
	if (read.error || size != info.size) {
		elf_free(S, data, allocation_size);
		elf_push_nil(S);
		return 1;
	}

	data[size] = 0;
	elf_StrSlice source = {data, size};
	if (elf_push_json(S, name, source) != ELF_ERROR_NONE) elf_push_nil(S);
	elf_free(S, data, allocation_size);
	return 1;
}

static const Lib_Binding l_serialization[] = {
	{"load_json_file", l_serialization_load_json_file},
};

static void elf_lib_serialization(elf_State *state)
{
	new_binding_table(state, l_serialization, lib_array_count(sizeof(l_serialization), sizeof(l_serialization[0])));
}
