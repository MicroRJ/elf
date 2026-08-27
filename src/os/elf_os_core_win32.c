//
// See Copyright Notice In elf.h
//
#include "elf_os.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stddef.h>

void *elf_os_virtual_reserve(elf_u64 size)
{
	if (size > SIZE_MAX) return NULL;
	return VirtualAlloc(NULL, (SIZE_T) size, MEM_RESERVE, PAGE_READWRITE);
}

elf_b32 elf_os_virtual_commit(void *memory, elf_u64 size)
{
	return memory && size <= SIZE_MAX && VirtualAlloc(memory, (SIZE_T) size, MEM_COMMIT, PAGE_READWRITE) != NULL;
}

void elf_os_virtual_release(void *memory)
{
	if (memory) VirtualFree(memory, 0, MEM_RELEASE);
}

elf_u64 elf_os_counter(void)
{
	LARGE_INTEGER value;
	return QueryPerformanceCounter(&value) ? (elf_u64)value.QuadPart : 0;
}

elf_u64 elf_os_counter_frequency(void)
{
	LARGE_INTEGER value;
	return QueryPerformanceFrequency(&value) ? (elf_u64)value.QuadPart : 0;
}

void elf_os_debug_break(void)
{
	DebugBreak();
}

void elf_os_exit_process(elf_i32 exit_code)
{
	ExitProcess((UINT) exit_code);
}

static HANDLE elf_os_standard_stream_handle(elf_OS_StandardStream stream)
{
	if (stream == ELF_OS_STANDARD_OUTPUT) return GetStdHandle(STD_OUTPUT_HANDLE);
	if (stream == ELF_OS_STANDARD_ERROR) return GetStdHandle(STD_ERROR_HANDLE);
	return NULL;
}

elf_b32 elf_os_enable_console_colors(elf_OS_StandardStream stream)
{
	HANDLE handle = elf_os_standard_stream_handle(stream);
	DWORD mode = 0;
	if (!handle || handle == INVALID_HANDLE_VALUE || !GetConsoleMode(handle, &mode)) return 0;
	return SetConsoleMode(handle, mode | ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
}
