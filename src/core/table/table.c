//
// See Copyright Notice In elf.h
//

#define TABLE_INITIAL_ENTRY_COUNT   4
#define TABLE_INITIAL_ARRAY_COUNT   8
#define TABLE_MAX_LOAD_NUMERATOR    3
#define TABLE_MAX_LOAD_DENOMINATOR  4
#define TABLE_SLOT_NOT_FOUND        ((u32)(-1))


static void checkwrite(elf_State *state, elf_Object *reference)
{
	if (reference->status & ELF_OBJECT_READONLY) {
		report_runtime_error(state, RUNTIME_ERROR_GENERIC, -1, "attempted to write to readonly object");
	}
}

static inline void table_init(elf_Table *table, u32 nentries)
{
	table->ndebug = 0;
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

elf_Table *elf_table_new_sized(elf_State *state, u32 nentries)
{
	elf_Table *table = gc_alloc(state, ELF_OBJECT_TABLE, sizeof(*table));

	table_init(table, nentries);
	return table;
}

elf_Table *elf_table_new(elf_State *state)
{
	return elf_table_new_sized(state, TABLE_INITIAL_ENTRY_COUNT);
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

static inline u32 table_hash_value(elf_Value *value)
{
	u32 type_hash = (u32)value->type * 0x9E3779B1u;
	if (value->type == ELF_VALUE_TYPE_ATOM) {
		return elf_atom_hash(value->x_atom) ^ type_hash;
	}

	return hash64(value->x_i64) ^ type_hash;
}

static inline bool table_entry_key_equal(Entry entry, elf_Value key)
{
	if (entry_type(entry) != key.type) {
		return false;
	}

	return entry.key == (u64)key.x_i64;
}

static inline u32 table_find_slot(Entry *entries, u32 nentries, elf_Value key)
{
	u32 hash = table_hash_value(&key);
	u32 head = hash & (nentries - 1);
	u32 slot = head;
	u32 step = 1;

	do {
		Entry entry = entries[slot];

		if (entry_is_nil(entry)) {
			return slot;
		}

		if (!entry_is_tomb(entry) && table_entry_key_equal(entry, key)) {
			return slot;
		}

		slot = (slot + step) & (nentries - 1);
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
		report_runtime_error(state, RUNTIME_ERROR_GENERIC, -1, "'%s': internal error, table slot lookup failed", value_type_name(key.type));
	}

	return slot;
}

bool elf_table_contains(elf_State *state, elf_Table *table, elf_Value key)
{
	u32 slot = table_find_slot_for_write(state, table, key);
	return slot != TABLE_SLOT_NOT_FOUND && entry_is_key(table->entries[slot]);
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

	u32 slot = table_find_slot(table->entries, table->nentries, key);
	if (slot == TABLE_SLOT_NOT_FOUND || !entry_is_key(table->entries[slot])) {
		return NIL_VALUE;
	}

	return table->array[entry_index(table->entries[slot])];
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
	checkwrite(state, (elf_Object *)table);

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
	checkwrite(state, (elf_Object *)table);

	u32 index = elf_table_ensure(state, table, key);
	table->array[index] = value;
	return index;
}

u32 elf_array_len(elf_Table *table)
{
	return table->count;
}

u32 elf_array_add(elf_State *state, elf_Table *table, elf_Value value)
{
	checkwrite(state, (elf_Object *)table);

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
	checkwrite(state, (elf_Object *)table);

	*table_array_slot(state, table, index) = value;
}

void elf_array_remove(elf_State *state, elf_Table *table, u32 index, u32 count)
{
	checkwrite(state, (elf_Object *)table);

	if (index > table->count || count > table->count - index) {
		report_runtime_error(state, RUNTIME_ERROR_GENERIC, NO_BYTE
		,	"array range out of bounds: index %u, count %u, length %u"
		,	index, count, table->count);
	}

	table_array_remove_unchecked(table, index, count);
}

void elf_array_swap(elf_State *state, elf_Table *table, u32 left, u32 right)
{
	checkwrite(state, (elf_Object *)table);
	value_swap(table_array_slot(state, table, left), table_array_slot(state, table, right));
}

void elf_table_mark_field_readonly(elf_State *state, elf_Table *table, elf_Value field)
{
	u32 slot = table_find_slot(table->entries, table->nentries, field);
	if (slot == TABLE_SLOT_NOT_FOUND || !entry_is_key(table->entries[slot])) {
		report_runtime_error(state, RUNTIME_ERROR_GENERIC, -1, "no such field!");
	}

	entry_mark_readonly(&table->entries[slot]);
}

//
// Note for myself:
//
// The hash-table implementations I know, mainly from stb, do something to hash
// integer bits, and so I did the same thing.
//
// One day however I decided to get rid of it because in my head I couldn't see
// how it could possibly make a difference. I mean, it's already an integer,
// right?
//
// Then as I kept making games that relied more and more on integer lookups, I
// started noticing unusually high CPU usage and significantly lower FPS.
//
// I don't remember exactly how I figured this out. I think maybe by stepping
// through with the debugger and seeing how many hash misses an integer lookup
// would get.
//
// So yes, integer hashing is crucial. The performance drop was because of all
// the time spent doing lookups after all the misses.
//
