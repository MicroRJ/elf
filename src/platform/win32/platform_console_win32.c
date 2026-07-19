//
// See Copyright Notice In elf.h
//

char *elf_platform_command_line(void)
{
	return GetCommandLineA();
}

elf_PlatformFile elf_platform_std_file(int std)
{
	switch (std)
	{
		case ELF_PLATFORM_STD_OUTPUT: return elf_platform_file_from_win32(GetStdHandle(STD_OUTPUT_HANDLE));
		case ELF_PLATFORM_STD_INPUT:  return elf_platform_file_from_win32(GetStdHandle(STD_INPUT_HANDLE));
		case ELF_PLATFORM_STD_ERROR:  return elf_platform_file_from_win32(GetStdHandle(STD_ERROR_HANDLE));
	}
	return 0;
}

static void elf_platform_enable_console_colors_for_file(elf_PlatformFile file)
{
	DWORD mode = 0;
	HANDLE handle = win32_handle(file);
	if (handle && handle != INVALID_HANDLE_VALUE && GetConsoleMode(handle, &mode))
	{
		mode |= ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
		SetConsoleMode(handle, mode);
	}
}

void elf_platform_enable_console_colors(void)
{
	elf_platform_enable_console_colors_for_file(elf_platform_file_from_win32(GetStdHandle(STD_OUTPUT_HANDLE)));
	elf_platform_enable_console_colors_for_file(elf_platform_file_from_win32(GetStdHandle(STD_ERROR_HANDLE)));
}

unsigned int elf_platform_read_console(elf_PlatformFile file, char *buf, unsigned int size)
{
	(void)file;
	DWORD read = 0;
	ReadConsole(GetStdHandle(STD_INPUT_HANDLE), buf, size, &read, NULL);
	return read;
}

void elf_platform_console_print(int type, char *message)
{
	(void)type;
	(void)message;
}
