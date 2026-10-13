//
// See Copyright Notice In elf.h
//

#ifndef ELF_CORE_TABLE_H
#define ELF_CORE_TABLE_H

#define ENTRY_TOMB_TYPE 0xFFu

elf_Table *elf_table_new(elf_State *state);

void elf_table_set(elf_State *state, elf_Table *table, elf_Value key, elf_Value value);
elf_Value elf_table_get_or_nil(elf_State *state, elf_Table *table, elf_Value key);
b32 elf_table_contains(elf_State *state, elf_Table *table, elf_Value key);
b32 elf_table_delete(elf_State *state, elf_Table *table, elf_Value key, elf_Value *removed);
void elf_table_clear(elf_State *state, elf_Table *table);

static inline elf_ValueType entry_type(Entry entry)
{
	return entry.key.type;
}

static inline b32 entry_is_nil(Entry entry)
{
	return entry_type(entry) == ELF_VALUE_TYPE_NIL;
}

static inline b32 entry_is_tomb(Entry entry)
{
	return entry_type(entry) == ENTRY_TOMB_TYPE;
}

static inline b32 entry_is_dead(Entry entry)
{
	return entry_is_nil(entry) || entry_is_tomb(entry);
}

static inline b32 entry_is_key(Entry entry)
{
	return !entry_is_dead(entry);
}

static inline void entry_set_tomb(Entry *entry)
{
	entry->key = value_nil();
	entry->key.type = (elf_ValueType)ENTRY_TOMB_TYPE;
	entry->value = value_nil();
}

static inline Entry entry_from_key_value(elf_Value key, elf_Value value)
{
	Entry entry = {};
	entry.key = key;
	entry.value = value;
	return entry;
}

static inline elf_Value entry_key_value(Entry entry)
{
	return entry.key;
}

#endif
