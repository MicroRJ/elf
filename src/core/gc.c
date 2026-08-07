//
// See Copyright Notice In elf.h
//

static void gc_check(elf_State *state);
static u32 gc_mark_reachable(elf_State *state, elf_Object * reference);

static void gc_ensure_reference_capacity(elf_State *state)
{
	if (state->gc_reference_count < state->gc_reference_capacity) {
		return;
	}

	u32 old_capacity = state->gc_reference_capacity;
	u32 new_capacity = old_capacity ? old_capacity << 1 : 4096;

	state->gc_references = realloc(state->gc_references, sizeof(*state->gc_references) * new_capacity);
	state->gc_scratch_references = realloc(state->gc_scratch_references, sizeof(*state->gc_scratch_references) * new_capacity);
	ASSERT(state->gc_references != 0);
	ASSERT(state->gc_scratch_references != 0);

	zero_memory(state->gc_references + old_capacity, sizeof(*state->gc_references) * (new_capacity - old_capacity));
	zero_memory(state->gc_scratch_references + old_capacity, sizeof(*state->gc_scratch_references) * (new_capacity - old_capacity));
	state->gc_reference_capacity = new_capacity;
}

void *elf_gc_alloc(elf_State *state, elf_ObjectType type, u32 size)
{
	state->gc_live_bytes += size;

	if (state->gc_mode == ELF_GC_ACTIVE)
	{
		gc_check(state);
	}

	elf_Object * reference = calloc(size, 1);
	reference->status = 0;
	reference->type   = type;
	reference->size   = size;

	gc_ensure_reference_capacity(state);
	state->gc_references[state->gc_reference_count ++] = reference;
	return reference;
}

static u32 gc_sweep(elf_State *state)
{
	elf_Object **references = state->gc_references;
	elf_Object **scratch_references = state->gc_scratch_references;

	u32 survivor_count = 0;
	u32 freed_count = 0;

	atom_remove_dead(state);

	for (u32 i = 0; i < state->gc_reference_count; ++ i)
	{
		elf_Object * reference = references[i];
		if (reference->status & ELF_OBJECT_REACHABLE)
		{
			reference->status &= ~ ELF_OBJECT_REACHABLE;
			scratch_references[survivor_count ++] = reference;
		}
		else
		{
			freed_count ++;
			state->gc_live_bytes -= reference->size;

			if (reference->type == ELF_OBJECT_TABLE)
			{
				free(((elf_Table *) reference)->entries);
				free(((elf_Table *) reference)->array);
			}

			free(reference);
		}
	}

	state->gc_references = scratch_references;
	state->gc_scratch_references = references;
	state->gc_reference_count = survivor_count;
	return freed_count;
}

static u32 gc_mark_stack(elf_State *state)
{
	u32 marked_count = 0;
	for (elf_Value *ptr = state->stack; ptr < state->stack_ptr; ++ ptr)
	{
		if (type_is_object(ptr->type))
		{
			marked_count += gc_mark_reachable(state, ptr->x_obj);
		}
	}
	return marked_count;
}

static u32 gc_mark_interned_ids(elf_State *state)
{
	u32 marked_count = 0;

	for (u32 i = 0; i < state->atom_bucket_count; ++i)
	{
		for (elf_String *atom = state->atom_buckets[i]; atom; atom = atom->next) {
			if (atom->id) {
				marked_count += gc_mark_reachable(state, (elf_Object *)atom);
			}
		}
	}

	return marked_count;
}

static u32 gc_mark_bytecode_functions(elf_State *state)
{
	u32 marked_count = 0;

	for (u32 i = 0; i < state->module.bytecode_function_count; ++i)
	{
		BcFunction *function = state->module.bytecode_functions + i;
		if (function->source_name) {
			marked_count += gc_mark_reachable(state, (elf_Object *)function->source_name);
		}
	}

	return marked_count;
}

static u32 gc_mark_metatables(elf_State *state)
{
	u32 marked_count = 0;

	if (state->metatables.atom) {
		marked_count += gc_mark_reachable(state, (elf_Object *)state->metatables.atom);
	}
	if (state->metatables.table) {
		marked_count += gc_mark_reachable(state, (elf_Object *)state->metatables.table);
	}
	if (state->metatables.number) {
		marked_count += gc_mark_reachable(state, (elf_Object *)state->metatables.number);
	}
	if (state->metatables.integer) {
		marked_count += gc_mark_reachable(state, (elf_Object *)state->metatables.integer);
	}

	return marked_count;
}

static u32 gc_collect(elf_State *state)
{
	// Time time = prof_get_time();
	u32 marked_count = gc_mark_stack(state);
	if (state->ref_table) {
		marked_count += gc_mark_reachable(state, (elf_Object *)state->ref_table);
	}
	marked_count += gc_mark_metatables(state);
	marked_count += gc_mark_interned_ids(state);
	marked_count += gc_mark_bytecode_functions(state);
	u32 freed_count = gc_sweep(state);
	// elf_f64 took = prof_time_diff_ms(time);
	// elf_ldebug("marked: %i, freed: %i, took: %.4fMS", marked_count, freed_count, took);
	return freed_count;
}

static void gc_update_next_cycle(elf_State *state)
{
	u32 next_cycle_bytes = state->gc_live_bytes * 2;
	if (next_cycle_bytes < ELF_GC_NEXT_MIN) {
		next_cycle_bytes = ELF_GC_NEXT_MIN;
	}
	if (next_cycle_bytes > ELF_GC_NEXT_MAX) {
		next_cycle_bytes = ELF_GC_NEXT_MAX;
	}
	state->gc_next_cycle_bytes = next_cycle_bytes;
}

void gc_check(elf_State *state)
{
	if (state->gc_live_bytes < state->gc_next_cycle_bytes) {
		return;
	}

	u32 previous_reference_count = state->gc_reference_count;
	u32 freed_count = gc_collect(state);
	ASSERT(freed_count <= previous_reference_count);

	gc_update_next_cycle(state);

	if (state->gc_live_bytes > state->gc_next_cycle_bytes) {
		elf_report_runtime_error(state, RUNTIME_ERROR_GENERIC, NO_BYTE, "out of memory, %uMB allocated, %uMB threshold"
		, state->gc_live_bytes / MEGABYTES(1)
		, state->gc_next_cycle_bytes / MEGABYTES(1));
	}
}

static u32 gc_mark_table(elf_State *state, elf_Table *table);

static u32 gc_mark_reachable(elf_State *state, elf_Object * reference)
{
	ASSERT(reference != 0);

	u32 marked_count = 0;

	if (~reference->status & ELF_OBJECT_REACHABLE)
	{
		reference->status |= ELF_OBJECT_REACHABLE;

		marked_count = 1;

		if (reference->type == ELF_OBJECT_CLOSURE)
		{
			elf_Closure * closure = (elf_Closure *) reference;

			for (u32 i = 0; i < closure->function.captures; ++ i)
			{
				if (value_is_object(closure->captures[i]))
				{
					marked_count += gc_mark_reachable(state, value_as_object(closure->captures[i]));
				}
			}
		}
		else if (reference->type == ELF_OBJECT_TABLE)
		{
			marked_count += gc_mark_table(state, (elf_Table *) reference);
		}
		else if (reference->type == ELF_OBJECT_ATOM)
		{
		}
	}
	return marked_count;
}

static u32 gc_mark_table(elf_State *state, elf_Table *table)
{
	u32 marked_count = 0;

	for(u32 i = 0; i < table->nentries; ++ i)
	{
		Entry entry = table->entries[i];
		if (entry_is_key(entry))
		{
			elf_Value key = entry_key_value(entry);
			if (value_is_object(key))
			{
				marked_count += gc_mark_reachable(state, value_as_object(key));
			}
		}
	}
	for(u32 i = 0; i < elf_array_length(table); ++ i)
	{
		elf_Value value = elf_array_get(state, table, i);
		if (value_is_object(value))
		{
			marked_count += gc_mark_reachable(state, value_as_object(value));
		}
	}
	return marked_count;
}
