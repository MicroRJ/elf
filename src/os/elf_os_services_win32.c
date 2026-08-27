//
// Private Win32 services required by the optional elf batteries.
//
#include "elf_os_services.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

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
