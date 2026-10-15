#ifndef ELF_COMPILER_ATOM_VIEW_H
#define ELF_COMPILER_ATOM_VIEW_H

// Immutable compiler string stored by the atom table.
typedef struct Atom Atom;
struct Atom
{
	Atom *next;
	u32   hash;
	u32   size;
	u16   id;
	char  data[1];
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

#endif
