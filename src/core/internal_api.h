//
// See Copyright Notice In elf.h
//


void elf_init_raw(elf_State *);

Table newtable2(elf_State *, Index nentries);
Table newtable(elf_State *);

String elf_alloc_string2(elf_State *, int length);
String elf_alloc_string3(elf_State *, char const *text, int length);
String elf_alloc_string(elf_State *, const char *text);



elf_Closure *elf_alloc_closure(elf_State *S, Proto proto);

void *elf_gc_alloc(elf_State *, GCType type, elf_i64 size);
int elf_get_global_slot(elf_State *S, elf_String *name);
int elf_set_global(elf_State *S, elf_String *name, V value);
// static void pushvalueunsafe(elf_State *S, V value);


// todo: rework this api, make separate paths for different
// lookups, such as strings...
void recycletable(Table tab);
Index tabletry(Table tab, V key);
Index elf_table_get_index_always_(Table tab, V key);

Index tableset(Table tab, V k, V v);
Index arrayadd(Table tab, V thing);

// these are temporary!
void elf_error(elf_State *, int instr, const char *error);
void elf_errorf(elf_State *, int instr, const char *format, ...);
