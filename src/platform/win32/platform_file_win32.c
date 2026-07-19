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
	ReadFile(win32_handle(file), buf, (DWORD)size, &read, NULL);
	return read;
}

i64 elf_platform_write_file(elf_PlatformFile file, void *buf, i64 size)
{
	DWORD wrote = 0;
	WriteFile(win32_handle(file), buf, (DWORD)size, &wrote, NULL);
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

int elf_platform_file_times(elf_PlatformFile file, elf_PlatformFileTimes *times)
{
	return GetFileTime(win32_handle(file), win32_file_time(&times->create), win32_file_time(&times->access), win32_file_time(&times->write));
}

void elf_platform_file_time_to_system_time(elf_PlatformFileTime *filetime, elf_PlatformSystemTime *system_time)
{
	FileTimeToSystemTime(win32_file_time(filetime), win32_system_time(system_time));
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
