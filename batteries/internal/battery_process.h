//
// See Copyright Notice In elf.h
//

#ifndef ELF_BATTERY_PROCESS_H
#define ELF_BATTERY_PROCESS_H

#include "battery_platform_types.h"

typedef struct elf_PlatformProcessResult
{
	b32 started;
	i32 exit_code;
	i32 error_code;
}
elf_PlatformProcessResult;

int elf_platform_process_id(void);
void battery_exit_process(int errorcode);

int elf_platform_work_dir(char *buf, int bufsize);
int elf_platform_set_work_dir(const char *buf);

int elf_platform_last_error(void);
void elf_platform_error_message(int error, char *buffer, int size);

elf_PlatformFile elf_platform_create_process(const char *file, const char *args);
elf_PlatformProcessResult elf_platform_run_process(const char *command_line, elf_Buffer *standard_output, elf_Buffer *standard_error);

#endif
