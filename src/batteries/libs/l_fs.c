//
// Optional filesystem operations.
//

#define ELF_FS_MAX_RECURSION 32

static b32 fs_get_file_info(const char *path, dy_File_Info *info)
{
	return !dy_get_file_info(dy_string_from_cstring(path), info).error &&
		!info->is_directory && !info->is_symbolic_link;
}

static int fs_push_path_kind(elf_State *state, int nargs, b32 files, b32 directories)
{
	lib_check_arg_count(state, "fs path query", nargs, 1, 1);
	dy_File_Info info = {0};
	b32 found = !dy_get_file_info(dy_string_from_cstring(lib_load_cstr(state, 1)), &info).error;
	elf_push_int(state, found && ((files && !info.is_directory) || (directories && info.is_directory)));
	return 1;
}

ELF_FUNCTION(lib_fs_exists)
{
	return fs_push_path_kind(S, nargs, true, true);
}

ELF_FUNCTION(lib_fs_is_file)
{
	return fs_push_path_kind(S, nargs, true, false);
}

ELF_FUNCTION(lib_fs_is_directory)
{
	return fs_push_path_kind(S, nargs, false, true);
}

ELF_FUNCTION(lib_fs_file_exists)
{
	lib_check_arg_count(S, "fs.file_exists", nargs, 1, 1);
	dy_File_Info info = {0};
	elf_push_int(S, fs_get_file_info(lib_load_cstr(S, 1), &info));
	return 1;
}

ELF_FUNCTION(lib_fs_read_text_file)
{
	lib_check_arg_count(S, "fs.read_text_file", nargs, 1, 1);
	const char *path = lib_load_cstr(S, 1);
	dy_File_Info info = {0};
	if (!fs_get_file_info(path, &info) || info.size > INT_MAX) {
		elf_push_nil(S);
		return 1;
	}
	dy_File file;
	if (dy_access_file(dy_string_from_cstring(path), DY_FILE_OPEN_EXISTING,
		DY_FILE_READ | DY_FILE_SHARE_READ, &file).error) {
		elf_push_nil(S);
		return 1;
	}
	char *data = malloc((size_t)info.size + 1);
	if (!data) {
		dy_close_file(file);
		elf_push_nil(S);
		return 1;
	}
	dy_u64 size = 0;
	dy_Result read = dy_read_file(file, data, info.size, &size);
	dy_close_file(file);
	if (read.error || size != info.size) {
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
	dy_File file;
	if (dy_access_file(dy_string_from_cstring(path), DY_FILE_CREATE_ALWAYS,
		DY_FILE_WRITE, &file).error) {
		elf_push_int(S, false);
		return 1;
	}
	dy_u64 written = 0;
	dy_Result write = dy_write_file(file, data.data, data.size, &written);
	b32 success = !write.error && written == data.size;
	dy_close_file(file);
	elf_push_int(S, success);
	return 1;
}

ELF_FUNCTION(lib_fs_get_file_info)
{
	lib_check_arg_count(S, "fs.get_file_info", nargs, 1, 1);
	dy_File_Info info = {0};
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
	elf_push_int(S, !dy_create_directory(dy_string_from_cstring(lib_load_cstr(S, 1))).error);
	return 1;
}

ELF_FUNCTION(lib_fs_create_directories)
{
	lib_check_arg_count(S, "fs.create_directories", nargs, 1, 1);
	elf_push_int(S, !dy_create_directories(dy_string_from_cstring(lib_load_cstr(S, 1))).error);
	return 1;
}

static int fs_transfer_file(elf_State *state, int nargs, b32 move)
{
	lib_check_arg_count(state, move ? "fs.move_file" : "fs.copy_file", nargs, 2, 3);
	const char *source = lib_load_cstr(state, 1);
	const char *destination = lib_load_cstr(state, 2);
	b32 overwrite = nargs == 4 ? (b32)lib_load_integer(state, 3) : true;
	dy_Result transfer = move ? dy_move_file(dy_string_from_cstring(source),
		dy_string_from_cstring(destination), overwrite) : dy_copy_file(dy_string_from_cstring(source),
		dy_string_from_cstring(destination), overwrite);
	b32 success = !transfer.error;
	elf_push_int(state, success);
	return 1;
}

ELF_FUNCTION(lib_fs_copy_file)
{
	return fs_transfer_file(S, nargs, false);
}

ELF_FUNCTION(lib_fs_move_file)
{
	return fs_transfer_file(S, nargs, true);
}

ELF_FUNCTION(lib_fs_remove_file)
{
	lib_check_arg_count(S, "fs.remove_file", nargs, 1, 1);
	elf_push_int(S, !dy_remove_file(dy_string_from_cstring(lib_load_cstr(S, 1))).error);
	return 1;
}

ELF_FUNCTION(lib_fs_remove_directory)
{
	lib_check_arg_count(S, "fs.remove_directory", nargs, 1, 1);
	elf_push_int(S, !dy_remove_directory(dy_string_from_cstring(lib_load_cstr(S, 1))).error);
	return 1;
}

ELF_FUNCTION(lib_fs_remove_tree)
{
	lib_check_arg_count(S, "fs.remove_tree", nargs, 1, 1);
	elf_push_int(S, !dy_remove_tree(dy_string_from_cstring(lib_load_cstr(S, 1))).error);
	return 1;
}

ELF_FUNCTION(lib_fs_get_working_directory)
{
	lib_check_arg_count(S, "fs.get_working_directory", nargs, 0, 0);
	dy_Scratch scratch = dy_begin_scratch();
	dy_String directory;
	if (dy_get_current_directory(scratch.arena, &directory).error || directory.size > INT_MAX) {
		dy_end_scratch(scratch);
		elf_push_nil(S);
		return 1;
	}
	lib_push_string(S, directory.data, (u32)directory.size);
	dy_end_scratch(scratch);
	return 1;
}

ELF_FUNCTION(lib_fs_set_working_directory)
{
	lib_check_arg_count(S, "fs.set_working_directory", nargs, 1, 1);
	elf_push_int(S, !dy_set_current_directory(dy_string_from_cstring(lib_load_cstr(S, 1))).error);
	return 1;
}

static b32 fs_collect_paths(elf_State *state, dy_Path_Builder *path, u32 recursion_level,
	elf_i32 output, b32 call_each, u32 *count)
{
	dy_Directory directory;
	dy_Directory_Entry entry;
	dy_Directory_Status status;
	if (dy_find_first_file(path, &directory, &entry, &status).error) return false;
	while (status == DY_DIRECTORY_ENTRY)
	{
		dy_Path_Mark mark = dy_path_mark(path);
		if (!dy_path_push(path, entry.name))
		{
			if (dy_find_next_file(&directory, &entry, &status).error) {
				dy_close_directory(&directory);
				return false;
			}
			continue;
		}
		if (call_each) {
			elf_push_value(state, output);
			elf_push_nil(state);
			elf_push_str(state, path->data, (int)path->size);
			elf_call(state, 2, 0);
			++*count;
		}
		else {
			elf_push_str(state, path->data, (int)path->size);
			elf_append(state, output);
		}
		if (recursion_level && entry.info.is_directory && !entry.info.is_symbolic_link)
		{
			fs_collect_paths(state, path, recursion_level - 1, output, call_each, count);
		}
		dy_path_pop(path, mark);
		if (dy_find_next_file(&directory, &entry, &status).error) {
			dy_close_directory(&directory);
			return false;
		}
	}
	return !dy_close_directory(&directory).error && status == DY_DIRECTORY_END;
}

ELF_FUNCTION(lib_fs_get_paths)
{
	(void)nrets;
	if (nargs < 1 || nargs > 3) {
		elf_push_nil(S);
		return 1;
	}
	elf_StrSlice root = {".", 1};
	if ((nargs >= 2 && !elf_to_str(S, 1, &root)) || root.size == 0 || root.size >= DY_PATH_CAPACITY) {
		elf_push_nil(S);
		return 1;
	}
	elf_Int recursion_level = 0;
	if ((nargs == 3 && !elf_to_int(S, 2, &recursion_level)) || recursion_level < 0 || recursion_level > ELF_FS_MAX_RECURSION) {
		elf_push_nil(S);
		return 1;
	}
	elf_new_table(S);
	elf_i32 result = elf_abs_index(S, -1);
	char storage[DY_PATH_CAPACITY];
	dy_Path_Builder path;
	if (!dy_path_builder_init(&path, storage, sizeof(storage), dy_string_from_data(root.data, root.size))) {
		elf_pop(S, 1);
		elf_push_nil(S);
		return 1;
	}
	u32 count = 0;
	b32 success = fs_collect_paths(S, &path, (u32)recursion_level, result, false, &count);
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
	elf_Int recursion_level = 0;
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
	if (root.size == 0 || root.size >= DY_PATH_CAPACITY || recursion_level < 0 || recursion_level > ELF_FS_MAX_RECURSION || !elf_is_callable(S, callback)) {
		elf_push_nil(S);
		return 1;
	}
	callback = elf_abs_index(S, callback);
	char storage[DY_PATH_CAPACITY];
	dy_Path_Builder path;
	if (!dy_path_builder_init(&path, storage, sizeof(storage), dy_string_from_data(root.data, root.size))) {
		elf_push_nil(S);
		return 1;
	}
	u32 count = 0;
	b32 success = fs_collect_paths(S, &path, (u32)recursion_level, callback, true, &count);
	if (success) elf_push_int(S, count);
	else elf_push_nil(S);
	return 1;
}

static const Battery_Binding l_fs[] = {
	{"exists",                lib_fs_exists},
	{"is_file",               lib_fs_is_file},
	{"is_directory",          lib_fs_is_directory},
	{"file_exists",           lib_fs_file_exists},
	{"get_paths",             lib_fs_get_paths},
	{"for_each_path",         lib_fs_for_each_path},
	{"read_text_file",        lib_fs_read_text_file},
	{"write_text_file",       lib_fs_write_text_file},
	{"get_file_info",         lib_fs_get_file_info},
	{"create_directory",      lib_fs_create_directory},
	{"create_directories",    lib_fs_create_directories},
	{"copy_file",             lib_fs_copy_file},
	{"move_file",             lib_fs_move_file},
	{"remove_file",           lib_fs_remove_file},
	{"remove_directory",      lib_fs_remove_directory},
	{"remove_tree",           lib_fs_remove_tree},
	{"get_working_directory", lib_fs_get_working_directory},
	{"set_working_directory", lib_fs_set_working_directory},
};

static void elf_lib_fs(elf_State *state)
{
	new_binding_table(state, l_fs, battery_array_count(sizeof(l_fs), sizeof(l_fs[0])));
}
