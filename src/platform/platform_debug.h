//
// See Copyright Notice In elf.h
//
#ifndef ELF_PLATFORM_DEBUG_H
#define ELF_PLATFORM_DEBUG_H

#include "base.h"

b32 elf_platform_debug_break(void);
void elf_platform_enable_console_colors(void);
void elf_platform_exit_process(int errorcode);

#endif
