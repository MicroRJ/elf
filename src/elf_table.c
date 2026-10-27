//
// See Copyright Notice In elf.h
//

#define TABLE_INITIAL_CAPACITY 4u
#define TABLE_MAX_LOAD_NUMERATOR 3u
#define TABLE_MAX_LOAD_DENOMINATOR 4u

static inline b32 entry_is_nil(Entry entry)
{
	return entry.key.type == ELF_VALUE_TYPE_NIL;
}

static inline b32 entry_is_tomb(Entry entry)
{
	return entry.key.type == ENTRY_TOMB_TYPE;
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

static void table_check_write(elf_State *state, elf_Table *table)
{
	if (table->obj.status & ELF_OBJECT_READONLY)
	{
		report_runtime_error(state, ELF_ERROR_READONLY, "attempted to write to readonly object");
	}
}

elf_Table *elf_table_new(elf_State *state)
{
	elf_Table *table = elf_gc_alloc(state, ELF_OBJECT_TABLE, sizeof(*table));
	table->capacity = TABLE_INITIAL_CAPACITY;
	table->entries = elf_alloc_zero(state, (u64)table->capacity * sizeof(*table->entries));
	ASSERT(table->entries != 0);
	return table;
}

static inline u32 rehash(u64 hash)
{
	return (u32)(hash + (hash >> 6) + (hash >> 19));
}

static inline u32 hash64(u64 value)
{
	u32 hash = rehash(value);
	hash += hash << 16;
	hash ^= hash << 3;
	hash += hash >> 5;
	hash ^= hash << 2;
	hash += hash >> 15;
	hash ^= hash << 10;
	return rehash(hash);
}

static inline u32 table_hash_key(u64 key, u32 type)
{
	u32 type_hash = type * 0x9E3779B1u;
	if (type == ELF_VALUE_TYPE_STRING)
	{
		return ((elf_String *)key)->hash ^ type_hash;
	}
	return hash64(key) ^ type_hash;
}

static inline Entry *table_find_entry(Entry *entries, u32 capacity, elf_Value key)
{
	u64 key_bits = (u64)key.x_i64;
	u32 slot = table_hash_key(key_bits, key.type) & (capacity - 1);
	for (;;)
	{
		Entry *entry = entries + slot;
		if (entry_is_nil(*entry))
		{
			return 0;
		}
		if (entry->key.type == key.type && (u64)entry->key.x_i64 == key_bits)
		{
			return entry;
		}
		slot = (slot + 1) & (capacity - 1);
	}
}

static inline Entry *table_find_entry_for_write(Entry *entries, u32 capacity, elf_Value key)
{
	u64 key_bits = (u64)key.x_i64;
	u32 slot = table_hash_key(key_bits, key.type) & (capacity - 1);
	Entry *tomb = 0;
	for (;;)
	{
		Entry *entry = entries + slot;
		if (entry_is_nil(*entry))
		{
			return tomb ? tomb : entry;
		}
		if (entry_is_tomb(*entry))
		{
			if (!tomb)
			{
				tomb = entry;
			}
		}
		else if (entry->key.type == key.type && (u64)entry->key.x_i64 == key_bits)
		{
			return entry;
		}
		slot = (slot + 1) & (capacity - 1);
	}
}

static void table_resize(elf_State *state, elf_Table *table, u32 capacity)
{
	Entry *entries = elf_alloc_zero(state, (u64)capacity * sizeof(*entries));
	ASSERT(entries != 0);
	for (u32 i = 0; i < table->capacity; ++i)
	{
		Entry entry = table->entries[i];
		if (elf_table_entry_is_key(entry))
		{
			Entry *destination = table_find_entry_for_write(entries, capacity, entry.key);
			*destination = entry;
		}
	}
	elf_free(state, table->entries, (u64)table->capacity * sizeof(*table->entries));
	table->entries = entries;
	table->capacity = capacity;
	table->occupied = table->count;
}

static void table_ensure_capacity(elf_State *state, elf_Table *table)
{
	if ((u64)(table->occupied + 1) * TABLE_MAX_LOAD_DENOMINATOR <= (u64)table->capacity * TABLE_MAX_LOAD_NUMERATOR)
	{
		return;
	}
	u32 capacity = table->capacity;
	if ((u64)(table->count + 1) * TABLE_MAX_LOAD_DENOMINATOR > (u64)table->capacity * TABLE_MAX_LOAD_NUMERATOR)
	{
		capacity <<= 1;
	}
	table_resize(state, table, capacity);
}

b32 elf_table_contains(elf_State *state, elf_Table *table, elf_Value key)
{
	(void)state;
	return table_find_entry(table->entries, table->capacity, key) != 0;
}

elf_Value elf_table_get_or_nil(elf_State *state, elf_Table *table, elf_Value key)
{
	(void)state;
	Entry *entry = table_find_entry(table->entries, table->capacity, key);
	return entry ? entry->value : value_nil();
}

void elf_table_set(elf_State *state, elf_Table *table, elf_Value key, elf_Value value)
{
	table_check_write(state, table);
	if (value_is_nil(key))
	{
		report_runtime_error(state, ELF_ERROR_INVALID_TABLE_KEY, "table key cannot be nil");
		return;
	}
	table_ensure_capacity(state, table);
	Entry *entry = table_find_entry_for_write(table->entries, table->capacity, key);
	if (elf_table_entry_is_key(*entry))
	{
		entry->value = value;
	}
	else
	{
		if (entry_is_nil(*entry))
		{
			table->occupied += 1;
		}
		table->count += 1;
		*entry = entry_from_key_value(key, value);
	}
}

b32 elf_table_delete(elf_State *state, elf_Table *table, elf_Value key, elf_Value *removed)
{
	table_check_write(state, table);
	Entry *entry = table_find_entry(table->entries, table->capacity, key);
	if (!entry)
	{
		if (removed)
		{
			*removed = value_nil();
		}
		return false;
	}
	if (removed)
	{
		*removed = entry->value;
	}
	entry_set_tomb(entry);
	table->count -= 1;
	if (!table->count)
	{
		zero_memory(table->entries, (u64)table->capacity * sizeof(*table->entries));
		table->occupied = 0;
	}
	return true;
}

void elf_table_clear(elf_State *state, elf_Table *table)
{
	table_check_write(state, table);
	zero_memory(table->entries, (u64)table->capacity * sizeof(*table->entries));
	table->count = 0;
	table->occupied = 0;
}
