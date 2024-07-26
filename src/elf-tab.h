/*
** See Copyright Notice In elf.h
** elf-tab.h
** Table
*/


typedef struct elEntry {
	union { elValue key, k; };
	union { elInteger index, i; };
} elEntry;


typedef struct elTable {
	elObject obj;
	union { elEntry *entries, *slots; };
	elInteger ntotal;
	elInteger nslots;
	elInteger ncollisions;
	/* array object */
	union {elValue *v,*array;};
} elTable;


elTable *elf_new_table_metatable(elState *);

elTable *elf_new_table_of_length(elState *, elInteger);
elTable *elf_new_table(elState *);
void elf_dealloc_table(elTable *);

elBool elf_table_set(elTable *table, elValue k, elValue v);
elInteger elf_table_lookup_index(elTable *table, elValue k);
elInteger elf_table_get_value_hash(elValue v);
elHashId elf_table_rehash(elHashId hash);
elHashId elf_tabhashstr(char *junk);
elHashId elf_tabhashptr(elAddr *ptr);
elBool elf_tabvaleq(elValue *x, elValue *y);


void elf_table_field_alias(elState *S, elTable *tab, char *key, elValue alias);
void elf_table_alias(elState *S, elTable *tab, elValue key, elValue alias);


/* metatable */
int elf_table_libfn_add(elState *);
int elf_table_libfn_xadd(elState *);
int elf_table_libfn_xremove(elState *);
int elf_table_libfn_xdelete(elState *);
int elf_table_libfn_index(elState *);
int elf_table_libfn_tally(elState *);
int elf_table_libfn_length(elState *);
int elf_table_libfn_delete(elState *);
int elf_table_libfn_itemize(elState *);
int elf_table_libfn_inject(elState *);
int elf_table_libfn_alias(elState *);
int elf_table_libfn_contains(elState *);
int elf_table_libfn_lookup(elState *);
int elf_table_libfn_foreach(elState *);
int elf_table_libfn_get_collisions(elState *);
int elf_table_libfn_bubble_sort(elState *);
int elf_table_libfn_find_aliases(elState *);
int elf_table_libfn_array(elState *R);
int elf_table_libfn_xset(elState *R);
int elf_table_libfn_merge(elState *);
int elf_table_libfn_xmerge(elState *);
int elf_table_libfn_diff(elState *R);
int elf_table_libfn_xclone(elState *);
int elf_table_libfn_reverse(elState *);
int elf_table_libfn_clone(elState *);
int elf_table_libfn_slice(elState *R);
int elf_table_libfn_swap(elState *R);
