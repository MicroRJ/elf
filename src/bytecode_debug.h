//
// See Copyright Notice In elf.h
//

#ifndef ELF_BYTECODE_DEBUG_H
#define ELF_BYTECODE_DEBUG_H

#include "base.h"

typedef struct elf_State elf_State;
typedef struct BcFunctionRef BcFunctionRef;

char *format_bytecode_function(elf_State *state, elf_Arena *arena, BcFunctionRef function);
void print_bytecode_function(elf_State *state, BcFunctionRef function);

#endif
