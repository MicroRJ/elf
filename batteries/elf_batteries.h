#ifndef ELF_BATTERIES_H
#define ELF_BATTERIES_H

#include "elf.h"

void elf_open_batteries(elf_State *state);
int elf_push_code_file(elf_State *state, const char *name);

#endif
