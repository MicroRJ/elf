//
// See Copyright Notice In elf.h
//
#include "elf_os_services.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

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
