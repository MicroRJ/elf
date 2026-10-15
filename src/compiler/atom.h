//
// See Copyright Notice In elf.h
//

#ifndef ELF_COMPILER_ATOM_H
#define ELF_COMPILER_ATOM_H

#include "atom_view.h"

typedef struct Atom_Table Atom_Table;

struct Atom_Table
{
	elf_Arena  *arena;
	Atom      **buckets;
	u32         bucket_count;
	u32         count;
};

static void atom_table_init(Atom_Table *table, elf_Arena *arena);
static Atom *atom_from_data_size_id(Atom_Table *table, const char *data, u32 size, u16 id);

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


#endif
