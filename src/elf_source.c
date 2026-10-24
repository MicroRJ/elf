//
// See Copyright Notice In elf.h
//

elf_Source *elf_source_create(elf_State *state, dy_String name, dy_String text)
{
	ASSERT(state);
	ASSERT(name.data || !name.size);
	ASSERT(text.data || !text.size);
	ASSERT(name.size <= UINT64_MAX - sizeof(elf_Source) - 1);
	u64 allocation_size = sizeof(elf_Source) + name.size + 1;
	ASSERT(text.size <= UINT64_MAX - allocation_size - 1);
	allocation_size += text.size + 1;

	elf_Source *source = elf_alloc_zero(state, allocation_size);
	ASSERT(source);
	source->allocation_size = allocation_size;

	char *data = (char *)(source + 1);
	source->name.data = data;
	source->name.size = name.size;
	if (name.size)
	{
		copy_memory(source->name.data, name.data, name.size);
	}

	data += name.size + 1;
	source->text.data = data;
	source->text.size = text.size;
	if (text.size)
	{
		copy_memory(source->text.data, text.data, text.size);
	}

	source->next = state->sources;
	state->sources = source;
	return source;
}

void elf_sources_destroy(elf_State *state)
{
	elf_Source *source = state->sources;
	while (source)
	{
		elf_Source *next = source->next;
		elf_free(state, source, source->allocation_size);
		source = next;
	}
	state->sources = 0;
}
