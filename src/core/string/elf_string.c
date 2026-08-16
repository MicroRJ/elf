//
// See Copyright Notice In elf.h
//

// TODO(RJ): investigate performance and design trade-offs of lazy hashing.
// Not sure that we may want to introduce additional logic & complexity to
// support lazy hashing in extremely hot paths.
// Given the typical use case, is it OK to just pay the hash cost once
// for every string at creation time, even for large strings?
// Are very large strings really that common?
// Will we have dedicated large storage containers regardless, that relieve string
// building pressure?
// How expensive is hashing really, compared to the permanent added cost of the additional
// logic added for the entirety of the runtime?

#define ELF_STRING_INITIAL_EXTENT       8192u
#define ELF_STRING_MAX_LOAD_NUMERATOR   3u
#define ELF_STRING_MAX_LOAD_DENOMINATOR 4u

static inline u32 string_hash_data(const char *data, u32 size)
{
	u32 hash = 2166136261u;

	for (u32 i = 0; i < size; ++i) {
		hash ^= (u8) data[i];
		hash *= 16777619u;
	}

	return hash;
}

static u32 string_bucket_index(elf_State *state, u32 hash)
{
	return hash & (state->string_bucket_count - 1);
}

static void string_state_resize(elf_State *state)
{
	u32 old_bucket_count = state->string_bucket_count;
	elf_String **old_buckets = state->string_buckets;

	state->string_bucket_count <<= 1;
	state->string_buckets = calloc(state->string_bucket_count, sizeof(*state->string_buckets));

	for (u32 i = 0; i < old_bucket_count; ++i) {
		elf_String *string = old_buckets[i];
		while (string) {
			elf_String *next = string->next;
			u32 bucket_index = string_bucket_index(state, string->hash);
			string->next = state->string_buckets[bucket_index];
			state->string_buckets[bucket_index] = string;
			string = next;
		}
	}

	free(old_buckets);
}

static elf_String *string_find(elf_State *state, const char *data, u32 size, u32 hash)
{
	PROF_ADD(PROF_COUNTER_STRING_LOOKUP, 1);

	if (!state->string_bucket_count) {
		PROF_ADD(PROF_COUNTER_STRING_MISS, 1);
		return 0;
	}

	u32 bucket_index = string_bucket_index(state, hash);
	for (elf_String *string = state->string_buckets[bucket_index]; string; string = string->next) {
		PROF_ADD(PROF_COUNTER_STRING_PROBE, 1);
		if (string->hash == hash && string_size(string) == size && !memcmp(string->data, data, size)) {
			PROF_ADD(PROF_COUNTER_STRING_HIT, 1);
			return string;
		}
	}

	PROF_ADD(PROF_COUNTER_STRING_MISS, 1);
	return 0;
}

static void string_insert(elf_State *state, elf_String *string)
{
	if (state->string_count * ELF_STRING_MAX_LOAD_DENOMINATOR >= state->string_bucket_count * ELF_STRING_MAX_LOAD_NUMERATOR) {
		string_state_resize(state);
	}

	u32 bucket_index = string_bucket_index(state, string->hash);
	string->next = state->string_buckets[bucket_index];
	state->string_buckets[bucket_index] = string;
	state->string_count++;
}

static void string_remove_dead(elf_State *state)
{
	if (!state->string_bucket_count) {
		return;
	}

	u32 counter = 0;
	for (u32 i = 0; i < state->string_bucket_count; ++i) {
		elf_String **link = state->string_buckets + i;
		while (*link) {
			elf_String *string = *link;
			if (string->obj.status & ELF_OBJECT_REACHABLE) {
				counter++;
				link = &string->next;
			}
			else {
				*link = string->next;
				string->next = 0;
			}
		}
	}
	state->string_count = counter;
}

elf_String *elf_string_from_data_size(elf_State *state, const char *data, u32 size)
{
	ASSERT(state);
	ASSERT(data);
	ASSERT(size <= ELF_STRING_MAX_SIZE);

	u32 hash = string_hash_data(data, size);
	elf_String *interned = string_find(state, data, size, hash);
	if (interned) return interned;

	u32 allocation_size = ELF_STRING_HEADER_SIZE + size + 1;
	elf_String *string = elf_gc_alloc(state, ELF_OBJECT_STRING, allocation_size);
	string->hash = hash;
	copy_memory(string->data, data, size);
	string->data[size] = 0;

	string_insert(state, string);
	return string;
}



