//
// See Copyright Notice In elf.h
//

#define GLOBAL_INITIAL_CAPACITY 16u
#define GLOBAL_MAX_LOAD_NUMERATOR 3u
#define GLOBAL_MAX_LOAD_DENOMINATOR 4u

struct GlobalEntry
{
	char *name;
	u64   size;
	u32   hash;
	u32   slot;
};

static u32 global_name_hash(elf_StrSlice name)
{
	ASSERT(name.data || !name.size);
	u32 hash = 2166136261u;
	for (u64 i = 0; i < name.size; ++i)
	{
		hash ^= (u8)name.data[i];
		hash *= 16777619u;
	}
	return hash;
}

static GlobalEntry *global_find_entry(GlobalEntry *entries, u32 capacity, elf_StrSlice name, u32 hash)
{
	if (!capacity)
	{
		return 0;
	}

	u32 slot = hash & (capacity - 1);
	for (;;)
	{
		GlobalEntry *entry = entries + slot;
		if (!entry->name)
		{
			return entry;
		}
		if (entry->hash == hash && entry->size == name.size && (!name.size || !memcmp(entry->name, name.data, (size_t)name.size)))
		{
			return entry;
		}
		slot = (slot + 1) & (capacity - 1);
	}
}

static void globals_resize_entries(elf_Globals *globals, u32 capacity)
{
	u64 entry_bytes = (u64)capacity * sizeof(*globals->entries);
	GlobalEntry *entries = elf_alloc_zero(globals->state, entry_bytes);
	ASSERT(entries != 0);

	for (u32 i = 0; i < globals->entry_capacity; ++i)
	{
		GlobalEntry entry = globals->entries[i];
		if (entry.name)
		{
			elf_StrSlice name = {entry.name, entry.size};
			GlobalEntry *destination = global_find_entry(entries, capacity, name, entry.hash);
			*destination = entry;
		}
	}

	elf_free(globals->state, globals->entries, (u64)globals->entry_capacity * sizeof(*globals->entries));
	globals->entries = entries;
	globals->entry_capacity = capacity;
}

static void globals_ensure_entry_capacity(elf_Globals *globals)
{
	if (globals->entry_capacity && (u64)(globals->count + 1) * GLOBAL_MAX_LOAD_DENOMINATOR <= (u64)globals->entry_capacity * GLOBAL_MAX_LOAD_NUMERATOR)
	{
		return;
	}
	u32 capacity = globals->entry_capacity ? globals->entry_capacity << 1 : GLOBAL_INITIAL_CAPACITY;
	globals_resize_entries(globals, capacity);
}

static void globals_ensure_value_capacity(elf_Globals *globals)
{
	if (globals->count < globals->value_capacity)
	{
		return;
	}
	u32 capacity = globals->value_capacity ? globals->value_capacity << 1 : GLOBAL_INITIAL_CAPACITY;
	u64 old_bytes = (u64)globals->value_capacity * sizeof(*globals->values);
	u64 new_bytes = (u64)capacity * sizeof(*globals->values);
	elf_Value *values = elf_realloc(globals->state, globals->values, old_bytes, new_bytes);
	ASSERT(values != 0);
	value_zero_many(values + globals->value_capacity, capacity - globals->value_capacity);
	globals->values = values;
	globals->value_capacity = capacity;
}

void elf_globals_init(elf_Globals *globals, elf_State *state)
{
	zero_memory(globals, sizeof(*globals));
	globals->state = state;
}

void elf_globals_destroy(elf_Globals *globals)
{
	for (u32 i = 0; i < globals->entry_capacity; ++i)
	{
		GlobalEntry *entry = globals->entries + i;
		if (entry->name)
		{
			elf_free(globals->state, entry->name, entry->size + 1);
		}
	}
	elf_free(globals->state, globals->entries, (u64)globals->entry_capacity * sizeof(*globals->entries));
	elf_free(globals->state, globals->values, (u64)globals->value_capacity * sizeof(*globals->values));
	zero_memory(globals, sizeof(*globals));
}

b32 elf_globals_find(elf_Globals *globals, elf_StrSlice name, u32 *slot)
{
	u32 hash = global_name_hash(name);
	GlobalEntry *entry = global_find_entry(globals->entries, globals->entry_capacity, name, hash);
	if (!entry || !entry->name)
	{
		return false;
	}
	if (slot)
	{
		*slot = entry->slot;
	}
	return true;
}

u32 elf_globals_ensure(elf_Globals *globals, elf_StrSlice name)
{
	u32 hash = global_name_hash(name);
	GlobalEntry *entry = global_find_entry(globals->entries, globals->entry_capacity, name, hash);
	if (entry && entry->name)
	{
		return entry->slot;
	}

	globals_ensure_entry_capacity(globals);
	globals_ensure_value_capacity(globals);
	entry = global_find_entry(globals->entries, globals->entry_capacity, name, hash);
	ASSERT(entry && !entry->name);

	char *name_copy = elf_alloc(globals->state, name.size + 1);
	ASSERT(name_copy != 0);
	if (name.size)
	{
		copy_memory(name_copy, name.data, name.size);
	}
	name_copy[name.size] = 0;

	u32 slot = globals->count++;
	entry->name = name_copy;
	entry->size = name.size;
	entry->hash = hash;
	entry->slot = slot;
	globals->values[slot] = value_nil();
	return slot;
}

elf_u32 elf_ensure_global_symbol(elf_State *state, elf_StrSlice name)
{
	ASSERT(state);
	return elf_globals_ensure(&state->globals, name);
}

elf_Value elf_globals_get_by_name(elf_Globals *globals, elf_StrSlice name)
{
	u32 slot = 0;
	return elf_globals_find(globals, name, &slot) ? globals->values[slot] : value_nil();
}

void elf_globals_set_by_name(elf_Globals *globals, elf_StrSlice name, elf_Value value)
{
	u32 slot = elf_globals_ensure(globals, name);
	globals->values[slot] = value;
}

elf_Value elf_globals_get_by_slot(elf_Globals *globals, u32 slot)
{
	ASSERT(slot < globals->count);
	return globals->values[slot];
}

void elf_globals_set_by_slot(elf_Globals *globals, u32 slot, elf_Value value)
{
	ASSERT(slot < globals->count);
	globals->values[slot] = value;
}
