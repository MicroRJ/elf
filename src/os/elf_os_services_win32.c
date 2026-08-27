//
// See Copyright Notice In elf.h
//
#include "elf_os_services.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string.h>

static HANDLE elf_os_file_handle(elf_OS_File file)
{
	return (HANDLE)file.value;
}

static elf_i64 elf_os_file_time_to_unix_ms(FILETIME time)
{
	ULARGE_INTEGER value;
	value.LowPart = time.dwLowDateTime;
	value.HighPart = time.dwHighDateTime;
	const elf_u64 unix_epoch = 116444736000000000ull;
	if (value.QuadPart < unix_epoch) return 0;
	return (elf_i64)((value.QuadPart - unix_epoch) / 10000ull);
}

static HANDLE elf_os_service_standard_stream_handle(elf_OS_StandardStream stream)
{
	if (stream == ELF_OS_STANDARD_OUTPUT) return GetStdHandle(STD_OUTPUT_HANDLE);
	if (stream == ELF_OS_STANDARD_ERROR) return GetStdHandle(STD_ERROR_HANDLE);
	return NULL;
}

elf_u64 elf_os_write_console(elf_OS_StandardStream stream, const void *data, elf_u64 size)
{
	HANDLE handle = elf_os_service_standard_stream_handle(stream);
	if (!handle || handle == INVALID_HANDLE_VALUE || (!data && size)) return 0;

	elf_u64 total = 0;
	while (total < size)
	{
		elf_u64 remaining = size - total;
		DWORD request = remaining > MAXDWORD ? MAXDWORD : (DWORD)remaining;
		DWORD written = 0;
		if (!WriteFile(handle, (const char *)data + total, request, &written, NULL)) break;
		if (!written) break;
		total += written;
	}
	return total;
}

void elf_os_sleep(elf_u64 milliseconds)
{
	while (milliseconds > MAXDWORD)
	{
		Sleep(MAXDWORD);
		milliseconds -= MAXDWORD;
	}
	Sleep((DWORD)milliseconds);
}

elf_b32 elf_os_get_file_info(const char *path, elf_OS_FileInfo *info)
{
	WIN32_FILE_ATTRIBUTE_DATA data;
	if (!path || !info || !GetFileAttributesExA(path, GetFileExInfoStandard, &data)) return 0;

	ULARGE_INTEGER size;
	size.LowPart = data.nFileSizeLow;
	size.HighPart = data.nFileSizeHigh;
	*info = (elf_OS_FileInfo) {
		.size = size.QuadPart,
		.created_unix_ms = elf_os_file_time_to_unix_ms(data.ftCreationTime),
		.accessed_unix_ms = elf_os_file_time_to_unix_ms(data.ftLastAccessTime),
		.modified_unix_ms = elf_os_file_time_to_unix_ms(data.ftLastWriteTime),
		.is_directory = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0,
		.is_symbolic_link = (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0,
	};
	return 1;
}

elf_OS_File elf_os_open_file_read(const char *path)
{
	if (!path) return (elf_OS_File) {0};
	HANDLE handle = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (handle == INVALID_HANDLE_VALUE) return (elf_OS_File) {0};
	return (elf_OS_File) {.value = (uintptr_t)handle};
}

elf_OS_File elf_os_create_file(const char *path)
{
	if (!path) return (elf_OS_File) {0};
	HANDLE handle = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (handle == INVALID_HANDLE_VALUE) return (elf_OS_File) {0};
	return (elf_OS_File) {.value = (uintptr_t)handle};
}

elf_b32 elf_os_file_is_valid(elf_OS_File file)
{
	return file.value != 0 && elf_os_file_handle(file) != INVALID_HANDLE_VALUE;
}

void elf_os_close_file(elf_OS_File file)
{
	if (elf_os_file_is_valid(file)) CloseHandle(elf_os_file_handle(file));
}

elf_b32 elf_os_read_file(elf_OS_File file, void *data, elf_u64 size, elf_u64 *bytes_read)
{
	elf_u64 total = 0;
	if (bytes_read) *bytes_read = 0;
	if (!elf_os_file_is_valid(file) || (!data && size)) return 0;

	while (total < size)
	{
		elf_u64 remaining = size - total;
		DWORD request = remaining > MAXDWORD ? MAXDWORD : (DWORD)remaining;
		DWORD received = 0;
		if (!ReadFile(elf_os_file_handle(file), (char *)data + total, request, &received, NULL)) return 0;
		total += received;
		if (received < request) break;
	}
	if (bytes_read) *bytes_read = total;
	return 1;
}

elf_b32 elf_os_write_file(elf_OS_File file, const void *data, elf_u64 size, elf_u64 *bytes_written)
{
	elf_u64 total = 0;
	if (bytes_written) *bytes_written = 0;
	if (!elf_os_file_is_valid(file) || (!data && size)) return 0;

	while (total < size)
	{
		elf_u64 remaining = size - total;
		DWORD request = remaining > MAXDWORD ? MAXDWORD : (DWORD)remaining;
		DWORD written = 0;
		if (!WriteFile(elf_os_file_handle(file), (const char *)data + total, request, &written, NULL)) return 0;
		total += written;
		if (written < request) break;
	}
	if (bytes_written) *bytes_written = total;
	return 1;
}

elf_b32 elf_os_copy_file(const char *source, const char *destination, elf_b32 overwrite)
{
	return source && destination && CopyFileA(source, destination, !overwrite) != 0;
}

elf_b32 elf_os_move_file(const char *source, const char *destination, elf_b32 overwrite)
{
	if (!source || !destination) return 0;
	DWORD flags = MOVEFILE_COPY_ALLOWED;
	if (overwrite) flags |= MOVEFILE_REPLACE_EXISTING;
	return MoveFileExA(source, destination, flags) != 0;
}

elf_b32 elf_os_remove_file(const char *path)
{
	if (!path) return 0;
	if (DeleteFileA(path)) return 1;
	DWORD error = GetLastError();
	return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
}

static elf_b32 elf_os_create_directory_internal(const char *path)
{
	if (CreateDirectoryA(path, NULL)) return 1;
	if (GetLastError() != ERROR_ALREADY_EXISTS) return 0;
	DWORD attributes = GetFileAttributesA(path);
	return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

elf_b32 elf_os_create_directory(const char *path)
{
	return path && elf_os_create_directory_internal(path);
}

elf_b32 elf_os_create_directories(const char *path)
{
	if (!path || !path[0]) return 0;
	SIZE_T size = strlen(path) + 1;
	char *copy = HeapAlloc(GetProcessHeap(), 0, size);
	if (!copy) return 0;
	memcpy(copy, path, size);

	SIZE_T root = 0;
	if (copy[0] && copy[1] == ':') root = 2;
	else if (copy[0] == '/' || copy[0] == '\\') root = 1;
	if ((copy[0] == '/' || copy[0] == '\\') && (copy[1] == '/' || copy[1] == '\\'))
	{
		root = 2;
		unsigned components = 0;
		while (copy[root] && components < 2)
		{
			if (copy[root] == '/' || copy[root] == '\\') ++components;
			++root;
		}
	}

	elf_b32 result = 1;
	for (SIZE_T index = root; copy[index]; ++index)
	{
		if (copy[index] != '/' && copy[index] != '\\') continue;
		if (index == root) continue;
		char separator = copy[index];
		copy[index] = 0;
		if (!elf_os_create_directory_internal(copy)) result = 0;
		copy[index] = separator;
		if (!result) break;
	}
	if (result) result = elf_os_create_directory_internal(copy);
	HeapFree(GetProcessHeap(), 0, copy);
	return result;
}

elf_b32 elf_os_remove_directory(const char *path)
{
	if (!path) return 0;
	if (RemoveDirectoryA(path)) return 1;
	DWORD error = GetLastError();
	return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
}

elf_OS_StringResult elf_os_get_current_directory(char *buffer, elf_u64 capacity)
{
	elf_OS_StringResult result = {0};
	DWORD required = GetCurrentDirectoryA(0, NULL);
	if (!required) return result;
	result.required_capacity = required;
	result.size = required - 1;
	if (!buffer)
	{
		result.success = 1;
		return result;
	}
	if (capacity < required || capacity > MAXDWORD) return result;
	DWORD length = GetCurrentDirectoryA((DWORD)capacity, buffer);
	if (!length || length >= capacity) return result;
	result.size = length;
	result.success = 1;
	return result;
}

elf_b32 elf_os_set_current_directory(const char *path)
{
	return path && SetCurrentDirectoryA(path) != 0;
}

typedef struct
{
	HANDLE find;
	WIN32_FIND_DATAA data;
}
elf_OS_Win32Directory;

static void elf_os_win32_directory_entry(elf_OS_Win32Directory *state, elf_OS_DirectoryEntry *entry)
{
	elf_u64 length = strlen(state->data.cFileName);
	ULARGE_INTEGER size;
	size.LowPart = state->data.nFileSizeLow;
	size.HighPart = state->data.nFileSizeHigh;
	*entry = (elf_OS_DirectoryEntry) {
		.name = state->data.cFileName,
		.name_size = length,
		.info = {
			.size = size.QuadPart,
			.created_unix_ms = elf_os_file_time_to_unix_ms(state->data.ftCreationTime),
			.accessed_unix_ms = elf_os_file_time_to_unix_ms(state->data.ftLastAccessTime),
			.modified_unix_ms = elf_os_file_time_to_unix_ms(state->data.ftLastWriteTime),
			.is_directory = (state->data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0,
			.is_symbolic_link = (state->data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0,
		},
	};
}

static elf_OS_DirectoryStatus elf_os_win32_find_next_file(elf_OS_Win32Directory *state,
	elf_OS_DirectoryEntry *entry, elf_b32 use_current)
{
	for (;;)
	{
		if (!use_current && !FindNextFileA(state->find, &state->data))
		{
			return GetLastError() == ERROR_NO_MORE_FILES ? ELF_OS_DIRECTORY_END : ELF_OS_DIRECTORY_ERROR;
		}
		use_current = 0;
		if (strcmp(state->data.cFileName, ".") == 0 || strcmp(state->data.cFileName, "..") == 0) continue;
		elf_os_win32_directory_entry(state, entry);
		return ELF_OS_DIRECTORY_ENTRY;
	}
}

elf_OS_DirectoryStatus elf_os_find_first_file(elf_PathBuilder *path,
	elf_OS_Directory *directory, elf_OS_DirectoryEntry *entry)
{
	if (directory) *directory = (elf_OS_Directory) {0};
	if (entry) *entry = (elf_OS_DirectoryEntry) {0};
	if (!path || !path->data || !directory || !entry) return ELF_OS_DIRECTORY_ERROR;
	elf_OS_Win32Directory *state = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*state));
	if (!state) return ELF_OS_DIRECTORY_ERROR;

	elf_PathMark mark = elf_path_mark(path);
	if (!elf_path_push(path, "*", 1))
	{
		HeapFree(GetProcessHeap(), 0, state);
		return ELF_OS_DIRECTORY_ERROR;
	}
	state->find = FindFirstFileA(path->data, &state->data);
	elf_path_pop(path, mark);
	if (state->find == INVALID_HANDLE_VALUE)
	{
		DWORD error = GetLastError();
		HeapFree(GetProcessHeap(), 0, state);
		return error == ERROR_FILE_NOT_FOUND ? ELF_OS_DIRECTORY_END : ELF_OS_DIRECTORY_ERROR;
	}
	directory->value = (uintptr_t)state;
	return elf_os_win32_find_next_file(state, entry, 1);
}

elf_OS_DirectoryStatus elf_os_find_next_file(elf_OS_Directory *directory, elf_OS_DirectoryEntry *entry)
{
	if (entry) *entry = (elf_OS_DirectoryEntry) {0};
	if (!directory || !directory->value || !entry) return ELF_OS_DIRECTORY_ERROR;
	elf_OS_Win32Directory *state = (elf_OS_Win32Directory *)directory->value;
	return elf_os_win32_find_next_file(state, entry, 0);
}

void elf_os_close_directory(elf_OS_Directory *directory)
{
	if (!directory || !directory->value) return;
	elf_OS_Win32Directory *state = (elf_OS_Win32Directory *)directory->value;
	FindClose(state->find);
	HeapFree(GetProcessHeap(), 0, state);
	*directory = (elf_OS_Directory) {0};
}
