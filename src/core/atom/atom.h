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

elf_String *elf_atom_from_data_size_id(elf_State *state, const char *data, u32 size, u16 id);

static inline u32 atom_size(elf_String *atom)
{
	return atom->size;
}

static inline const char *atom_data(elf_String *atom)
{
	return atom->data;
}

static inline u32 atom_hash(elf_String *atom)
{
	return atom->hash;
}

static inline b32 atoms_equal(elf_String *left, elf_String *right)
{
	return left == right;
}

static elf_String *elf_atom_from_data_size(elf_State *state, const char *data, u32 size)
{
	return elf_atom_from_data_size_id(state, data, size, 0);
}

static elf_String *elf_atom_from_data(elf_State *state, const char *data)
{
	ASSERT(data);
	return elf_atom_from_data_size(state, data, (u32)strlen(data));
}

static elf_String *elf_atom_from_data_id(elf_State *state, const char *data, u16 id)
{
	ASSERT(data);
	return elf_atom_from_data_size_id(state, data, (u32)strlen(data), id);
}

#endif
