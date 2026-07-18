//
// See Copyright Notice In elf.h
//

#define ELF_ATOM_INITIAL_EXTENT       8192u
#define ELF_ATOM_MAX_LOAD_NUMERATOR   3u
#define ELF_ATOM_MAX_LOAD_DENOMINATOR 4u

static inline u32 atom_hash_data(const char *data, u32 size)
{
	u32 hash = 2166136261u;

	for (u32 i = 0; i < size; ++i) {
		hash ^= (u8) data[i];
		hash *= 16777619u;
	}

	return hash;
}

static void atom_state_init(elf_State *state)
{
	if (!state->atom_bucket_count) {
		state->atom_bucket_count = ELF_ATOM_INITIAL_EXTENT;
		state->atom_buckets = calloc(state->atom_bucket_count, sizeof(*state->atom_buckets));
	}
}

static u32 atom_bucket_index(elf_State *state, u32 hash)
{
	return hash & (state->atom_bucket_count - 1);
}

static void atom_state_resize(elf_State *state)
{
	u32 old_bucket_count = state->atom_bucket_count;
	elf_Atom **old_buckets = state->atom_buckets;

	state->atom_bucket_count <<= 1;
	state->atom_buckets = calloc(state->atom_bucket_count, sizeof(*state->atom_buckets));

	for (u32 i = 0; i < old_bucket_count; ++i) {
		elf_Atom *atom = old_buckets[i];
		while (atom) {
			elf_Atom *next = atom->next;
			u32 bucket_index = atom_bucket_index(state, atom->hash);
			atom->next = state->atom_buckets[bucket_index];
			state->atom_buckets[bucket_index] = atom;
			atom = next;
		}
	}

	free(old_buckets);
}

static elf_Atom *atom_find(elf_State *state, const char *data, u32 size, u32 hash)
{
	PROF_ADD(PROF_COUNTER_ATOM_LOOKUP, 1);

	if (!state->atom_bucket_count) {
		PROF_ADD(PROF_COUNTER_ATOM_MISS, 1);
		return 0;
	}

	u32 bucket_index = atom_bucket_index(state, hash);
	for (elf_Atom *atom = state->atom_buckets[bucket_index]; atom; atom = atom->next) {
		PROF_ADD(PROF_COUNTER_ATOM_PROBE, 1);
		if (atom->hash == hash && atom->size == size && !memcmp(atom->data, data, size)) {
			PROF_ADD(PROF_COUNTER_ATOM_HIT, 1);
			return atom;
		}
	}

	PROF_ADD(PROF_COUNTER_ATOM_MISS, 1);
	return 0;
}

static void atom_insert(elf_State *state, elf_Atom *atom)
{
	atom_state_init(state);

	if (state->atom_count * ELF_ATOM_MAX_LOAD_DENOMINATOR >= state->atom_bucket_count * ELF_ATOM_MAX_LOAD_NUMERATOR) {
		atom_state_resize(state);
	}

	u32 bucket_index = atom_bucket_index(state, atom->hash);
	atom->next = state->atom_buckets[bucket_index];
	state->atom_buckets[bucket_index] = atom;
	state->atom_count++;
}

static void atom_remove_dead(elf_State *state)
{
	if (!state->atom_bucket_count) {
		return;
	}

	u32 counter = 0;
	for (u32 i = 0; i < state->atom_bucket_count; ++i) {
		elf_Atom **link = state->atom_buckets + i;
		while (*link) {
			elf_Atom *atom = *link;
			if (atom->obj.status & ELF_OBJECT_REACHABLE) {
				counter++;
				link = &atom->next;
			}
			else {
				*link = atom->next;
				atom->next = 0;
			}
		}
	}
	state->atom_count = counter;
}

elf_Atom *elf_atom_from_data_size_id(elf_State *state, const char *data, u32 size, u16 id)
{
	ASSERT(state);
	ASSERT(data);
	ASSERT(size <= 0xffff);

	u32 hash = atom_hash_data(data, size);
	elf_Atom *interned = atom_find(state, data, size, hash);
	if (interned) {
		ASSERT(!id || !interned->id || interned->id == id);
		if (id && !interned->id) {
			interned->id = id;
		}
		return interned;
	}

	atom_state_init(state);

	elf_Atom *atom = gc_alloc(state, ELF_OBJECT_ATOM, sizeof(*atom) + size + 1);
	atom->hash = hash;
	atom->id = id;
	atom->size = (u16)size;
	copy_memory(atom->data, data, size);
	atom->data[size] = 0;

	atom_insert(state, atom);
	return atom;
}

elf_Atom *elf_atom_from_data_size(elf_State *state, const char *data, u32 size)
{
	return elf_atom_from_data_size_id(state, data, size, 0);
}

elf_Atom *elf_atom_from_data(elf_State *state, const char *data)
{
	ASSERT(data);
	return elf_atom_from_data_size(state, data, (u32)strlen(data));
}

elf_Atom *elf_atom_from_data_id(elf_State *state, const char *data, u16 id)
{
	ASSERT(data);
	return elf_atom_from_data_size_id(state, data, (u32)strlen(data), id);
}

u32 elf_atom_size(elf_Atom *atom)
{
	return atom->size;
}

const char *elf_atom_data(elf_Atom *atom)
{
	return atom->data;
}

elf_StrSlice elf_atom_copy_text(elf_Arena *arena, elf_Atom *atom)
{
	elf_StrSlice copy = {};
	copy.data = elf_arena_push_data(arena, atom->data, atom->size);
	copy.size = atom->size;
	elf_arena_push_zero(arena, 1);
	return copy;
}

u32 elf_atom_hash(elf_Atom *atom)
{
	return atom->hash;
}

b32 elf_atoms_equal(elf_Atom *left, elf_Atom *right)
{
	return left == right;
}
