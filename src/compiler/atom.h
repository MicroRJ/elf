//
// See Copyright Notice In elf.h
//

#ifndef ELF_COMPILER_ATOM_H
#define ELF_COMPILER_ATOM_H

typedef struct elf_Atom elf_Atom;
typedef struct AtomTable AtomTable;

struct elf_Atom
{
	elf_Atom *next;
	u32       hash;
	u32       size;
	u16       id;
	char      data[1];
};

struct AtomTable
{
	elf_Arena  *arena;
	elf_Atom **buckets;
	u32         bucket_count;
	u32         count;
};

static inline u32 atom_size(elf_Atom *atom)
{
	return atom->size;
}

static inline const char *atom_data(elf_Atom *atom)
{
	return atom->data;
}

static inline u32 atom_hash(elf_Atom *atom)
{
	return atom->hash;
}

static inline b32 atoms_equal(elf_Atom *left, elf_Atom *right)
{
	return left == right;
}

static void atom_table_init(AtomTable *table, elf_Arena *arena);
static elf_Atom *atom_from_data_size_id(AtomTable *table, const char *data, u32 size, u16 id);
static elf_Atom *atom_table_begin(AtomTable *table, u32 capacity);
static elf_Atom *atom_table_end(AtomTable *table, elf_Atom *candidate, u32 size);

static inline elf_Atom *atom_from_data_size(AtomTable *table, const char *data, u32 size)
{
	return atom_from_data_size_id(table, data, size, 0);
}

static inline elf_Atom *atom_from_data(AtomTable *table, const char *data)
{
	ASSERT(data);
	return atom_from_data_size(table, data, (u32)strlen(data));
}

static inline elf_Atom *atom_from_data_id(AtomTable *table, const char *data, u16 id)
{
	ASSERT(data);
	return atom_from_data_size_id(table, data, (u32)strlen(data), id);
}

static elf_String *elf_string_from_atom(elf_State *state, elf_Atom *atom);

#endif
