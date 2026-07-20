//
// See Copyright Notice In elf.h
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

#if defined(PATH_BUILDER)
static void elf_platform_push_file_data(elf_PlatformFileIter *iter, WIN32_FIND_DATAA *info)
{
	iter->type = ELF_PLATFORM_FILE;
	if (info->dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
		iter->type = ELF_PLATFORM_SYMLINK;
	}
	else if (info->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
		iter->type = ELF_PLATFORM_FOLDER;
	}

	iter->size = info->nFileSizeLow;
	elf_path_push(&iter->pb, info->cFileName);
}

elf_PlatformFile elf_platform_find_first_file(elf_PlatformFileIter *iter)
{
	elf_path_push(&iter->pb, "*");
	WIN32_FIND_DATAA info;
	HANDLE hand = FindFirstFileA(iter->pb.path, &info);
	elf_path_pop(&iter->pb);

	if (hand == INVALID_HANDLE_VALUE) return 0;
	elf_platform_push_file_data(iter, &info);
	return elf_platform_file_from_win32(hand);
}

int elf_platform_find_next_file(elf_PlatformFile hand, elf_PlatformFileIter *iter)
{
	elf_path_pop(&iter->pb);

	WIN32_FIND_DATAA info;
	int found = FindNextFileA(win32_handle(hand), &info);
	if (found) {
		elf_platform_push_file_data(iter, &info);
	}
	return found;
}

void elf_platform_find_close(elf_PlatformFile hand)
{
	FindClose(win32_handle(hand));
}
#endif
