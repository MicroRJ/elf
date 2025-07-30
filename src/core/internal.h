
//
// See Copyright Notice In elf.h
//

// todo: @deprecated
elf_Closure *elf_new_closure(elf_State *, elf_Proto proto);
elf_String *elf_new_string2(elf_State *, elf_i32 length);
elf_String *elf_new_string(elf_State *, const char *text);
elf_Table *elf_new_table(elf_State *);


void *elf_gc_alloc(elf_State *, elf_GC_Ty type, elf_i64 size);
elf_Closure *elf_alloc_closure(elf_State *S, elf_Proto proto);
int elf_get_global_slot(elf_State *S, elf_String *name);
int elf_set_global(elf_State *S, elf_String *name, elf_Value value);
void elf_push_value_raw(elf_State *S, elf_Value value);
void elf_push_object_raw(elf_State *S, elf_Object *);
void elf_push_string_raw(elf_State *S, elf_String *);
void elf_push_table_raw(elf_State *S, elf_Table *);
void elf_push_closure_raw(elf_State *S, elf_Closure *);
elf_String *elf_alloc_string2(elf_State *S, elf_i32 length);
elf_String *elf_alloc_string(elf_State *S, const char *text);
int  elf_get_string_length(elf_String *);
elf_HashInt  elf_get_string_hash(elf_String *);

elf_Table *elf_alloc_table2(elf_State *, elf_IndexInt nentries);
elf_Table *elf_alloc_table(elf_State *);

void elf_table_alias(elf_Table *tab, elf_Value key, elf_Value alias);
void elf_table_merge(elf_Table *tab, elf_Table *merger);
void elf_table_recycle(elf_Table *tab);
elf_IndexInt elf_table_try(elf_Table *tab, elf_Value key);
elf_IndexInt elf_table_try_text(elf_Table *tab, const char *text, elf_i32 length, elf_HashInt hash);
elf_IndexInt elf_table_get_or_add_raw(elf_Table *tab, elf_Value key);
elf_Value elf_table_get_raw(elf_Table *tab, elf_Value key);
elf_b32 elf_table_set_raw(elf_Table *tab, elf_Value k, elf_Value v);
elf_IndexInt elf_array_get_length(elf_Table *tab);
void elf_array_add_raw(elf_Table *tab, elf_Value thing);
