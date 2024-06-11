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


elTable *elf_newtabmetatab(elState *);
void elf_tabstralias(elState *S, elTable *tab, char *key, elValue alias);

void elf_deltab(elTable *);
elTable *elf_newtablen(elState *, elInteger);
elTable *elf_newtab(elState *);

elInteger elf_tabtake(elTable *table, elValue k);
void elf_tabset(elTable *table, elValue k, elValue v);
elInteger elf_tabhashval(elValue v);
elf_hashint elf_tabrehash(elf_hashint hash);
elf_hashint elf_tabhashstr(char *junk);
elf_hashint elf_tabhashptr(elAddr *ptr);
elBool elf_tabvaleq(elValue *x, elValue *y);


/* metatable */
int elf_tabadd_(elState *);
int elf_itemize_(elState *);
int elf_inject_(elState *);
int elf_tabdel_(elState *);
int elf_tabidx_(elState *);
int elf_tabdelete_(elState *);
int elf_tabxrem_(elState *);
int elf_tabalias_(elState *);
int elf_tabtally_(elState *);
int elf_tablength_(elState *);
int elf_tabhaskey_(elState *);
int elf_tablookup_(elState *);
int elf_tabiter_(elState *);
int elf_tabcollisions_(elState *);
int elf_tabbubblesort_(elState *);
int elf_tabfndaliases_(elState *);
int elf_tabmerge_(elState *);
int elf_tabclone_(elState *);
