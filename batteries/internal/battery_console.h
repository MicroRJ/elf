//
// See Copyright Notice In elf.h
//

#ifndef ELF_BATTERY_CONSOLE_H
#define ELF_BATTERY_CONSOLE_H

#include "battery_platform_types.h"

typedef enum
{
	ELF_PLATFORM_STD_OUTPUT = 0,
	ELF_PLATFORM_STD_ERROR,
	ELF_PLATFORM_STD_INPUT,
}
elf_PlatformStdFile;

char *elf_platform_command_line(void);
elf_PlatformFile elf_platform_std_file(int std);
unsigned int elf_platform_read_console(elf_PlatformFile file, char *buf, unsigned int size);
void elf_platform_console_print(int type, char *message);

#endif
