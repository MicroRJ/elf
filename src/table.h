/*
** See Copyright Notice In elf.h
** table.h
** Table
*/



/* ---------------------------------
	Table
--------------------------------- */

typedef struct elEntry {
	union { elValue key, k; };
	union { elInteger index, i; };
} elEntry;


typedef struct elTable {
	elObject obj;
	elInteger ntotal;
	elInteger nslots;
	elInteger ncollisions;
	union { elEntry *entries, /* @DEPRECATED */ *slots;};
	union { elValue *values, /* @DEPRECATED */ *array;};
} elTable;


elTable *elf_new_table_metatable(elState *);
elTable *elf_new_ltable(elState *, elInteger);
elTable *elf_new_table(elState *);


/* todo: all of these have to revised,
there are a bunch of inconsistencies and
incoherences with this API. */
int elf_libH_get_metatable(elState *);
int elf_libH_set_metatable(elState *);

int elf_libH_add(elState *);
int elf_libH_xadd(elState *);
int elf_libH_xremove(elState *);
int elf_libH_xdelete(elState *);
int elf_libH_index(elState *);
int elf_libH_tally(elState *);
int elf_libH_length(elState *);
int elf_libH_delete(elState *);
int elf_libH_itemize(elState *);
int elf_libH_inject(elState *);
int elf_libH_alias(elState *);
int elf_libH_contains(elState *);
int elf_libH_lookup(elState *);
int elf_libH_foreach(elState *);
int elf_libH_get_collisions(elState *);
int elf_libH_bubble_sort(elState *);
int elf_libH_find_aliases(elState *);
int elf_libH_array(elState *);
int elf_libH_xset(elState *);
int elf_libH_merge(elState *);
int elf_libH_xmerge(elState *);
int elf_libH_diff(elState *);
int elf_libH_xclone(elState *);
int elf_libH_reverse(elState *);
int elf_libH_clone(elState *);
int elf_libH_slice(elState *);
int elf_libH_swap(elState *);


elGLOBAL elBinding elf_libH_[] = {
	{"get_metatable",elf_libH_get_metatable},
	{"set_metatable",elf_libH_set_metatable},
	{"length",elf_libH_length},
	{"tally",elf_libH_tally},
	{"delete",elf_libH_delete},
	{"haskey",elf_libH_contains},
	{"lookup",elf_libH_lookup},
	{"foreach",elf_libH_foreach},
	{"collisions",elf_libH_get_collisions},
	{"add",elf_libH_add},
	{"xadd",elf_libH_xadd},
	{"itemize",elf_libH_itemize},
	{"inject",elf_libH_inject},
	{"idx",elf_libH_index},
	{"xrem",elf_libH_xremove},
	{"xdelete",elf_libH_xdelete},
	{"bubblesort",elf_libH_bubble_sort},
	{"fndaliases",elf_libH_find_aliases},
	{"alias",elf_libH_alias},
	{"merge",elf_libH_merge},
	{"xmerge",elf_libH_xmerge},
	{"reverse",elf_libH_reverse},
	{"clone",elf_libH_clone},
	{"xclone",elf_libH_xclone},
	{"slice",elf_libH_slice},
	{"xset",elf_libH_xset},
	{"swap",elf_libH_swap},
	{"diff",elf_libH_diff},
};


elInteger elf_table_tryS(elTable *tab, char *contents, elInteger length, elHashId hash);
void elf_check_table(elTable *table);
void elf_dealloc_table(elTable *);
void elf_table_add(elTable *table, elValue v);
elBool elf_table_set(elTable *table, elValue k, elValue v);
elInteger elf_table_lookup_index(elTable *table, elValue k);
elInteger elf_table_get_value_hash(elValue v);
elHashId elf_table_rehash(elHashId hash);
elHashId elf_tabhashstr(char *junk);
elHashId elf_tabhashptr(elAddr *ptr);
elBool elf_tabvaleq(elValue *x, elValue *y);
void elf_table_field_alias(elState *S, elTable *tab, char *key, elValue alias);
void elf_table_alias(elState *S, elTable *tab, elValue key, elValue alias);
