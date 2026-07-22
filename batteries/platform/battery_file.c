//
// Filesystem bindings over the shared platform library.
//

static Platform_File battery_platform_file(elf_PlatformFile file)
{
	return (Platform_File){ .value = (UPtr)file };
}

static elf_PlatformFile battery_elf_file(Platform_File file)
{
	return (elf_PlatformFile)file.value;
}

elf_PlatformFile elf_platform_open_file(const char *name, int flags, int mode)
{
	Platform_File_Access access = 0;
	if (flags & ELF_PLATFORM_OPEN_READ) access |= PLATFORM_FILE_READ;
	if (flags & ELF_PLATFORM_OPEN_WRITE) access |= PLATFORM_FILE_WRITE;
	if (flags & ELF_PLATFORM_OPEN_EXECUTE) access |= PLATFORM_FILE_EXECUTE;
	if (flags & ELF_PLATFORM_SHARE_READ) access |= PLATFORM_FILE_SHARE_READ;
	if (flags & ELF_PLATFORM_SHARE_WRITE) access |= PLATFORM_FILE_SHARE_WRITE;
	if (flags & ELF_PLATFORM_NO_BUFFERING) access |= PLATFORM_FILE_NO_BUFFERING;

	Platform_File_Intent intent = PLATFORM_FILE_OPEN_EXISTING;
	switch (mode) {
	case ELF_PLATFORM_CREATE_ALWAYS: intent = PLATFORM_FILE_CREATE_ALWAYS; break;
	case ELF_PLATFORM_CREATE_NEW: intent = PLATFORM_FILE_CREATE_NEW; break;
	case ELF_PLATFORM_TRUNCATE_EXISTING: intent = PLATFORM_FILE_TRUNCATE_EXISTING; break;
	case ELF_PLATFORM_OPEN_ALWAYS: intent = PLATFORM_FILE_OPEN_ALWAYS; break;
	case ELF_PLATFORM_OPEN_EXISTING: intent = PLATFORM_FILE_OPEN_EXISTING; break;
	default: return ELF_HINVALID;
	}

	Platform_File file = platform_access_file(name, intent, access);
	return platform_file_is_valid(file) ? battery_elf_file(file) : ELF_HINVALID;
}

void elf_platform_close_file(elf_PlatformFile file)
{
	platform_close_file(battery_platform_file(file));
}

i64 elf_platform_file_size(elf_PlatformFile file)
{
	U64 size = 0;
	if (!platform_get_file_size(battery_platform_file(file), &size) || size > INT64_MAX) return -1;
	return (i64)size;
}

i64 elf_platform_read_file(elf_PlatformFile file, void *buffer, i64 size)
{
	if (size < 0) return -1;
	U64 read = 0;
	if (!platform_read_file(battery_platform_file(file), buffer, (U64)size, &read) || read > INT64_MAX) return -1;
	return (i64)read;
}

i64 elf_platform_write_file(elf_PlatformFile file, void *buffer, i64 size)
{
	if (size < 0) return -1;
	U64 written = 0;
	if (!platform_write_file(battery_platform_file(file), buffer, (U64)size, &written) || written > INT64_MAX) return -1;
	return (i64)written;
}

i64 elf_platform_seek_file(elf_PlatformFile file, int origin, i64 distance)
{
	Platform_Seek_Origin seek_origin = PLATFORM_SEEK_CURRENT;
	if (origin == ELF_PLATFORM_SEEK_BEGIN) seek_origin = PLATFORM_SEEK_BEGIN;
	else if (origin == ELF_PLATFORM_SEEK_END) seek_origin = PLATFORM_SEEK_END;
	else if (origin != ELF_PLATFORM_SEEK_CURRENT) return -1;
	U64 position = 0;
	if (!platform_set_file_cursor(battery_platform_file(file), seek_origin, distance, &position) || position > INT64_MAX) return -1;
	return (i64)position;
}

int elf_platform_make_dir(const char *path)
{
	return platform_create_directory(path);
}

b32 elf_platform_delete_file(const char *path)
{
	return platform_remove_file(path);
}

b32 elf_platform_get_file_info(const char *path, elf_PlatformFileInfo *info)
{
	Platform_File_Info platform_info;
	if (!info || !platform_get_file_info(path, &platform_info) || platform_info.size > INT64_MAX) return false;
	info->type = ELF_PLATFORM_FILE;
	if (platform_info.is_symbolic_link) info->type = ELF_PLATFORM_SYMLINK;
	else if (platform_info.is_directory) info->type = ELF_PLATFORM_FOLDER;
	info->size_bytes = (i64)platform_info.size;
	info->created_unix_ms = platform_info.created_unix_ms;
	info->accessed_unix_ms = platform_info.accessed_unix_ms;
	info->modified_unix_ms = platform_info.modified_unix_ms;
	return true;
}

#define ELF_FS_MAX_RECURSION 32
#define ELF_PATH_CAPACITY 32768

static b32 battery_collect_paths(elf_State *state, char *path, u32 path_size, u32 recursion_level, elf_i32 output, b32 call_each, u32 *count)
{
	path[path_size] = 0;
	Platform_Directory_Open_Result opened = platform_open_directory(path);
	if (opened.error) return false;
	for (;;) {
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

		if (recursion_level && next.info.is_directory && !next.info.is_symbolic_link) {
			battery_collect_paths(state, path, child_size, recursion_level - 1, output, call_each, count);
		}
		path[path_size] = 0;
	}
	platform_close_directory(&opened.directory);
	return true;
}

ELF_FUNCTION(elf_platform_fs_get_paths)
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
	b32 success = battery_collect_paths(S, path, (u32)root.size, (u32)recursion_level, result, false, &count);
	free(path);
	if (!success) {
		elf_pop(S, 1);
		elf_push_nil(S);
	}
	return 1;
}

ELF_FUNCTION(elf_platform_fs_for_each_path)
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
	b32 success = battery_collect_paths(S, path, (u32)root.size, (u32)recursion_level, callback, true, &count);
	free(path);
	if (success) elf_push_int(S, count);
	else elf_push_nil(S);
	return 1;
}
