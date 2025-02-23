/*
** See Copyright Notice In elf.h
** elf.h
*/

elf_Int elf_table_try(elf_Table *tab, elf_Value key);
elf_Int elf_table_try_text(elf_Table *tab, const char *text, elf_Int length, elf_Hash hash);
elf_Int elf_table_get_or_add(elf_Table *table, elf_Value key);
elf_Value elf_table_get(elf_Table *tab, elf_Value key);
elf_Int elf_get_array_tally(elf_Table *table);
void elf_array_add(elf_Table *table, elf_Value thing);
bool elf_table_set(elf_Table *table, elf_Value k, elf_Value v);
