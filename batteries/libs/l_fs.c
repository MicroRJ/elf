//
// Optional filesystem operations.
//

static b32 fs_get_file_info(const char *path, elf_PlatformFileInfo *info)
{
	return elf_platform_get_file_info(path, info) && info->type == ELF_PLATFORM_FILE;
}

ELF_FUNCTION(lib_fs_file_exists)
{
	lib_check_arg_count(S, "fs.file_exists", nargs, 1, 1);
	elf_PlatformFileInfo info = {0};
	b32 exists = fs_get_file_info(lib_load_cstr(S, 1), &info);
	elf_push_int(S, exists);
	return 1;
}

ELF_FUNCTION(lib_fs_read_text_file)
{
	lib_check_arg_count(S, "fs.read_text_file", nargs, 1, 1);
	const char *path = lib_load_cstr(S, 1);
	elf_PlatformFileInfo info = {0};
	if (!fs_get_file_info(path, &info) || info.size_bytes < 0 || info.size_bytes > INT_MAX)
	{
		elf_push_nil(S);
		return 1;
	}

	elf_PlatformFile file = elf_platform_open_file(path,
		ELF_PLATFORM_OPEN_READ | ELF_PLATFORM_SHARE_READ, ELF_PLATFORM_OPEN_EXISTING);
	if (ELF_IS_HANDLE_INVALID(file))
	{
		elf_push_nil(S);
		return 1;
	}

	char *data = malloc((size_t)info.size_bytes + 1);
	if (!data) {
		elf_platform_close_file(file);
		elf_push_nil(S);
		return 1;
	}
	i64 size = elf_platform_read_file(file, data, info.size_bytes);
	elf_platform_close_file(file);
	if (size != info.size_bytes)
	{
		free(data);
		elf_push_nil(S);
		return 1;
	}

	lib_push_string(S, data, (u32)size);
	free(data);
	return 1;
}

ELF_FUNCTION(lib_fs_write_text_file)
{
	lib_check_arg_count(S, "fs.write_text_file", nargs, 2, 2);
	const char *path = lib_load_cstr(S, 1);
	elf_StrSlice data = lib_load_string(S, 2);
	elf_PlatformFile file = elf_platform_open_file(path,
		ELF_PLATFORM_OPEN_WRITE, ELF_PLATFORM_CREATE_ALWAYS);
	if (ELF_IS_HANDLE_INVALID(file))
	{
		elf_push_int(S, false);
		return 1;
	}

	i64 written = elf_platform_write_file(file, data.data, (i64)data.size);
	elf_platform_close_file(file);
	elf_push_int(S, written == (i64)data.size);
	return 1;
}

ELF_FUNCTION(lib_fs_get_file_info)
{
	lib_check_arg_count(S, "fs.get_file_info", nargs, 1, 1);
	elf_PlatformFileInfo info = {0};
	if (!fs_get_file_info(lib_load_cstr(S, 1), &info))
	{
		elf_push_nil(S);
		return 1;
	}

	elf_new_table(S);
	elf_i32 result = elf_abs_index(S, -1);
	lib_set_integer_field(S, result, "size_bytes", info.size_bytes);
	lib_set_integer_field(S, result, "created_unix_ms", info.created_unix_ms);
	lib_set_integer_field(S, result, "accessed_unix_ms", info.accessed_unix_ms);
	lib_set_integer_field(S, result, "modified_unix_ms", info.modified_unix_ms);
	return 1;
}

ELF_FUNCTION(lib_fs_create_directory)
{
	lib_check_arg_count(S, "fs.create_directory", nargs, 1, 1);
	elf_push_int(S, elf_platform_make_dir(lib_load_cstr(S, 1)));
	return 1;
}

ELF_FUNCTION(lib_fs_remove_file)
{
	lib_check_arg_count(S, "fs.remove_file", nargs, 1, 1);
	elf_push_int(S, elf_platform_delete_file(lib_load_cstr(S, 1)));
	return 1;
}

ELF_FUNCTION(lib_fs_get_working_directory)
{
	lib_check_arg_count(S, "fs.get_working_directory", nargs, 0, 0);
	const i32 capacity = 32768;
	char *buffer = calloc(1, capacity);
	i32 size = elf_platform_work_dir(buffer, capacity);
	if (size <= 0 || size >= capacity) elf_push_nil(S);
	else lib_push_string(S, buffer, size);
	free(buffer);
	return 1;
}

ELF_FUNCTION(lib_fs_set_working_directory)
{
	lib_check_arg_count(S, "fs.set_working_directory", nargs, 1, 1);
	elf_push_int(S, elf_platform_set_work_dir(lib_load_cstr(S, 1)));
	return 1;
}

static const Battery_Binding l_fs[] = {
	{"file_exists",           lib_fs_file_exists},
	{"get_paths",             elf_platform_fs_get_paths},
	{"for_each_path",         elf_platform_fs_for_each_path},
	{"read_text_file",        lib_fs_read_text_file},
	{"write_text_file",       lib_fs_write_text_file},
	{"get_file_info",         lib_fs_get_file_info},
	{"create_directory",      lib_fs_create_directory},
	{"remove_file",           lib_fs_remove_file},
	{"get_working_directory", lib_fs_get_working_directory},
	{"set_working_directory", lib_fs_set_working_directory},
};

static void elf_lib_fs(elf_State *state)
{
	new_binding_table(state, l_fs, battery_array_count(sizeof(l_fs), sizeof(l_fs[0])));
}
