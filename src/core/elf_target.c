//
// See Copyright Notice In elf.h
//

#define ELF_PROTOTYPE_COUNT_MAX (1u << 15)

static u64 prototype_append_region(u64 *allocation_size, u64 size, u64 alignment)
{
	ASSERT(alignment && !(alignment & (alignment - 1)));
	if (!size)
	{
		return 0;
	}
	u64 offset = (*allocation_size + alignment - 1) & ~(alignment - 1);
	ASSERT(offset <= UINT64_MAX - size);
	*allocation_size = offset + size;
	return offset;
}

elf_u32 elf_reserve_prototype(elf_State *state)
{
	elf_u32 index = ELF_PROTOTYPE_INVALID;
	if (state && state->prototype_count < ELF_PROTOTYPE_COUNT_MAX)
	{
		if (state->prototype_count == state->prototype_capacity)
		{
			u32 old_capacity = state->prototype_capacity;
			u32 new_capacity = old_capacity ? old_capacity * 2 : 16;
			new_capacity = MIN(new_capacity, ELF_PROTOTYPE_COUNT_MAX);
			state->prototypes = elf_realloc(state, state->prototypes, sizeof(*state->prototypes) * old_capacity, sizeof(*state->prototypes) * new_capacity);
			ASSERT(state->prototypes);
			zero_memory(state->prototypes + old_capacity, sizeof(*state->prototypes) * (new_capacity - old_capacity));
			state->prototype_capacity = new_capacity;
		}
		index = state->prototype_count++;
	}
	return index;
}

// TODO(RJ): this is not a safe API,
elf_Bool elf_init_prototype(elf_State *state, elf_u32 index, const BcFunction *function)
{
	elf_Bool initialized = false;
	if (state && function && index < state->prototype_count && !state->prototypes[index])
	{
		u32 saved_gc_mode = state->gc_mode;
		state->gc_mode = ELF_GC_PAUSED;

		u64 allocation_size = sizeof(elf_Prototype);
		u64 strings_offset = prototype_append_region(&allocation_size, sizeof(elf_String *) * function->string_count, _Alignof(elf_String *));
		u64 integer_constants_offset = prototype_append_region(&allocation_size, sizeof(i64) * function->integer_constant_count, _Alignof(i64));
		u64 number_constants_offset = prototype_append_region(&allocation_size, sizeof(f64) * function->number_constant_count, _Alignof(f64));
		u64 bytecode_offset = prototype_append_region(&allocation_size, sizeof(Bytecode) * function->bytecode_count, _Alignof(Bytecode));
		u64 source_map_offset = prototype_append_region(&allocation_size, sizeof(BcSourceMapEntry) * function->source_map_count, _Alignof(BcSourceMapEntry));
		elf_Prototype *prototype = elf_alloc_zero(state, allocation_size);
		ASSERT(prototype);

		*prototype = (elf_Prototype) {
			.allocation_size = allocation_size,
			.variadic = function->variadic,
			.arity = function->arity,
			.captures = function->captures,
			.stack_size = function->stack_size,
			.string_count = function->string_count,
			.integer_constant_count = function->integer_constant_count,
			.number_constant_count = function->number_constant_count,
			.bytecode_count = function->bytecode_count,
			.source_map_count = function->source_map_count,
		};
		if (prototype->string_count)
		{
			prototype->strings = (elf_String **)((u8 *)prototype + strings_offset);
			for (u32 i = 0; i < prototype->string_count; ++i)
			{
				prototype->strings[i] = elf_string_from_data_size(state, function->strings[i].data, function->strings[i].size);
			}
		}
		if (prototype->integer_constant_count)
		{
			prototype->integer_constants = (i64 *)((u8 *)prototype + integer_constants_offset);
			copy_memory(prototype->integer_constants, function->integer_constants, sizeof(*prototype->integer_constants) * prototype->integer_constant_count);
		}
		if (prototype->number_constant_count)
		{
			prototype->number_constants = (f64 *)((u8 *)prototype + number_constants_offset);
			copy_memory(prototype->number_constants, function->number_constants, sizeof(*prototype->number_constants) * prototype->number_constant_count);
		}
		if (prototype->bytecode_count)
		{
			prototype->bytecode = (Bytecode *)((u8 *)prototype + bytecode_offset);
			copy_memory(prototype->bytecode, function->bytecode, sizeof(*prototype->bytecode) * prototype->bytecode_count);
		}
		if (prototype->source_map_count)
		{
			prototype->source_map = (BcSourceMapEntry *)((u8 *)prototype + source_map_offset);
			copy_memory(prototype->source_map, function->source_map, sizeof(*prototype->source_map) * prototype->source_map_count);
		}

		state->prototypes[index] = prototype;
		state->gc_mode = saved_gc_mode;
		initialized = true;
	}
	return initialized;
}
