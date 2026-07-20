//
// See Copyright Notice In elf.h
//

#define TABLE_INITIAL_ENTRY_COUNT   4
#define TABLE_INITIAL_ARRAY_COUNT   8
#define TABLE_MAX_LOAD_NUMERATOR    3
#define TABLE_MAX_LOAD_DENOMINATOR  4
#define TABLE_SLOT_NOT_FOUND        ((u32)(-1))


static void check_write(elf_State *state, elf_Object *reference)
{
	if (reference->status & ELF_OBJECT_READONLY) {
		elf_report_runtime_error(state, RUNTIME_ERROR_GENERIC, -1, "attempted to write to readonly object");
	}
}

static inline void table_init(elf_Table *table, u32 nentries)
{
	table->fillcounter = 0;
	table->entries = calloc(1, nentries * sizeof(*table->entries));
	table->nentries = nentries;
	table->array = 0;
	table->count = 0;
	table->capacity = 0;
}

static void table_array_reserve(elf_Table *table, u32 min_capacity)
{
	if (table->capacity >= min_capacity) {
		return;
	}

	u32 capacity = table->capacity ? table->capacity : TABLE_INITIAL_ARRAY_COUNT;
	while (capacity < min_capacity) {
		capacity <<= 1;
	}

	table->array = realloc(table->array, capacity * sizeof(*table->array));
	ASSERT(table->array != 0);

	value_zero_many(table->array + table->capacity, capacity - table->capacity);
	table->capacity = capacity;
}

static elf_Value *table_array_slot(elf_State *state, elf_Table *table, u32 index)
{
	check_array_index(state, NO_BYTE, index, table->count);

	return &table->array[index];
}

static void table_array_remove_unchecked(elf_Table *table, u32 index, u32 count)
{
	ASSERT(index <= table->count);
	ASSERT(count <= table->count - index);

	u32 tail_count = table->count - index - count;
	memmove(table->array + index, table->array + index + count, tail_count * sizeof(*table->array));
	table->count -= count;
	value_zero_many(table->array + table->count, count);
}

elf_Table *elf_new_table_rogue2(elf_State *state, u32 nentries)
{
	elf_Table *table = elf_gc_alloc(state, ELF_OBJECT_TABLE, sizeof(*table));

	table_init(table, nentries);
	return table;
}

elf_Table *elf_new_table_rogue(elf_State *state)
{
	return elf_new_table_rogue2(state, 4);
}

static inline u32 rehash(u64 hash) {
	return ((hash) + ((hash) >> 6) + ((hash) >> 19));
}

static inline u32 hash64(u64 u) {
	u32 hash = rehash(u);
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
	if (type == ELF_VALUE_TYPE_ATOM) {
		return atom_hash((elf_String *)key) ^ type_hash;
	}

	return hash64(key) ^ type_hash;
}

static inline Entry *table_find_entry(Entry *entries, u32 nentries, elf_Value key)
{
	PROF_ADD(PROF_COUNTER_TABLE_LOOKUP, 1);

	u64 key_bits = (u64)key.x_i64;
	u64 type_bits = (u64)key.type << ENTRY_TYPE_SHIFT;
	u32 hash = table_hash_key(key_bits, key.type);
	u32 head = hash & (nentries - 1);
	u32 slot = head;

	do {
		PROF_ADD(PROF_COUNTER_TABLE_PROBE, 1);

		Entry *entry = entries + slot;
		u64 data = entry->data;

		if ((data & ENTRY_TYPE_MASK) == 0) {
			PROF_ADD(PROF_COUNTER_TABLE_MISS, 1);
			return 0;
		}

		if ((data & ENTRY_TYPE_MASK) == type_bits && entry->key == key_bits) {
			PROF_ADD(PROF_COUNTER_TABLE_HIT, 1);
			return entry;
		}

		slot = (slot + 1) & (nentries - 1);
	} while (slot != head);

	return 0;
}

static inline u32 table_find_slot(Entry *entries, u32 nentries, elf_Value key)
{
	PROF_ADD(PROF_COUNTER_TABLE_LOOKUP, 1);

	u64 key_bits = (u64)key.x_i64;
	u64 type_bits = (u64)key.type << ENTRY_TYPE_SHIFT;
	u32 hash = table_hash_key(key_bits, key.type);
	u32 head = hash & (nentries - 1);
	u32 slot = head;

	do {
		PROF_ADD(PROF_COUNTER_TABLE_PROBE, 1);

		Entry entry = entries[slot];
		u64 data = entry.data;

		if ((data & ENTRY_TYPE_MASK) == 0) {
			PROF_ADD(PROF_COUNTER_TABLE_MISS, 1);
			return slot;
		}

		if ((data & ENTRY_TYPE_MASK) == type_bits && entry.key == key_bits) {
			PROF_ADD(PROF_COUNTER_TABLE_HIT, 1);
			return slot;
		}

		slot = (slot + 1) & (nentries - 1);
	} while (slot != head);

	return TABLE_SLOT_NOT_FOUND;
}

static void table_resize_if_needed(elf_Table *table)
{
	if (table->nentries * TABLE_MAX_LOAD_NUMERATOR >= table->fillcounter * TABLE_MAX_LOAD_DENOMINATOR) {
		return;
	}

	elf_Table resized = *table;
	resized.nentries = table->nentries << 1;
	resized.entries = calloc(1, resized.nentries * sizeof(*resized.entries));

	for (u32 i = 0; i < table->nentries; ++i) {
		Entry entry = table->entries[i];

		if (entry_is_key(entry)) {
			u32 new_slot = table_find_slot(resized.entries, resized.nentries, entry_key_value(entry));
			ASSERT(new_slot != TABLE_SLOT_NOT_FOUND);
			resized.entries[new_slot] = entry;
		}
	}

	free(table->entries);
	table->nentries = resized.nentries;
	table->entries = resized.entries;
}

static inline u32 table_find_slot_for_write(elf_State *state, elf_Table *table, elf_Value key)
{
	ASSERT(table != 0);

	table_resize_if_needed(table);

	u32 slot = table_find_slot(table->entries, table->nentries, key);
	if (slot == TABLE_SLOT_NOT_FOUND) {
		elf_report_runtime_error(state, RUNTIME_ERROR_GENERIC, -1, "'%s': internal error, table slot lookup failed", value_type_name(key.type));
	}

	return slot;
}

b32 elf_table_contains(elf_State *state, elf_Table *table, elf_Value key)
{
	return table_find_entry(table->entries, table->nentries, key) != 0;
}

b32 elf_table_delete(elf_State *state, elf_Table *table, elf_Value key, elf_Value *removed)
{
	check_write(state, (elf_Object *)table);

	Entry *entry = table_find_entry(table->entries, table->nentries, key);
	if (!entry) {
		if (removed) *removed = value_nil();
		return false;
	}

	u32 index = entry_index(*entry);
	if (removed) *removed = table->array[index];

	for (u32 i = 0; i < table->nentries; ++i)
	{
		Entry *current = table->entries + i;
		if (!entry_is_key(*current)) continue;
		u32 current_index = entry_index(*current);
		if (current_index == index) entry_set_tomb(current);
		else if (current_index > index) entry_set_index(current, current_index - 1);
	}

	table_array_remove_unchecked(table, index, 1);
	return true;
}

void elf_table_clear(elf_State *state, elf_Table *table)
{
	check_write(state, (elf_Object *)table);
	value_zero_many(table->array, table->count);
	memset(table->entries, 0, table->nentries * sizeof(*table->entries));
	table->count = 0;
	table->fillcounter = 0;
}

void elf_table_alias(elf_State *state, elf_Table *table, elf_Value key, elf_Value alias)
{
	u32 key_slot = table_find_slot_for_write(state, table, key);

	if (!entry_is_key(table->entries[key_slot])) {
		return;
	}

	u32 alias_slot = table_find_slot_for_write(state, table, alias);
	table->entries[alias_slot] = entry_from_key_index(alias, entry_index(table->entries[key_slot]));
	table->fillcounter += 1;
}

elf_Value elf_table_get_or_nil(elf_State *state, elf_Table *table, elf_Value key)
{
	(void)state;

	Entry *entry = table_find_entry(table->entries, table->nentries, key);
	if (!entry) {
		return NIL_VALUE;
	}

	return table->array[entry_index(*entry)];
}

u32 elf_table_ensure(elf_State *state, elf_Table *table, elf_Value key)
{
	u32 slot = table_find_slot_for_write(state, table, key);
	Entry *entry = &table->entries[slot];

	if (entry_is_key(*entry)) {
		return entry_index(*entry);
	}

	table_array_reserve(table, table->count + 1);
	u32 index = table->count++;
	table->array[index] = value_nil();

	*entry = entry_from_key_index(key, index);
	table->fillcounter += 1;
	return index;
}

u32 elf_table_bind_to_index(elf_State *state, elf_Table *table, elf_Value key, u32 index)
{
	check_write(state, (elf_Object *)table);

	check_array_index(state, NO_BYTE, index, table->count);

	u32 slot = table_find_slot_for_write(state, table, key);
	Entry *entry = &table->entries[slot];

	if (entry_is_key(*entry)) {
		return entry_index(*entry);
	}

	*entry = entry_from_key_index(key, index);
	table->fillcounter += 1;
	return index;
}

u32 elf_table_set(elf_State *state, elf_Table *table, elf_Value key, elf_Value value)
{
	check_write(state, (elf_Object *)table);

	u32 index = elf_table_ensure(state, table, key);
	table->array[index] = value;
	return index;
}

u32 elf_array_length(elf_Table *table)
{
	return table->count;
}

u32 elf_array_add(elf_State *state, elf_Table *table, elf_Value value)
{
	check_write(state, (elf_Object *)table);

	table_array_reserve(table, table->count + 1);
	u32 index = table->count++;
	table->array[index] = value;
	return index;
}

elf_Value elf_array_get(elf_State *state, elf_Table *table, u32 index)
{
	return *table_array_slot(state, table, index);
}

void elf_array_set(elf_State *state, elf_Table *table, u32 index, elf_Value value)
{
	check_write(state, (elf_Object *)table);

	*table_array_slot(state, table, index) = value;
}

void elf_array_remove(elf_State *state, elf_Table *table, u32 index, u32 count)
{
	check_write(state, (elf_Object *)table);

	if (index > table->count || count > table->count - index) {
		elf_report_runtime_error(state, RUNTIME_ERROR_GENERIC, NO_BYTE
		,	"array range out of bounds: index %u, count %u, length %u"
		,	index, count, table->count);
	}

	u32 end = index + count;
	for (u32 i = 0; i < table->nentries; ++i)
	{
		Entry *entry = table->entries + i;
		if (!entry_is_key(*entry)) continue;
		u32 entry_at = entry_index(*entry);
		if (entry_at >= index && entry_at < end) entry_set_tomb(entry);
		else if (entry_at >= end) entry_set_index(entry, entry_at - count);
	}

	table_array_remove_unchecked(table, index, count);
}

u32 elf_array_insert(elf_State *state, elf_Table *table, u32 index, elf_Value value)
{
	check_write(state, (elf_Object *)table);
	if (index > table->count) {
		elf_report_runtime_error(state, RUNTIME_ERROR_GENERIC, NO_BYTE,
			"array insert index %u is out of bounds for length %u", index, table->count);
	}

	table_array_reserve(table, table->count + 1);
	memmove(table->array + index + 1, table->array + index,
		(table->count - index) * sizeof(*table->array));
	for (u32 i = 0; i < table->nentries; ++i) {
		Entry *entry = table->entries + i;
		if (entry_is_key(*entry) && entry_index(*entry) >= index) {
			entry_set_index(entry, entry_index(*entry) + 1);
		}
	}
	table->array[index] = value;
	table->count += 1;
	return index;
}

void elf_array_swap(elf_State *state, elf_Table *table, u32 left, u32 right)
{
	check_write(state, (elf_Object *)table);
	value_swap(table_array_slot(state, table, left), table_array_slot(state, table, right));
}

void elf_table_mark_field_readonly(elf_State *state, elf_Table *table, elf_Value field)
{
	u32 slot = table_find_slot(table->entries, table->nentries, field);
	if (slot == TABLE_SLOT_NOT_FOUND || !entry_is_key(table->entries[slot])) {
		elf_report_runtime_error(state, RUNTIME_ERROR_GENERIC, -1, "no such field!");
	}

	entry_mark_readonly(&table->entries[slot]);
}
