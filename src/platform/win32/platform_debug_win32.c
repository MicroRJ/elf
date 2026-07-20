//
// See Copyright Notice In elf.h
//

b32 elf_platform_debug_break(void)
{
	DebugBreak();
	return true;
}

static void win32_enable_console_colors(HANDLE handle)
{
	DWORD mode = 0;
	if (handle && handle != INVALID_HANDLE_VALUE && GetConsoleMode(handle, &mode))
	{
		mode |= ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
		SetConsoleMode(handle, mode);
	}
}

void elf_platform_enable_console_colors(void)
{
	win32_enable_console_colors(GetStdHandle(STD_OUTPUT_HANDLE));
	win32_enable_console_colors(GetStdHandle(STD_ERROR_HANDLE));
}

void elf_platform_exit_process(int errorcode)
{
	ExitProcess((UINT)errorcode);
}
