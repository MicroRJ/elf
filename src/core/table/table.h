#ifndef ELF_CORE_TABLE_H
#define ELF_CORE_TABLE_H

typedef struct
{
	u64 key;
	u64 data;
}
Entry;
STATIC_ASSERT(sizeof(Entry) == 16);

#define ENTRY_INDEX_MASK       0x00000000FFFFFFFFull
#define ENTRY_TYPE_SHIFT       32
#define ENTRY_TYPE_MASK        0x000000FF00000000ull
#define ENTRY_READONLY_BIT     0x0000010000000000ull
#define ENTRY_TOMB_TYPE        0xFFu

static inline u32 entry_index(Entry entry)
{
	return (u32)(entry.data & ENTRY_INDEX_MASK);
}

static inline elf_ValueType entry_type(Entry entry)
{
	return (elf_ValueType)((entry.data & ENTRY_TYPE_MASK) >> ENTRY_TYPE_SHIFT);
}

static inline u32 entry_state(Entry entry)
{
	return (u32)((entry.data & ENTRY_TYPE_MASK) >> ENTRY_TYPE_SHIFT);
}

static inline b32 entry_is_nil(Entry entry)
{
	return entry_type(entry) == ELF_VALUE_TYPE_NIL;
}

static inline b32 entry_is_tomb(Entry entry)
{
	return entry_state(entry) == ENTRY_TOMB_TYPE;
}

static inline b32 entry_is_dead(Entry entry)
{
	return entry_is_nil(entry) || entry_is_tomb(entry);
}

static inline b32 entry_is_key(Entry entry)
{
	return !entry_is_dead(entry);
}

static inline b32 entry_is_readonly(Entry entry)
{
	return (entry.data & ENTRY_READONLY_BIT) != 0;
}

static inline void entry_set_index(Entry *entry, u32 index)
{
	entry->data = (entry->data & ~ENTRY_INDEX_MASK) | index;
}

static inline void entry_set_type(Entry *entry, elf_ValueType type)
{
	entry->data = (entry->data & ~ENTRY_TYPE_MASK) | ((u64)type << ENTRY_TYPE_SHIFT);
}

static inline void entry_set_tomb(Entry *entry)
{
	entry->data = (entry->data & ~ENTRY_TYPE_MASK) | ((u64)ENTRY_TOMB_TYPE << ENTRY_TYPE_SHIFT);
}

static inline void entry_mark_readonly(Entry *entry)
{
	entry->data |= ENTRY_READONLY_BIT;
}

static inline Entry entry_from_key_index(elf_Value key, u32 index)
{
	Entry entry = {};
	entry.key = (u64)key.x_i64;
	entry.data = ((u64)key.type << ENTRY_TYPE_SHIFT) | index;
	if (key.status & VALUE_READONLY) {
		entry.data |= ENTRY_READONLY_BIT;
	}
	return entry;
}

static inline elf_Value entry_key_value(Entry entry)
{
	elf_Value value = {};
	value.type = entry_type(entry);
	value.status = entry_is_readonly(entry) ? VALUE_READONLY : 0;
	value.x_i64 = (i64)entry.key;
	return value;
}

struct elf_Table
{
	elf_Object      obj;
	u32         ndebug;
	u32         fillcounter;
	u32         nentries;
	Entry      *entries;
	u32         count;
	u32         capacity;
	elf_Value  *array;
};

elf_Table *elf_table_new_unrooted_sized(elf_State *S, u32 nentries);
elf_Table *elf_table_new_unrooted(elf_State *S);

u32 elf_table_set(elf_State *S, elf_Table *tab, elf_Value key, elf_Value value);
elf_Value elf_table_get_or_nil(elf_State *S, elf_Table *tab, elf_Value key);
u32 elf_table_ensure(elf_State *S, elf_Table *tab, elf_Value key);
u32 elf_table_bind_to_index(elf_State *S, elf_Table *tab, elf_Value key, u32 index);
b32 elf_table_contains(elf_State *S, elf_Table *tab, elf_Value key);
void elf_table_alias(elf_State *S, elf_Table *tab, elf_Value key, elf_Value alias);

u32 elf_array_add(elf_State *S, elf_Table *tab, elf_Value value);
elf_Value elf_array_get(elf_State *S, elf_Table *tab, u32 index);
void elf_array_set(elf_State *S, elf_Table *tab, u32 index, elf_Value value);
void elf_array_remove(elf_State *S, elf_Table *tab, u32 index, u32 count);
void elf_array_swap(elf_State *S, elf_Table *tab, u32 left, u32 right);
u32 elf_array_len(elf_Table *tab);

void elf_table_mark_field_readonly(elf_State *S, elf_Table *tab, elf_Value field);

#endif
