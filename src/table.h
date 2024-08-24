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


/* todo: tlib has to be revised, there are a bunch of
inconsistencies and incoherences with this API. */

elAPI elTable *elf_new_table_metatable(elState *);
elAPI elTable *elf_new_ltable(elState *, elInteger);
elAPI elTable *elf_new_table(elState *);
elAPI elInteger elf_get_table_length(elTable *table);

elAPI elInteger elf_tgeti(elTable *table, elValue k);
elAPI elValue elf_tgets_any(elTable *tab, elString *key);
elAPI elNumber elf_tgets_num(elTable *tab, elString *key);
elAPI elInteger elf_tgets_int(elTable *tab, elString *key);
elAPI elString *elf_tgets_str(elTable *tab, elString *key);
elAPI elTable *elf_tgets_tab(elTable *tab, elString *key);
elAPI elInteger elf_tgetsor_int(elTable *tab, elString *key, elInteger or);
elAPI void elf_tadd(elTable *table, elValue v);
elAPI elBool elf_tset(elTable *table, elValue k, elValue v);
elAPI void elf_tsets_num(elTable *tab, elString *key, elNumber val);
elAPI void elf_tsets_int(elTable *tab, elString *key, elInteger val);
elAPI void elf_tsets_str(elTable *tab, elString *key, elString *val);
elAPI void elf_tsets_tab(elTable *tab, elString *key, elTable *val);
elAPI void elf_tsetx_bindings(elState *R, elTable *tab, elCBinding *list, int num);
elAPI int elf_tlib_get_metatable(elState *);
elAPI int elf_tlib_set_metatable(elState *);
elAPI int elf_tlib_add(elState *);
elAPI int elf_tlib_xadd(elState *);
elAPI int elf_tlib_xremove(elState *);
elAPI int elf_tlib_xdelete(elState *);
elAPI int elf_tlib_index(elState *);
elAPI int elf_tlib_tally(elState *);
elAPI int elf_tlib_length(elState *);
elAPI int elf_tlib_delete(elState *);
elAPI int elf_tlib_itemize(elState *);
elAPI int elf_tlib_inject(elState *);
elAPI int elf_tlib_alias(elState *);
elAPI int elf_tlib_contains(elState *);
elAPI int elf_tlib_lookup(elState *);
elAPI int elf_tlib_foreach(elState *);
elAPI int elf_tlib_get_collisions(elState *);
elAPI int elf_tlib_bubble_sort(elState *);
elAPI int elf_tlib_find_aliases(elState *);
elAPI int elf_tlib_array(elState *);
elAPI int elf_tlib_xset(elState *);
elAPI int elf_tlib_merge(elState *);
elAPI int elf_tlib_xmerge(elState *);
elAPI int elf_tlib_diff(elState *);
elAPI int elf_tlib_xclone(elState *);
elAPI int elf_tlib_reverse(elState *);
elAPI int elf_tlib_clone(elState *);
elAPI int elf_tlib_slice(elState *);
elAPI int elf_tlib_swap(elState *);

elAPI elValue elf_table_lookup(elTable *tab, elValue k);
elAPI elInteger elf_ttry_text(elTable *tab, const char *text, elInteger length, elHashId hash);
elAPI void elf_check_table(elTable *table);
elAPI void elf_dealloc_table(elTable *);
elAPI elInteger elf_hash_value(elValue v);
elAPI elHashId elf_rehash(elHashId hash);
elAPI elHashId elf_hash_text(const char *text);
elAPI elHashId elf_hash_ptr(elAddr *ptr);
elAPI elBool elf_value_eq(elValue *x, elValue *y);
elAPI void elf_table_alias(elState *S, elTable *tab, elValue key, elValue alias);
