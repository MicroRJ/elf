/*
** See Copyright Notice In elf.h
** table.h
** Table
*/




elAPI elf_Table *new_table_lib(elf_Shell *);
elAPI elf_Table *elf_alloc_table2(elf_Shell *, elf_Int);
elAPI elf_Table *elf_alloc_table(elf_Shell *);
elAPI elf_Int elf_get_table_length(elf_Table *table);

elAPI elf_Int elf_tgeti(elf_Table *table, elf_Value k);
elAPI elf_Value elf_tgets_any(elf_Table *tab, elf_String *key);
elAPI elf_Value elf_tgetx_any(elf_Table *tab, char const *key);
elAPI elf_Num elf_tgets_num(elf_Table *tab, elf_String *key);
elAPI elf_Int elf_tgets_int(elf_Table *tab, elf_String *key);
elAPI elf_String *elf_tgets_str(elf_Table *tab, elf_String *key);
elAPI elf_Table *elf_tgets_tab(elf_Table *tab, elf_String *key);
elAPI elf_Int elf_tgetsor_int(elf_Table *tab, elf_String *key, elf_Int or);
elAPI void elf_tadd(elf_Table *table, elf_Value v);
elAPI elf_Bool elf_tset(elf_Table *table, elf_Value k, elf_Value v);
elAPI void elf_tsets_num(elf_Table *tab, elf_String *key, elf_Num val);
elAPI void elf_tsets_int(elf_Table *tab, elf_String *key, elf_Int val);
elAPI void elf_tsets_str(elf_Table *tab, elf_String *key, elf_String *val);
elAPI void elf_tsets_tab(elf_Table *tab, elf_String *key, elf_Table *val);
elAPI void elf_tsetx_bindings(elf_Shell *R, elf_Table *tab, elf_CBinding *list, int num);

elAPI void elf_merge_tables(elf_Table *tab, elf_Table *merger);

elAPI elf_Value elf_table_lookup(elf_Table *tab, elf_Value k);
elAPI elf_Int elf_ttry_text(elf_Table *tab, const char *text, elf_Int length, elf_Hash hash);
elAPI void elf_check_table(elf_Table *table);
elAPI void elf_dealloc_table(elf_Table *);
elAPI elf_Int elf_hash_value(elf_Value v);
elAPI elf_Hash elf_rehash(elf_Hash hash);
elAPI elf_Hash elf_hash_text(const char *text);
elAPI elf_Hash elf_hash_ptr(void *ptr);
elAPI elf_Bool elf_value_eq(elf_Value *x, elf_Value *y);
elAPI void elf_table_alias(elf_Shell *S, elf_Table *tab, elf_Value key, elf_Value alias);
