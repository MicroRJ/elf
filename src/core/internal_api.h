//
// See Copyright Notice In elf.h
//


void elf_init_raw(elf_State *);

tabID elf_alloc_table2(elf_State *, index_t nentries);
tabID elf_alloc_table(elf_State *);

strID elf_alloc_string2(elf_State *, elf_i32 length);
strID elf_alloc_string(elf_State *, const char *text);
strID elf_alloc_string3(elf_State *, char const *text, elf_i32 length);



elf_Closure *elf_alloc_closure(elf_State *S, elf_Proto proto);

void *elf_gc_alloc(elf_State *, elf_GC_Ty type, elf_i64 size);
int elf_get_global_slot(elf_State *S, elf_String *name);
int elf_set_global(elf_State *S, elf_String *name, elf_Value value);
// static void pushvalue(elf_State *S, elf_Value value);


// todo: rework this api, make separate paths for different
// lookups, such as strings...
void elf_tableK_recycle(tabID tab);
index_t elf_table_try_(tabID tab, elf_Value key);
index_t elf_table_try_text(tabID tab, const char *text, elf_i32 length, hash_t hash);
index_t elf_table_get_index_always_(tabID tab, elf_Value key);
elf_Value elf_table_get_raw(tabID tab, elf_Value key);
index_t elf_raw_table_set(tabID tab, elf_Value k, elf_Value v);
index_t elf_array_get_length(tabID tab);
index_t elf_raw_array_add(tabID tab, elf_Value thing);

// these are temporary!
void elf_error(elf_State *, int instr, const char *error);
void elf_errorf(elf_State *, int instr, const char *format, ...);
