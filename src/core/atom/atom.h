#ifndef ELF_CORE_ATOM_H
#define ELF_CORE_ATOM_H

typedef struct elf_State elf_State;
typedef struct elf_String elf_String;

struct elf_String
{
	elf_Object  obj;
	elf_String *next;
	u32         hash;
	u16         id;
	u16         size;
	char        data[1];
};

elf_String *elf_atom_from_data_size(elf_State *state, const char *data, u32 size);
elf_String *elf_atom_from_data(elf_State *state, const char *data);
elf_String *elf_atom_from_data_size_id(elf_State *state, const char *data, u32 size, u16 id);
elf_String *elf_atom_from_data_id(elf_State *state, const char *data, u16 id);
u32 atom_size(elf_String *atom);
const char *atom_data(elf_String *atom);
u32 atom_hash(elf_String *atom);
b32 atoms_equal(elf_String *left, elf_String *right);
elf_StrSlice elf_atom_copy_text(Arena *arena, elf_String *atom);

#endif
