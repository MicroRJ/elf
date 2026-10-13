//
// See Copyright Notice In elf.h
//

#ifndef ELF_CORE_ARRAY_H
#define ELF_CORE_ARRAY_H

elf_Array *elf_array_new(elf_State *state);
u32 elf_array_add(elf_State *state, elf_Array *array, elf_Value value);
u32 elf_array_insert(elf_State *state, elf_Array *array, u32 index, elf_Value value);
elf_Value elf_array_get(elf_State *state, elf_Array *array, u32 index);
void elf_array_set(elf_State *state, elf_Array *array, u32 index, elf_Value value);
void elf_array_remove(elf_State *state, elf_Array *array, u32 index, u32 count);
void elf_array_swap(elf_State *state, elf_Array *array, u32 left, u32 right);
u32 elf_array_length(elf_Array *array);

#endif
