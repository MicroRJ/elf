/*
** See Copyright Notice In elf.h
** table.h
** Table
*/



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
int elf_libT_get_metatable(elState *);
int elf_libT_set_metatable(elState *);

int elf_libT_add(elState *);
int elf_libT_xadd(elState *);
int elf_libT_xremove(elState *);
int elf_libT_xdelete(elState *);
int elf_libT_index(elState *);
int elf_libT_tally(elState *);
int elf_libT_length(elState *);
int elf_libT_delete(elState *);
int elf_libT_itemize(elState *);
int elf_libT_inject(elState *);
int elf_libT_alias(elState *);
int elf_libT_contains(elState *);
int elf_libT_lookup(elState *);
int elf_libT_foreach(elState *);
int elf_libT_get_collisions(elState *);
int elf_libT_bubble_sort(elState *);
int elf_libT_find_aliases(elState *);
int elf_libT_array(elState *);
int elf_libT_xset(elState *);
int elf_libT_merge(elState *);
int elf_libT_xmerge(elState *);
int elf_libT_diff(elState *);
int elf_libT_xclone(elState *);
int elf_libT_reverse(elState *);
int elf_libT_clone(elState *);
int elf_libT_slice(elState *);
int elf_libT_swap(elState *);


elGLOBAL elBinding elf_libT_[] = {
	{"get_metatable",elf_libT_get_metatable},
	{"set_metatable",elf_libT_set_metatable},
	{"length",elf_libT_length},
	{"tally",elf_libT_tally},
	{"delete",elf_libT_delete},
	{"haskey",elf_libT_contains},
	{"lookup",elf_libT_lookup},
	{"foreach",elf_libT_foreach},
	{"collisions",elf_libT_get_collisions},
	{"add",elf_libT_add},
	{"xadd",elf_libT_xadd},
	{"itemize",elf_libT_itemize},
	{"inject",elf_libT_inject},
	{"idx",elf_libT_index},
	{"xrem",elf_libT_xremove},
	{"xdelete",elf_libT_xdelete},
	{"bubblesort",elf_libT_bubble_sort},
	{"fndaliases",elf_libT_find_aliases},
	{"alias",elf_libT_alias},
	{"merge",elf_libT_merge},
	{"xmerge",elf_libT_xmerge},
	{"reverse",elf_libT_reverse},
	{"clone",elf_libT_clone},
	{"xclone",elf_libT_xclone},
	{"slice",elf_libT_slice},
	{"xset",elf_libT_xset},
	{"swap",elf_libT_swap},
	{"diff",elf_libT_diff},
};


elInteger elf_table_tryS(elTable *tab, char *contents, elInteger length, elHashId hash);
void elf_check_table(elTable *table);
void elf_dealloc_table(elTable *);
void elf_tadd(elTable *table, elValue v);
elBool elf_table_set(elTable *table, elValue k, elValue v);
elInteger elf_table_lookup_index(elTable *table, elValue k);
elInteger elf_table_get_value_hash(elValue v);
elHashId elf_table_rehash(elHashId hash);
elHashId elf_tabhashstr(char *junk);
elHashId elf_tabhashptr(elAddr *ptr);
elBool elf_tabvaleq(elValue *x, elValue *y);
void elf_table_field_alias(elState *S, elTable *tab, char *key, elValue alias);
void elf_table_alias(elState *S, elTable *tab, elValue key, elValue alias);
