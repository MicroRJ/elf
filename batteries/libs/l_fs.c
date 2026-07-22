//
// Optional filesystem operations.
//

#define ELF_FS_MAX_RECURSION 32
#define ELF_PATH_CAPACITY 32768

static b32 fs_get_file_info(const char *path, Platform_File_Info *info)
{
	return platform_get_file_info(path, info) && !info->is_directory && !info->is_symbolic_link;
}

ELF_FUNCTION(lib_fs_file_exists)
{
	lib_check_arg_count(S, "fs.file_exists", nargs, 1, 1);
	Platform_File_Info info = {0};
	elf_push_int(S, fs_get_file_info(lib_load_cstr(S, 1), &info));
	return 1;
}

ELF_FUNCTION(lib_fs_read_text_file)
{
	lib_check_arg_count(S, "fs.read_text_file", nargs, 1, 1);
	const char *path = lib_load_cstr(S, 1);
	Platform_File_Info info = {0};
	if (!fs_get_file_info(path, &info) || info.size > INT_MAX) {
		elf_push_nil(S);
		return 1;
	}
	Platform_File file = platform_access_file(path, PLATFORM_FILE_OPEN_EXISTING, PLATFORM_FILE_READ | PLATFORM_FILE_SHARE_READ);
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
	B32 read = platform_read_file(file, data, info.size, &size);
	platform_close_file(file);
	if (!read || size != info.size) {
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
	Platform_File file = platform_access_file(path, PLATFORM_FILE_CREATE_ALWAYS, PLATFORM_FILE_WRITE);
	if (!platform_file_is_valid(file)) {
		elf_push_int(S, false);
		return 1;
	}
	U64 written = 0;
	B32 success = platform_write_file(file, data.data, data.size, &written) && written == data.size;
	platform_close_file(file);
	elf_push_int(S, success);
	return 1;
}

ELF_FUNCTION(lib_fs_get_file_info)
{
	lib_check_arg_count(S, "fs.get_file_info", nargs, 1, 1);
	Platform_File_Info info = {0};
	if (!fs_get_file_info(lib_load_cstr(S, 1), &info) || info.size > INT64_MAX) {
		elf_push_nil(S);
		return 1;
	}
	elf_new_table(S);
	elf_i32 result = elf_abs_index(S, -1);
	lib_set_integer_field(S, result, "size_bytes", (i64)info.size);
	lib_set_integer_field(S, result, "created_unix_ms", info.created_unix_ms);
	lib_set_integer_field(S, result, "accessed_unix_ms", info.accessed_unix_ms);
	lib_set_integer_field(S, result, "modified_unix_ms", info.modified_unix_ms);
	return 1;
}

ELF_FUNCTION(lib_fs_create_directory)
{
	lib_check_arg_count(S, "fs.create_directory", nargs, 1, 1);
	elf_push_int(S, platform_create_directory(lib_load_cstr(S, 1)));
	return 1;
}

ELF_FUNCTION(lib_fs_remove_file)
{
	lib_check_arg_count(S, "fs.remove_file", nargs, 1, 1);
	elf_push_int(S, platform_remove_file(lib_load_cstr(S, 1)));
	return 1;
}

ELF_FUNCTION(lib_fs_get_working_directory)
{
	lib_check_arg_count(S, "fs.get_working_directory", nargs, 0, 0);
	Platform_String_Result query = platform_get_current_directory(NULL, 0);
	if (query.error || query.required_capacity > INT_MAX) {
		elf_push_nil(S);
		return 1;
	}

	// TODO(RJ) remove heap allocation use scratch!
	char *buffer = malloc((size_t)query.required_capacity);
	if (!buffer) {
		elf_push_nil(S);
		return 1;
	}
	Platform_String_Result read = platform_get_current_directory(buffer, query.required_capacity);
	if (read.error) elf_push_nil(S);
	else lib_push_string(S, buffer, (u32)read.size);
	free(buffer);
	return 1;
}

ELF_FUNCTION(lib_fs_set_working_directory)
{
	lib_check_arg_count(S, "fs.set_working_directory", nargs, 1, 1);
	elf_push_int(S, platform_set_current_directory(lib_load_cstr(S, 1)));
	return 1;
}

static b32 fs_collect_paths(elf_State *state, char *path, u32 path_size, u32 recursion_level, elf_i32 output, b32 call_each, u32 *count)
{
	path[path_size] = 0;
	Platform_Directory_Open_Result opened = platform_open_directory(path);
	if (opened.error) return false;
	for (;;)
	{
		// TODO(RJ) use scratch memory!
		char name[ELF_PATH_CAPACITY];
		Platform_Directory_Next_Result next = platform_next_directory(&opened.directory, name, sizeof(name));
		if (next.error) {
			platform_close_directory(&opened.directory);
			return false;
		}
		if (!next.has_entry) break;
		u32 name_size = (u32)next.name_size;
		b32 separator = path_size > 0 && path[path_size - 1] != '/' && path[path_size - 1] != '\\';
		u32 child_offset = path_size + separator;
		if (child_offset + name_size >= ELF_PATH_CAPACITY) continue;
		if (separator) path[path_size] = '/';
		memcpy(path + child_offset, name, name_size);
		u32 child_size = child_offset + name_size;
		path[child_size] = 0;
		if (call_each) {
			elf_push_value(state, output);
			elf_push_nil(state);
			elf_push_str(state, path, (int)child_size);
			elf_call(state, 2, 0);
			++*count;
		}
		else {
			elf_push_str(state, path, (int)child_size);
			elf_append(state, output);
		}
		if (recursion_level && next.info.is_directory && !next.info.is_symbolic_link) fs_collect_paths(state, path, child_size, recursion_level - 1, output, call_each, count);
		path[path_size] = 0;
	}
	platform_close_directory(&opened.directory);
	return true;
}

ELF_FUNCTION(lib_fs_get_paths)
{
	(void)nrets;
	if (nargs < 1 || nargs > 3) {
		elf_push_nil(S);
		return 1;
	}
	elf_StrSlice root = {".", 1};
	if ((nargs >= 2 && !elf_to_str(S, 1, &root)) || root.size == 0 || root.size >= ELF_PATH_CAPACITY) {
		elf_push_nil(S);
		return 1;
	}
	elf_Integer recursion_level = 0;
	if ((nargs == 3 && !elf_to_int(S, 2, &recursion_level)) || recursion_level < 0 || recursion_level > ELF_FS_MAX_RECURSION) {
		elf_push_nil(S);
		return 1;
	}
	elf_new_table(S);
	elf_i32 result = elf_abs_index(S, -1);
	char *path = malloc(ELF_PATH_CAPACITY);
	if (!path) {
		elf_pop(S, 1);
		elf_push_nil(S);
		return 1;
	}
	memcpy(path, root.data, root.size);
	u32 count = 0;
	b32 success = fs_collect_paths(S, path, (u32)root.size, (u32)recursion_level, result, false, &count);
	free(path);
	if (!success) {
		elf_pop(S, 1);
		elf_push_nil(S);
	}
	return 1;
}

ELF_FUNCTION(lib_fs_for_each_path)
{
	(void)nrets;
	if (nargs < 2 || nargs > 4) {
		elf_push_nil(S);
		return 1;
	}
	elf_StrSlice root = {".", 1};
	elf_Integer recursion_level = 0;
	elf_i32 callback = 1;
	if (nargs >= 3) {
		if (!elf_to_str(S, 1, &root)) {
			elf_push_nil(S);
			return 1;
		}
		callback = 2;
	}
	if (nargs == 4) {
		if (!elf_to_int(S, 2, &recursion_level)) {
			elf_push_nil(S);
			return 1;
		}
		callback = 3;
	}
	if (root.size == 0 || root.size >= ELF_PATH_CAPACITY || recursion_level < 0 || recursion_level > ELF_FS_MAX_RECURSION || !elf_is_callable(S, callback)) {
		elf_push_nil(S);
		return 1;
	}
	callback = elf_abs_index(S, callback);
	char *path = malloc(ELF_PATH_CAPACITY);
	if (!path) {
		elf_push_nil(S);
		return 1;
	}
	memcpy(path, root.data, root.size);
	u32 count = 0;
	b32 success = fs_collect_paths(S, path, (u32)root.size, (u32)recursion_level, callback, true, &count);
	free(path);
	if (success) elf_push_int(S, count);
	else elf_push_nil(S);
	return 1;
}

static const Battery_Binding l_fs[] = {
	{"file_exists",           lib_fs_file_exists},
	{"get_paths",             lib_fs_get_paths},
	{"for_each_path",         lib_fs_for_each_path},
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
