//
// Optional Win32 filesystem support.
//

elf_PlatformFile elf_platform_open_file(const char *name, int flags, int mode)
{
	int os_flags = 0;
	if (flags & ELF_PLATFORM_OPEN_READ) os_flags |= GENERIC_READ;
	if (flags & ELF_PLATFORM_OPEN_WRITE) os_flags |= GENERIC_WRITE;
	if (flags & ELF_PLATFORM_OPEN_EXECUTE) os_flags |= GENERIC_EXECUTE;

	int os_sharing_flags = 0;
	if (flags & ELF_PLATFORM_SHARE_READ) os_sharing_flags |= FILE_SHARE_READ;
	if (flags & ELF_PLATFORM_SHARE_WRITE) os_sharing_flags |= FILE_SHARE_WRITE;

	int os_misc_flags = 0;
	if (flags & ELF_PLATFORM_NO_BUFFERING) os_misc_flags |= FILE_FLAG_NO_BUFFERING;

	int os_mode = OPEN_ALWAYS;
	switch (mode)
	{
		case ELF_PLATFORM_CREATE_ALWAYS:     os_mode = CREATE_ALWAYS; break;
		case ELF_PLATFORM_CREATE_NEW:        os_mode = CREATE_NEW; break;
		case ELF_PLATFORM_OPEN_ALWAYS:       os_mode = OPEN_ALWAYS; break;
		case ELF_PLATFORM_OPEN_EXISTING:     os_mode = OPEN_EXISTING; break;
		case ELF_PLATFORM_TRUNCATE_EXISTING: os_mode = TRUNCATE_EXISTING; break;
	}

	HANDLE file = CreateFileA(name, os_flags, os_sharing_flags, NULL, os_mode, os_misc_flags, NULL);
	return file == INVALID_HANDLE_VALUE ? 0 : elf_platform_file_from_win32(file);
}

void elf_platform_close_file(elf_PlatformFile file)
{
	CloseHandle(win32_handle(file));
}

i64 elf_platform_file_size(elf_PlatformFile file)
{
	LARGE_INTEGER size;
	if (!GetFileSizeEx(win32_handle(file), &size)) return -1;
	return size.QuadPart;
}

i64 elf_platform_read_file(elf_PlatformFile file, void *buf, i64 size)
{
	DWORD read = 0;
	if (!ReadFile(win32_handle(file), buf, (DWORD)size, &read, NULL)) return -1;
	return read;
}

i64 elf_platform_write_file(elf_PlatformFile file, void *buf, i64 size)
{
	DWORD wrote = 0;
	if (!WriteFile(win32_handle(file), buf, (DWORD)size, &wrote, NULL)) return -1;
	return wrote;
}

i64 elf_platform_seek_file(elf_PlatformFile file, int origin, i64 distance)
{
	DWORD move_method = FILE_CURRENT;
	if (origin == ELF_PLATFORM_SEEK_BEGIN) move_method = FILE_BEGIN;
	else if (origin == ELF_PLATFORM_SEEK_END) move_method = FILE_END;

	LARGE_INTEGER offset;
	LARGE_INTEGER position;
	offset.QuadPart = distance;
	if (!SetFilePointerEx(win32_handle(file), offset, &position, move_method)) return -1;
	return position.QuadPart;
}

void elf_platform_flush_file(elf_PlatformFile file)
{
	FlushFileBuffers(win32_handle(file));
}

int elf_platform_make_dir(const char *path)
{
	return CreateDirectoryA(path, NULL);
}

b32 elf_platform_delete_file(const char *path)
{
	return DeleteFileA(path);
}

static i64 win32_file_time_to_unix_ms(FILETIME time)
{
	ULARGE_INTEGER ticks;
	ticks.LowPart = time.dwLowDateTime;
	ticks.HighPart = time.dwHighDateTime;
	const u64 unix_epoch_ticks = 116444736000000000ull;
	if (ticks.QuadPart < unix_epoch_ticks) return 0;
	return (i64)((ticks.QuadPart - unix_epoch_ticks) / 10000ull);
}

b32 elf_platform_get_file_info(const char *path, elf_PlatformFileInfo *info)
{
	WIN32_FILE_ATTRIBUTE_DATA data;
	if (!GetFileAttributesExA(path, GetFileExInfoStandard, &data)) return false;

	info->type = ELF_PLATFORM_FILE;
	if (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) info->type = ELF_PLATFORM_SYMLINK;
	else if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) info->type = ELF_PLATFORM_FOLDER;

	ULARGE_INTEGER size;
	size.LowPart = data.nFileSizeLow;
	size.HighPart = data.nFileSizeHigh;
	if (size.QuadPart > 0x7fffffffffffffffull) return false;
	info->size_bytes = (i64)size.QuadPart;
	info->created_unix_ms = win32_file_time_to_unix_ms(data.ftCreationTime);
	info->accessed_unix_ms = win32_file_time_to_unix_ms(data.ftLastAccessTime);
	info->modified_unix_ms = win32_file_time_to_unix_ms(data.ftLastWriteTime);
	return true;
}

static b32 win32_is_virtual_path(const char *name)
{
	return (name[0] == '.' && name[1] == 0)
	|| (name[0] == '.' && name[1] == '.' && name[2] == 0);
}

#define ELF_FS_MAX_RECURSION 32
#define WIN32_PATH_CAPACITY 32768

static b32 win32_collect_paths(elf_State *S, char *path, u32 path_size,
u32 recursion_level, elf_i32 output, b32 call_each, u32 *count)
{
	b32 separator = path[path_size - 1] != '\\' && path[path_size - 1] != '/';
	u32 child_offset = path_size + separator;
	if (child_offset + 1 >= WIN32_PATH_CAPACITY) return false;
	if (separator) path[path_size] = '\\';
	path[child_offset] = '*';
	path[child_offset + 1] = 0;

	WIN32_FIND_DATAA info;
	HANDLE handle = FindFirstFileA(path, &info);
	if (handle == INVALID_HANDLE_VALUE) return false;

	do
	{
		if (win32_is_virtual_path(info.cFileName)) continue;
		u32 name_size = (u32)strlen(info.cFileName);
		if (child_offset + name_size >= WIN32_PATH_CAPACITY) continue;
		CopyMemory(path + child_offset, info.cFileName, name_size);
		u32 child_size = child_offset + name_size;
		if (call_each)
		{
			elf_push_value(S, output);
			elf_push_nil(S);
			elf_push_str(S, path, (int)child_size);
			elf_call(S, 2, 0);
			++*count;
		}
		else
		{
			elf_push_str(S, path, (int)child_size);
			elf_append(S, output);
		}

		b32 directory = (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
		b32 reparse_point = (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
		if (recursion_level && directory && !reparse_point)
		{
			win32_collect_paths(S, path, child_size, recursion_level - 1,
			output, call_each, count);
		}
	}
	while (FindNextFileA(handle, &info));

	FindClose(handle);
	return true;
}

ELF_FUNCTION(elf_platform_fs_get_paths)
{
	(void)nrets;
	if (nargs < 1 || nargs > 3)
	{
		elf_push_nil(S);
		return 1;
	}

	elf_StrSlice root = {".", 1};
	if ((nargs == 2 && !elf_to_str(S, 1, &root))
	||  (nargs == 3 && !elf_to_str(S, 1, &root))
	||   root.size == 0 || root.size >= WIN32_PATH_CAPACITY - 2)
	{
		elf_push_nil(S);
		return 1;
	}

	elf_Integer recursion_level = 0;
	if ((nargs == 3 && !elf_to_int(S, 2, &recursion_level))
	|| recursion_level < 0 || recursion_level > ELF_FS_MAX_RECURSION)
	{
		elf_push_nil(S);
		return 1;
	}

	elf_new_table(S);
	elf_i32 result = elf_abs_index(S, -1);
	char *path = malloc(WIN32_PATH_CAPACITY);
	if (!path) {
		elf_pop(S, 1);
		elf_push_nil(S);
		return 1;
	}
	CopyMemory(path, root.data, (size_t)root.size);
	u32 count = 0;
	b32 success = win32_collect_paths(S, path, (u32)root.size,
	(u32)recursion_level, result, false, &count);
	free(path);
	if (!success)
	{
		elf_pop(S, 1);
		elf_push_nil(S);
	}
	return 1;
}

ELF_FUNCTION(elf_platform_fs_for_each_path)
{
	(void)nrets;
	if (nargs < 2 || nargs > 4)
	{
		elf_push_nil(S);
		return 1;
	}

	elf_StrSlice root = {".", 1};
	elf_Integer recursion_level = 0;
	elf_i32 callback = 1;
	if (nargs >= 3)
	{
		if (!elf_to_str(S, 1, &root))
		{
			elf_push_nil(S);
			return 1;
		}
		callback = 2;
	}
	if (nargs == 4)
	{
		if (!elf_to_int(S, 2, &recursion_level))
		{
			elf_push_nil(S);
			return 1;
		}
		callback = 3;
	}
	if (root.size == 0 || root.size >= WIN32_PATH_CAPACITY - 2
	|| recursion_level < 0 || recursion_level > ELF_FS_MAX_RECURSION
	|| !elf_is_callable(S, callback))
	{
		elf_push_nil(S);
		return 1;
	}

	callback = elf_abs_index(S, callback);
	char *path = malloc(WIN32_PATH_CAPACITY);
	if (!path) {
		elf_push_nil(S);
		return 1;
	}
	CopyMemory(path, root.data, (size_t)root.size);
	u32 count = 0;
	b32 success = win32_collect_paths(S, path, (u32)root.size,
	(u32)recursion_level, callback, true, &count);
	free(path);
	if (success) elf_push_int(S, count);
	else elf_push_nil(S);
	return 1;
}
