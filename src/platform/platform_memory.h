//
// See Copyright Notice In elf.h
//

#ifndef ELF_PLATFORM_MEMORY_H
#define ELF_PLATFORM_MEMORY_H

#include "base.h"

void *elf_platform_virtual_alloc(i64 size);
void elf_platform_virtual_free(void *memory);

#endif
