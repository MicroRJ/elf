//
// See Copyright Notice In elf.h
//

#ifndef ELF_CORE_GLOBALS_H
#define ELF_CORE_GLOBALS_H

typedef struct GlobalEntry GlobalEntry;
typedef struct elf_State elf_State;

typedef struct
{
	elf_State *state;
	GlobalEntry *entries;
	u32 entry_capacity;
	elf_Value *values;
	u32 value_capacity;
	u32 count;
}
elf_Globals;

void elf_globals_init(elf_Globals *globals, elf_State *state);
void elf_globals_destroy(elf_Globals *globals);
b32 elf_globals_find(elf_Globals *globals, elf_StrSlice name, u32 *slot);
u32 elf_globals_ensure(elf_Globals *globals, elf_StrSlice name);
elf_Value elf_globals_get_by_name(elf_Globals *globals, elf_StrSlice name);
void elf_globals_set_by_name(elf_Globals *globals, elf_StrSlice name, elf_Value value);
elf_Value elf_globals_get_by_slot(elf_Globals *globals, u32 slot);
void elf_globals_set_by_slot(elf_Globals *globals, u32 slot, elf_Value value);

#endif
