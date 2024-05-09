/*
** See Copyright Notice In elf.h
** elf-tab.h
** Table
*/


typedef struct elf_tabslot {
	elf_Value k;
	elf_int i;
} elf_tabslot;


typedef struct elf_Table {
	elf_Object obj;
	elf_tabslot *slots;
	elf_int ntotal;
	elf_int nslots;
	elf_int ncollisions;
	/* array object */
	union {elf_Value *v,*array;};
} elf_Table;


elf_Table *elf_newtabmetatab(elf_State *);
void elf_tabstralias(elf_State *S, elf_Table *tab, char *key, elf_Value alias);

void elf_deltab(elf_Table *);
elf_Table *elf_newtablen(elf_State *, elf_int);
elf_Table *elf_newtab(elf_State *);

elf_int elf_tabtake(elf_Table *table, elf_Value k);
void elf_tabset(elf_Table *table, elf_Value k, elf_Value v);
elf_int elf_tabhashval(elf_Value v);
elf_hashint elf_tabrehash(elf_hashint hash);
elf_hashint elf_tabhashstr(char *junk);
elf_hashint elf_tabhashptr(Ptr *ptr);
elf_bool elf_tabvaleq(elf_Value *x, elf_Value *y);


/* metatable */
int elf_tabadd_(elf_State *);
int elf_tabidx_(elf_State *);
int elf_tabxrem_(elf_State *);
int elf_tabalias_(elf_State *);
int elf_tabtally_(elf_State *);
int elf_tablength_(elf_State *);
int elf_tabunload_(elf_State *);
int elf_tabhaskey_(elf_State *);
int elf_tablookup_(elf_State *);
int elf_tabforeach_(elf_State *);
int elf_tabcollisions_(elf_State *);
