#ifndef ELF_CORE_MEMORY_H
#define ELF_CORE_MEMORY_H

#include "elf.h"

void *elf_alloc(elf_State *state, elf_u64 size);
void *elf_alloc_zero(elf_State *state, elf_u64 size);
void *elf_realloc(elf_State *state, void *memory, elf_u64 old_size, elf_u64 new_size);
void elf_free(elf_State *state, void *memory, elf_u64 size);

#endif
