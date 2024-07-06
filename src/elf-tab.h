/*
** See Copyright Notice In elf.h
** elf-tab.h
** Table
*/


typedef struct elEntry {
	elValue k;
	elInteger i;
} elEntry;


typedef struct elTable {
	elObject obj;
	elEntry *slots;
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

elInteger elf_table_take(elTable *table, elValue k);
void elf_table_insert(elTable *table, elValue k, elValue v);
elInteger elf_table_get_value_hash(elValue v);
elf_hashint elf_table_rehash(elf_hashint hash);
elf_hashint elf_tabhashstr(char *junk);
elf_hashint elf_tabhashptr(elAddr *ptr);
elBool elf_tabvaleq(elValue *x, elValue *y);


void elf_table_field_alias(elState *S, elTable *tab, char *key, elValue alias);
void elf_table_alias(elState *S, elTable *tab, elValue key, elValue alias);


/* metatable */
int elf_table_metatable_add(elState *);
int elf_table_metatable_index(elState *);
int elf_table_metatable_tally(elState *);
int elf_table_metatable_length(elState *);
int elf_table_metatable_xremove(elState *);
int elf_table_metatable_delete(elState *);
int elf_table_metatable_itemize(elState *);
int elf_table_metatable_inject(elState *);
int elf_table_metatable_alias(elState *);
int elf_table_metatable_contains(elState *);
int elf_table_metatable_lookup(elState *);
int elf_table_metatable_foreach(elState *);
int elf_table_metatable_get_collisions(elState *);
int elf_table_metatable_bubble_sort(elState *);
int elf_table_metatable_find_aliases(elState *);
int elf_table_metatable_merge(elState *);
int elf_table_metatable_clone(elState *);
