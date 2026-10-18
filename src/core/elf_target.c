//
// See Copyright Notice In elf.h
//

#define ELF_PROTOTYPE_COUNT_MAX (1u << 15)

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

elf_Bool elf_init_prototype(elf_State *state, elf_u32 index, const BcFunction *function)
{
	elf_Bool initialized = false;
	if (state && function && index < state->prototype_count)
	{
		u32 saved_gc_mode = state->gc_mode;
		state->gc_mode = ELF_GC_PAUSED;

		elf_Prototype prototype = {
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
		if (prototype.string_count)
		{
			prototype.strings = arena_push_zero(&state->arena, sizeof(*prototype.strings) * prototype.string_count, _Alignof(elf_String *));
			for (u32 i = 0; i < prototype.string_count; ++i) {
				prototype.strings[i] = elf_string_from_data_size(state, function->strings[i].data, function->strings[i].size);
			}
		}
		if (prototype.integer_constant_count) {
			prototype.integer_constants = arena_push_copy(&state->arena, sizeof(*prototype.integer_constants) * prototype.integer_constant_count, _Alignof(i64), function->integer_constants);
		}
		if (prototype.number_constant_count) {
			prototype.number_constants = arena_push_copy(&state->arena, sizeof(*prototype.number_constants) * prototype.number_constant_count, _Alignof(f64), function->number_constants);
		}
		if (prototype.bytecode_count) {
			prototype.bytecode = arena_push_copy(&state->arena, sizeof(*prototype.bytecode) * prototype.bytecode_count, _Alignof(Bytecode), function->bytecode);
		}
		if (prototype.source_map_count) {
			prototype.source_map = arena_push_copy(&state->arena, sizeof(*prototype.source_map) * prototype.source_map_count, _Alignof(BcSourceMapEntry), function->source_map);
		}

		state->prototypes[index] = prototype;
		state->gc_mode = saved_gc_mode;
		initialized = true;
	}
	return initialized;
}
