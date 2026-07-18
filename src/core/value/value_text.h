#ifndef ELF_CORE_VALUE_TEXT_H
#define ELF_CORE_VALUE_TEXT_H

void print_value(Arena *arena, elf_Value value);
b32 serialize_value(elf_State *state, Arena *arena, elf_Value value, u32 indent);

#endif
