//
// See Copyright Notice In elf.h
//

#define GLOBAL_INITIAL_CAPACITY 16u
#define GLOBAL_MAX_LOAD_NUMERATOR 3u
#define GLOBAL_MAX_LOAD_DENOMINATOR 4u

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

static void globals_resize_entries(elf_State *state, u32 capacity)
{
	u64 entry_bytes = (u64)capacity * sizeof(*state->global_entries);
	GlobalEntry *entries = elf_alloc_zero(state, entry_bytes);
	ASSERT(entries != 0);

	for (u32 i = 0; i < state->global_entry_capacity; ++i)
	{
		GlobalEntry entry = state->global_entries[i];
		if (entry.name)
		{
			elf_StrSlice name = {entry.name, entry.size};
			GlobalEntry *destination = global_find_entry(entries, capacity, name, entry.hash);
			*destination = entry;
		}
	}

	elf_free(state, state->global_entries, (u64)state->global_entry_capacity * sizeof(*state->global_entries));
	state->global_entries = entries;
	state->global_entry_capacity = capacity;
}

static void globals_ensure_entry_capacity(elf_State *state)
{
	if (state->global_entry_capacity && (u64)(state->global_count + 1) * GLOBAL_MAX_LOAD_DENOMINATOR <= (u64)state->global_entry_capacity * GLOBAL_MAX_LOAD_NUMERATOR)
	{
		return;
	}
	u32 capacity = state->global_entry_capacity ? state->global_entry_capacity << 1 : GLOBAL_INITIAL_CAPACITY;
	globals_resize_entries(state, capacity);
}

static void globals_ensure_value_capacity(elf_State *state)
{
	if (state->global_count < state->global_value_capacity)
	{
		return;
	}
	u32 capacity = state->global_value_capacity ? state->global_value_capacity << 1 : GLOBAL_INITIAL_CAPACITY;
	u64 old_bytes = (u64)state->global_value_capacity * sizeof(*state->global_values);
	u64 new_bytes = (u64)capacity * sizeof(*state->global_values);
	elf_Value *values = elf_realloc(state, state->global_values, old_bytes, new_bytes);
	ASSERT(values != 0);
	value_zero_many(values + state->global_value_capacity, capacity - state->global_value_capacity);
	state->global_values = values;
	state->global_value_capacity = capacity;
}

void elf_globals_destroy(elf_State *state)
{
	for (u32 i = 0; i < state->global_entry_capacity; ++i)
	{
		GlobalEntry *entry = state->global_entries + i;
		if (entry->name)
		{
			elf_free(state, entry->name, entry->size + 1);
		}
	}
	elf_free(state, state->global_entries, (u64)state->global_entry_capacity * sizeof(*state->global_entries));
	elf_free(state, state->global_values, (u64)state->global_value_capacity * sizeof(*state->global_values));
	state->global_entries = 0;
	state->global_entry_capacity = 0;
	state->global_values = 0;
	state->global_value_capacity = 0;
	state->global_count = 0;
}

b32 elf_globals_find(elf_State *state, elf_StrSlice name, u32 *slot)
{
	u32 hash = global_name_hash(name);
	GlobalEntry *entry = global_find_entry(state->global_entries, state->global_entry_capacity, name, hash);
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

u32 elf_globals_ensure(elf_State *state, elf_StrSlice name)
{
	u32 hash = global_name_hash(name);
	GlobalEntry *entry = global_find_entry(state->global_entries, state->global_entry_capacity, name, hash);
	if (entry && entry->name)
	{
		return entry->slot;
	}

	globals_ensure_entry_capacity(state);
	globals_ensure_value_capacity(state);
	entry = global_find_entry(state->global_entries, state->global_entry_capacity, name, hash);
	ASSERT(entry && !entry->name);

	char *name_copy = elf_alloc(state, name.size + 1);
	ASSERT(name_copy != 0);
	if (name.size)
	{
		copy_memory(name_copy, name.data, name.size);
	}
	name_copy[name.size] = 0;

	u32 slot = state->global_count++;
	entry->name = name_copy;
	entry->size = name.size;
	entry->hash = hash;
	entry->slot = slot;
	state->global_values[slot] = value_nil();
	return slot;
}

elf_Value elf_globals_get_by_name(elf_State *state, elf_StrSlice name)
{
	u32 slot = 0;
	return elf_globals_find(state, name, &slot) ? state->global_values[slot] : value_nil();
}

void elf_globals_set_by_name(elf_State *state, elf_StrSlice name, elf_Value value)
{
	u32 slot = elf_globals_ensure(state, name);
	state->global_values[slot] = value;
}

elf_Value elf_globals_get_by_slot(elf_State *state, u32 slot)
{
	ASSERT(slot < state->global_count);
	return state->global_values[slot];
}

void elf_globals_set_by_slot(elf_State *state, u32 slot, elf_Value value)
{
	ASSERT(slot < state->global_count);
	state->global_values[slot] = value;
}
