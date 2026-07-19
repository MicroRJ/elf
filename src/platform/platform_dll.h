//
// See Copyright Notice In elf.h
//

#ifndef ELF_PLATFORM_DLL_H
#define ELF_PLATFORM_DLL_H

#include "platform_types.h"

elf_PlatformFile elf_platform_load_dll(const char *name);
void *elf_platform_dll_symbol(elf_PlatformFile lib, const char *name);

#endif
