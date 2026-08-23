//
// See Copyright Notice In elf.h
//

#ifndef ELF_COMPILER_ATOM_H
#define ELF_COMPILER_ATOM_H

typedef struct Atom Atom;
typedef struct Atom_Table Atom_Table;

struct Atom
{
	Atom   *next;
	u32     hash;
	u32     size;
	u16     id;
	char    data[1];
};

struct Atom_Table
{
	elf_Arena  *arena;
	Atom      **buckets;
	u32         bucket_count;
	u32         count;
};

static inline u32 atom_size(Atom *atom)
{
	return atom->size;
}

static inline const char *atom_data(Atom *atom)
{
	return atom->data;
}

static inline u32 atom_hash(Atom *atom)
{
	return atom->hash;
}

static inline b32 atoms_equal(Atom *left, Atom *right)
{
	return left == right;
}

static void atom_table_init(Atom_Table *table, elf_Arena *arena);
static Atom *atom_from_data_size_id(Atom_Table *table, const char *data, u32 size, u16 id);
static Atom *atom_table_begin(Atom_Table *table, u32 capacity);
static Atom *atom_table_end(Atom_Table *table, Atom *candidate, u32 size);

static inline Atom *atom_from_data_size(Atom_Table *table, const char *data, u32 size)
{
	return atom_from_data_size_id(table, data, size, 0);
}

static inline Atom *atom_from_data(Atom_Table *table, const char *data)
{
	ASSERT(data);
	return atom_from_data_size(table, data, (u32)strlen(data));
}

static inline Atom *atom_from_data_id(Atom_Table *table, const char *data, u16 id)
{
	ASSERT(data);
	return atom_from_data_size_id(table, data, (u32)strlen(data), id);
}

static elf_String *elf_string_from_atom(elf_State *state, Atom *atom);

#endif
