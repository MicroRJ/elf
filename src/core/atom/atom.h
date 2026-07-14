#ifndef ELF_CORE_ATOM_H
#define ELF_CORE_ATOM_H

typedef struct elf_State elf_State;
typedef struct elf_Atom elf_Atom;

struct elf_Atom
{
	elf_Object obj;
	elf_Atom *next;
	u32       hash;
	u16       id;
	u16       size;
	char      data[1];
};

elf_Atom *elf_atom_from_data_size(elf_State *state, const char *data, u32 size);
elf_Atom *elf_atom_from_data(elf_State *state, const char *data);
elf_Atom *elf_atom_from_data_size_id(elf_State *state, const char *data, u32 size, u16 id);
elf_Atom *elf_atom_from_data_id(elf_State *state, const char *data, u16 id);
u32 elf_atom_size(elf_Atom *atom);
const char *elf_atom_data(elf_Atom *atom);
u32 elf_atom_hash(elf_Atom *atom);
bool elf_atoms_equal(elf_Atom *left, elf_Atom *right);

#endif
