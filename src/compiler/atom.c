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

static u32 atom_bucket_index(AtomTable *table, u32 hash)
{
	return hash & (table->bucket_count - 1);
}

static elf_Atom *atom_table_find(AtomTable *table, const char *data, u32 size, u32 hash)
{
	u32 bucket_index = atom_bucket_index(table, hash);
	for (elf_Atom *atom = table->buckets[bucket_index]; atom; atom = atom->next)
	{
		if (atom->hash == hash && atom->size == size && !memcmp(atom->data, data, size)) {
			return atom;
		}
	}
	return 0;
}

static void atom_table_resize(AtomTable *table);

static void atom_table_ensure_capacity(AtomTable *table)
{
	if (table->count * ATOM_MAX_LOAD_DENOMINATOR >= table->bucket_count * ATOM_MAX_LOAD_NUMERATOR) {
		atom_table_resize(table);
	}
}

static void atom_table_resize(AtomTable *table)
{
	u32 old_bucket_count = table->bucket_count;
	elf_Atom **old_buckets = table->buckets;

	table->bucket_count <<= 1;
	table->buckets = elf_arena_push_zero(table->arena, sizeof(*table->buckets) * table->bucket_count);

	for (u32 i = 0; i < old_bucket_count; ++i)
	{
		elf_Atom *atom = old_buckets[i];
		while (atom)
		{
			elf_Atom *next = atom->next;
			u32 bucket_index = atom_bucket_index(table, atom->hash);
			atom->next = table->buckets[bucket_index];
			table->buckets[bucket_index] = atom;
			atom = next;
		}
	}
}

static void atom_table_init(AtomTable *table, elf_Arena *arena)
{
	ASSERT(table);
	ASSERT(arena);
	zero_memory(table, sizeof(*table));
	table->arena = arena;
	table->bucket_count = ATOM_INITIAL_BUCKET_COUNT;
	table->buckets = elf_arena_push_zero(arena, sizeof(*table->buckets) * table->bucket_count);
}

static elf_Atom *atom_from_data_size_id(AtomTable *table, const char *data, u32 size, u16 id)
{
	ASSERT(table);
	ASSERT(data);

	u32 hash = atom_hash_data(data, size);
	elf_Atom *existing = atom_table_find(table, data, size, hash);
	if (existing)
	{
		ASSERT(!id || !existing->id || existing->id == id);
		if (id && !existing->id) existing->id = id;
		return existing;
	}

	atom_table_ensure_capacity(table);
	u32 bucket_index = atom_bucket_index(table, hash);

	elf_arena_align(table->arena, 8);
	elf_Atom *atom = elf_arena_push_zero(table->arena, sizeof(*atom) + size + 1);
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

static elf_Atom *atom_table_begin(AtomTable *table, u32 capacity)
{
	ASSERT(table);
	atom_table_ensure_capacity(table);
	elf_arena_align(table->arena, 8);
	elf_Atom *candidate = elf_arena_push(table->arena, sizeof(*candidate) + capacity + 1);
	candidate->size = capacity;
	return candidate;
}

static elf_Atom *atom_table_end(AtomTable *table, elf_Atom *candidate, u32 size)
{
	ASSERT(table);
	ASSERT(candidate);
	u32 capacity = candidate->size;
	ASSERT(size <= capacity);
	u64 candidate_start = (u64)((u8 *)candidate - table->arena->data);
	ASSERT(table->arena->in_use == candidate_start + sizeof(*candidate) + capacity + 1);

	u32 hash = atom_hash_data(candidate->data, size);
	elf_Atom *existing = atom_table_find(table, candidate->data, size, hash);
	if (existing)
	{
		table->arena->in_use = candidate_start;
		return existing;
	}

	elf_Atom *atom = candidate;
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

static elf_String *elf_string_from_atom(elf_State *state, elf_Atom *atom)
{
	ASSERT(atom);
	return elf_string_from_data_size(state, atom_data(atom), atom_size(atom));
}
