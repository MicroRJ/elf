#ifndef ELF_LIBS_H
#define ELF_LIBS_H

#include "elf.h"

elf_SymTable *elf_open_core_libraries(elf_State *state);
void elf_open_host_libraries(elf_State *state, elf_SymTable *elf_symbols);

#endif
