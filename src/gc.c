//
// See Copyright Notice In elf.h
//

static void collector_check(elf_State *state);
static u32 mark_reachable(elf_State *state, GCRef reference);

void *collector_alloc(elf_State *state, GCType type, u32 size)
{
	GCState *collector_state = & state->collector_state;

	collector_state->memory_counter += size;

	if (collector_state->state == ELF_GC_ACTIVE)
	{
		collector_check(state);
	}

	GCRef reference = calloc(size, 1);
	reference->status = 0;
	reference->type   = type;
	reference->size   = size;

	collector_state->references1[collector_state->counter ++] = reference;
	return reference;
}

static u32 collector_free(elf_State *state)
{
	GCState *collector_state = & state->collector_state;
	GCRef *references1 = collector_state->references1;
	GCRef *references2 = collector_state->references2;

	u32 counter = 0;
	u32 free_counter = 0;

	for (u32 i = 0; i < collector_state->counter; ++ i)
	{
		GCRef reference = references1[i];
		if (reference->status & GC_TAG_REACHABLE)
		{
			reference->status &= ~ GC_TAG_REACHABLE;
			references2[counter ++] = reference;
		}
		else
		{
			free_counter ++;
			collector_state->memory_counter -= reference->size;

			if (reference->type == GC_TABLE)
			{
				_table_freeinternalmemory((Table *) reference);
			}

			free(reference);
		}
	}

	collector_state->references1 = references2;
	collector_state->references2 = references1;
	return free_counter;
}

static u32 mark_stack(elf_State *state)
{
	u32 counter = 0;
	for (Value *ptr = state->stack; ptr < state->stack_ptr; ++ ptr)
	{
		if (tisobject(ptr->tag))
		{
			check_trap_reference(state, ptr->x_obj);
			counter += mark_reachable(state, ptr->x_obj);
		}
	}
	return counter;
}

static u32 collector_cycle(elf_State *state)
{
	// Time time = prof_get_time();
	u32 mark_counter = mark_stack(state);
	u32 free_counter = collector_free(state);
	// elf_f64 took = prof_time_diff_ms(time);
	// elf_ldebug("marked: %i, freed: %i, took: %.4fMS", mark_counter, free_counter, took);
	return free_counter;
}

void collector_check(elf_State *state)
{
	GCState *collector_state = & state->collector_state;

	u32 counter = collector_state->counter;
	u32 free_counter;

	if (counter > collector_state->counter_thresh)
	{
		free_counter = collector_cycle(state);

		ASSERT(free_counter <= counter);
		collector_state->counter_thresh += GC_OBJ_THRESHOLD_MIN - free_counter;
	}
	else if (collector_state->memory_counter > collector_state->memory_thresh)
	{
		collector_state->memory_thresh <<= 1;
		if (collector_state->memory_thresh > GC_MEM_THRESHOLD_MAX)
		{
			collector_state->memory_thresh = GC_MEM_THRESHOLD_MAX;
		}

		free_counter = collector_cycle(state);
		ASSERT(free_counter <= counter);

		if (collector_state->memory_counter > collector_state->memory_thresh) {
			reporterrorf(state, NO_BYTE, "out of memory, %lliMB allocated, %lliMB threshold"
			, collector_state->memory_counter / MEGABYTES(1)
			, collector_state->memory_thresh / MEGABYTES(1));
		}
	}
}

static u32 mark_table_reachable(elf_State *state, Table *table);

static u32 mark_reachable(elf_State *state, GCRef reference)
{
	ASSERT(reference != 0);

	u32 counter = 0;

	if (~reference->status & GC_TAG_REACHABLE)
	{
		reference->status |= GC_TAG_REACHABLE;

		counter = 1;

		if (reference->meta)
		{
			counter += mark_reachable(state, (GCRef) reference->meta);
		}

		if (reference->type == GC_CLOSURE)
		{
			Closure closure = (Closure) reference;

			for (u32 i = 0; i < closure->function.ncaptures; ++ i)
			{
				if (value_is_reference(closure->captures[i]))
				{
					counter += mark_reachable(state, reference_from_value(closure->captures[i]));
				}
			}
		}
		else if (reference->type == GC_TABLE)
		{
			counter += mark_table_reachable(state, (Table *) reference);
		}
	}
	return counter;
}

static u32 mark_table_reachable(elf_State *S, Table *table)
{
	u32 counter = 0;

	for(u32 i = 0; i < table->nentries; ++ i)
	{
		if (value_is_reference(table->entries[i].key))
		{
			counter += mark_reachable(S, reference_from_value(table->entries[i].key));
		}
	}
	for(u32 i = 0; i < heap_array_length(table->array); ++ i)
	{
		if (value_is_reference(table->array[i]))
		{
			counter += mark_reachable(S, reference_from_value(table->array[i]));
		}
	}
	return counter;
}
