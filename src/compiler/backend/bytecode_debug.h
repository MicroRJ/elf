//
// See Copyright Notice In elf.h
//

#ifndef ELF_BYTECODE_DEBUG_H
#define ELF_BYTECODE_DEBUG_H

#include "base.h"

typedef struct elf_State elf_State;
typedef struct BcFunction BcFunction;

char *format_bytecode_function(elf_State *state, Arena *arena, BcFunction function);
void print_bytecode_function(elf_State *state, BcFunction function);

#endif
