//
// See Copyright Notice In elf.h
//

#define ATOM_INITIAL_BUCKET_COUNT 128u
#define ATOM_MAX_LOAD_NUMERATOR   3u
#define ATOM_MAX_LOAD_DENOMINATOR 4u

static u32 atom_hash_data(const char *data, u32 size)
{
	u32 hash = 2166136261u;
	for (u32 i = 0; i < size; ++i)
	{
		hash ^= (u8)data[i];
		hash *= 16777619u;
	}
	return hash;
}

static u32 atom_bucket_index(Atom_Table *table, u32 hash)
{
	return hash & (table->bucket_count - 1);
}

static Atom *atom_table_find(Atom_Table *table, const char *data, u32 size, u32 hash)
{
	u32 bucket_index = atom_bucket_index(table, hash);
	for (Atom *atom = table->buckets[bucket_index]; atom; atom = atom->next)
	{
		if (atom->hash == hash && atom->size == size && !memcmp(atom->data, data, size)) {
			return atom;
		}
	}
	return 0;
}

static void atom_table_resize(Atom_Table *table);

static void atom_table_ensure_capacity(Atom_Table *table)
{
	if (table->count * ATOM_MAX_LOAD_DENOMINATOR >= table->bucket_count * ATOM_MAX_LOAD_NUMERATOR) {
		atom_table_resize(table);
	}
}

static void atom_table_resize(Atom_Table *table)
{
	u32 old_bucket_count = table->bucket_count;
	Atom **old_buckets = table->buckets;

	table->bucket_count <<= 1;
	table->buckets = elf_arena_push_zero(table->arena, sizeof(*table->buckets) * table->bucket_count);

	for (u32 i = 0; i < old_bucket_count; ++i)
	{
		Atom *atom = old_buckets[i];
		while (atom)
		{
			Atom *next = atom->next;
			u32 bucket_index = atom_bucket_index(table, atom->hash);
			atom->next = table->buckets[bucket_index];
			table->buckets[bucket_index] = atom;
			atom = next;
		}
	}
}

static void atom_table_init(Atom_Table *table, elf_Arena *arena)
{
	ASSERT(table);
	ASSERT(arena);
	zero_memory(table, sizeof(*table));
	table->arena = arena;
	table->bucket_count = ATOM_INITIAL_BUCKET_COUNT;
	table->buckets = elf_arena_push_zero(arena, sizeof(*table->buckets) * table->bucket_count);
}

static Atom *atom_from_data_size_id(Atom_Table *table, const char *data, u32 size, u16 id)
{
	ASSERT(table);
	ASSERT(data);

	u32 hash = atom_hash_data(data, size);
	Atom *existing = atom_table_find(table, data, size, hash);
	if (existing)
	{
		ASSERT(!id || !existing->id || existing->id == id);
		if (id && !existing->id) existing->id = id;
		return existing;
	}

	atom_table_ensure_capacity(table);
	u32 bucket_index = atom_bucket_index(table, hash);

	elf_arena_align(table->arena, 8);
	Atom *atom = elf_arena_push_zero(table->arena, sizeof(*atom) + size + 1);
	atom->hash = hash;
	atom->size = size;
	atom->id = id;
	copy_memory(atom->data, data, size);
	atom->data[size] = 0;
	atom->next = table->buckets[bucket_index];
	table->buckets[bucket_index] = atom;
	table->count += 1;
	return atom;
}

static Atom *atom_table_begin(Atom_Table *table, u32 capacity)
{
	ASSERT(table);
	atom_table_ensure_capacity(table);
	elf_arena_align(table->arena, 8);
	Atom *candidate = elf_arena_push(table->arena, sizeof(*candidate) + capacity + 1);
	candidate->size = capacity;
	return candidate;
}

static Atom *atom_table_end(Atom_Table *table, Atom *candidate, u32 size)
{
	ASSERT(table);
	ASSERT(candidate);
	u32 capacity = candidate->size;
	ASSERT(size <= capacity);
	u64 candidate_start = (u64)((u8 *)candidate - table->arena->data);
	ASSERT(table->arena->in_use == candidate_start + sizeof(*candidate) + capacity + 1);

	u32 hash = atom_hash_data(candidate->data, size);
	Atom *existing = atom_table_find(table, candidate->data, size, hash);
	if (existing)
	{
		table->arena->in_use = candidate_start;
		return existing;
	}

	Atom *atom = candidate;
	table->arena->in_use = candidate_start + sizeof(*atom) + size + 1;
	atom->next = 0;
	atom->hash = hash;
	atom->size = size;
	atom->id = 0;
	atom->data[size] = 0;

	u32 bucket_index = atom_bucket_index(table, hash);
	atom->next = table->buckets[bucket_index];
	table->buckets[bucket_index] = atom;
	table->count += 1;
	return atom;
}

static elf_String *elf_string_from_atom(elf_State *state, Atom *atom)
{
	ASSERT(atom);
	return elf_string_from_data_size(state, atom_data(atom), atom_size(atom));
}
