//
// See Copyright Notice In elf.h
//


void elf_init_raw(elf_State *);

tabID elf_alloc_table2(elf_State *, elf_Index nentries);
tabID elf_alloc_table(elf_State *);

strID elf_alloc_string2(elf_State *, elf_i32 length);
strID elf_alloc_string3(elf_State *, char const *text, elf_i32 length);
strID elf_alloc_string(elf_State *, const char *text);



elf_Closure *elf_alloc_closure(elf_State *S, Proto proto);

void *elf_gc_alloc(elf_State *, elf_GC_Ty type, elf_i64 size);
int elf_get_global_slot(elf_State *S, elf_String *name);
int elf_set_global(elf_State *S, elf_String *name, V value);
// static void pushvalueunsafe(elf_State *S, V value);


// todo: rework this api, make separate paths for different
// lookups, such as strings...
void elf_tableK_recycle(tabID tab);
elf_Index elf_table_try_(tabID tab, V key);
elf_Index elf_table_try_text(tabID tab, const char *text, elf_i32 length, hash_t hash);
elf_Index elf_table_get_index_always_(tabID tab, V key);
V elf_table_get_raw(tabID tab, V key);

elf_Index tableset(tabID tab, V k, V v);
elf_Index arrayadd(tabID tab, V thing);

// these are temporary!
void elf_error(elf_State *, int instr, const char *error);
void elf_errorf(elf_State *, int instr, const char *format, ...);
