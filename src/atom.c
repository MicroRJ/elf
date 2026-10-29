//
// See Copyright Notice In elf.h
//

typedef struct Atom Atom;
struct Atom
{
	Atom *next;
	u32   hash;
	u32   size;
	u16   id;
	char  data[1];
};

typedef struct Atom_Table Atom_Table;
struct Atom_Table
{
	Arena *arena;
	Atom **buckets;
	u32    bucket_count;
	u32    count;
};

#define ATOM_INITIAL_BUCKET_COUNT 128u
#define ATOM_MAX_LOAD_NUMERATOR   3u
#define ATOM_MAX_LOAD_DENOMINATOR 4u

static u32 atom_size(Atom *atom)
{
	return atom->size;
}

static const char *atom_data(Atom *atom)
{
	return atom->data;
}

static u32 atom_hash(Atom *atom)
{
	return atom->hash;
}

static b32 atoms_equal(Atom *left, Atom *right)
{
	return left == right;
}

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

static b32 atom_table_resize(Atom_Table *table);

static b32 atom_table_ensure_capacity(Atom_Table *table)
{
	if (table->count * ATOM_MAX_LOAD_DENOMINATOR >= table->bucket_count * ATOM_MAX_LOAD_NUMERATOR) {
		return atom_table_resize(table);
	}
	return true;
}

static b32 atom_table_resize(Atom_Table *table)
{
	u32 old_bucket_count = table->bucket_count;
	Atom **old_buckets = table->buckets;
	u32 new_bucket_count = old_bucket_count << 1;
	Atom **new_buckets = arena_push_zero(table->arena, sizeof(*new_buckets) * new_bucket_count, _Alignof(Atom *));
	if (!new_buckets) {
		return false;
	}

	table->bucket_count = new_bucket_count;
	table->buckets = new_buckets;

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
	return true;
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

	if (!atom_table_ensure_capacity(table)) {
		return 0;
	}
	u32 bucket_index = atom_bucket_index(table, hash);

	Atom *atom = arena_push_zero(table->arena, sizeof(*atom) + size + 1, 8);
	if (!atom) {
		return 0;
	}
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

static Atom *atom_from_data_size(Atom_Table *table, const char *data, u32 size)
{
	return atom_from_data_size_id(table, data, size, 0);
}

static Atom *atom_from_data(Atom_Table *table, const char *data)
{
	ASSERT(data);
	return atom_from_data_size(table, data, (u32)strlen(data));
}

static Atom *atom_from_data_id(Atom_Table *table, const char *data, u16 id)
{
	ASSERT(data);
	return atom_from_data_size_id(table, data, (u32)strlen(data), id);
}

static b32 atom_table_init(Atom_Table *table, Arena *arena)
{
	ASSERT(table);
	ASSERT(arena);
	zero_memory(table, sizeof(*table));
	table->arena = arena;
	table->bucket_count = ATOM_INITIAL_BUCKET_COUNT;
	table->buckets = arena_push_zero(arena, sizeof(*table->buckets) * table->bucket_count, _Alignof(Atom *));
	if (!table->buckets)
	{
		return false;
	}

	b32 result = true;
#define INTERN_KEYWORD_ATOM(NAME, TEXT) result = result && atom_from_data_id(table, TEXT, XFUSE(TOK_, NAME));
	KEYWORD_DEFINITIONS(INTERN_KEYWORD_ATOM)
#undef INTERN_KEYWORD_ATOM

#define INTERN_MACRO_ATOM(NAME, TEXT) result = result && atom_from_data_id(table, "#" TEXT, XFUSE(TOK_, NAME));
	MACRO_DEFINITIONS(INTERN_MACRO_ATOM)
#undef INTERN_MACRO_ATOM

	return result;
}
