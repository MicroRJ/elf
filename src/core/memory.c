//
// See Copyright Notice In elf.h
//

static void memory_record_allocation(elf_State *state, u64 size)
{
	if (!state || !size) return;
	state->gc_allocated_bytes += size;
	state->gc_live_bytes += size;
	state->gc_peak_live_bytes = MAX(state->gc_peak_live_bytes, state->gc_live_bytes);
}

static void memory_record_free(elf_State *state, u64 size)
{
	if (!state || !size) return;
	ASSERT(state->gc_live_bytes >= size);
	state->gc_freed_bytes += size;
	state->gc_live_bytes -= size;
}

void *elf_alloc(elf_State *state, u64 size)
{
	if (!size) return 0;
	ASSERT(size <= SIZE_MAX);
	void *memory = malloc((size_t)size);
	if (memory) memory_record_allocation(state, size);
	return memory;
}

void *elf_alloc_zero(elf_State *state, u64 size)
{
	void *memory = elf_alloc(state, size);
	if (memory) zero_memory(memory, size);
	return memory;
}

void *elf_realloc(elf_State *state, void *memory, u64 old_size, u64 new_size)
{
	ASSERT(old_size <= SIZE_MAX);
	ASSERT(new_size <= SIZE_MAX);
	ASSERT(memory || !old_size);

	if (!new_size)
	{
		elf_free(state, memory, old_size);
		return 0;
	}

	void *resized = realloc(memory, (size_t)new_size);
	if (!resized) return 0;

	if (new_size > old_size) memory_record_allocation(state, new_size - old_size);
	else if (old_size > new_size) memory_record_free(state, old_size - new_size);
	return resized;
}

void elf_free(elf_State *state, void *memory, u64 size)
{
	if (!memory)
	{
		ASSERT(!size);
		return;
	}
	free(memory);
	memory_record_free(state, size);
}
