//
// See Copyright Notice In elf.h
//
#ifndef ELF_OS_H
#define ELF_OS_H

#include "elf.h"

typedef enum
{
	ELF_OS_STANDARD_OUTPUT,
	ELF_OS_STANDARD_ERROR,
}
elf_OS_StandardStream;

void *elf_os_virtual_reserve(elf_u64 size);
elf_b32 elf_os_virtual_commit(void *memory, elf_u64 size);
void elf_os_virtual_release(void *memory);

elf_u64 elf_os_counter(void);
elf_u64 elf_os_counter_frequency(void);

void elf_os_debug_break(void);
void elf_os_exit_process(elf_i32 exit_code);
elf_b32 elf_os_enable_console_colors(elf_OS_StandardStream stream);

#endif
