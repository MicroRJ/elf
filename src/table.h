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


elValue elf_tget_any(elTable *tab, elString *key);
elNumber elf_tget_num(elTable *tab, elString *key);
elInteger elf_tget_int(elTable *tab, elString *key);
elString *elf_tget_str(elTable *tab, elString *key);
elTable *elf_tget_tab(elTable *tab, elString *key);

/* array part */
void elf_tadd(elTable *table, elValue v);

elBool elf_table_set(elTable *table, elValue k, elValue v);
void elf_tset_num(elTable *tab, elString *key, elNumber val);
void elf_tset_int(elTable *tab, elString *key, elInteger val);
void elf_tset_str(elTable *tab, elString *key, elString *val);
void elf_tset_tab(elTable *tab, elString *key, elTable *val);



/* todo: all of these have to revised,
there are a bunch of inconsistencies and
incoherences with this API. */
int elf_tlib_get_metatable(elState *);
int elf_tlib_set_metatable(elState *);

int elf_tlib_add(elState *);
int elf_tlib_xadd(elState *);
int elf_tlib_xremove(elState *);
int elf_tlib_xdelete(elState *);
int elf_tlib_index(elState *);
int elf_tlib_tally(elState *);
int elf_tlib_length(elState *);
int elf_tlib_delete(elState *);
int elf_tlib_itemize(elState *);
int elf_tlib_inject(elState *);
int elf_tlib_alias(elState *);
int elf_tlib_contains(elState *);
int elf_tlib_lookup(elState *);
int elf_tlib_foreach(elState *);
int elf_tlib_get_collisions(elState *);
int elf_tlib_bubble_sort(elState *);
int elf_tlib_find_aliases(elState *);
int elf_tlib_array(elState *);
int elf_tlib_xset(elState *);
int elf_tlib_merge(elState *);
int elf_tlib_xmerge(elState *);
int elf_tlib_diff(elState *);
int elf_tlib_xclone(elState *);
int elf_tlib_reverse(elState *);
int elf_tlib_clone(elState *);
int elf_tlib_slice(elState *);
int elf_tlib_swap(elState *);


elGLOBAL elBinding elf_tlib_[] = {
	{"get_metatable",elf_tlib_get_metatable},
	{"set_metatable",elf_tlib_set_metatable},
	{"length",elf_tlib_length},
	{"tally",elf_tlib_tally},
	{"delete",elf_tlib_delete},
	{"haskey",elf_tlib_contains},
	{"lookup",elf_tlib_lookup},
	{"foreach",elf_tlib_foreach},
	{"collisions",elf_tlib_get_collisions},
	{"add",elf_tlib_add},
	{"xadd",elf_tlib_xadd},
	{"itemize",elf_tlib_itemize},
	{"inject",elf_tlib_inject},
	{"idx",elf_tlib_index},
	{"xrem",elf_tlib_xremove},
	{"xdelete",elf_tlib_xdelete},
	{"bubblesort",elf_tlib_bubble_sort},
	{"fndaliases",elf_tlib_find_aliases},
	{"alias",elf_tlib_alias},
	{"merge",elf_tlib_merge},
	{"xmerge",elf_tlib_xmerge},
	{"reverse",elf_tlib_reverse},
	{"clone",elf_tlib_clone},
	{"xclone",elf_tlib_xclone},
	{"slice",elf_tlib_slice},
	{"xset",elf_tlib_xset},
	{"swap",elf_tlib_swap},
	{"diff",elf_tlib_diff},
};


elValue elf_table_lookup(elTable *tab, elValue k);

elInteger elf_ttry_text(elTable *tab, char *contents, elInteger length, elHashId hash);
void elf_check_table(elTable *table);
void elf_dealloc_table(elTable *);

elInteger elf_table_lookup_index(elTable *table, elValue k);
elInteger elf_hash_value(elValue v);
elHashId elf_rehash(elHashId hash);
elHashId elf_hash_text(char *junk);
elHashId elf_hash_ptr(elAddr *ptr);
elBool elf_value_eq(elValue *x, elValue *y);
void elf_table_alias(elState *S, elTable *tab, elValue key, elValue alias);
