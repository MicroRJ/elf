//
// See Copyright Notice In elf.h
//

#ifndef ELF_CORE_VALUE_TEXT_H
#define ELF_CORE_VALUE_TEXT_H

void elf_print_value(elf_Arena *arena, elf_Value value);
b32 elf_unparse_value(elf_State *state, elf_Arena *arena, elf_Value value, u32 indent);

#endif
