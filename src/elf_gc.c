//
// See Copyright Notice In elf.h
//

#include <dy_time.h>

static void gc_check(elf_State *state);
static u32 gc_mark_reachable(elf_State *state, elf_Object * reference);

static void gc_free_object(elf_State *state, elf_Object *reference)
{
	if (reference->type == ELF_OBJECT_TABLE)
	{
		elf_Table *table = (elf_Table *)reference;
		elf_free(state, table->entries, (u64)table->capacity * sizeof(*table->entries));
	}
	else if (reference->type == ELF_OBJECT_ARRAY)
	{
		elf_Array *array = (elf_Array *)reference;
		elf_free(state, array->items, (u64)array->capacity * sizeof(*array->items));
	}
	else if (reference->type == ELF_OBJECT_BUFFER)
	{
		elf_ByteBuffer *buffer = (elf_ByteBuffer *)reference;
		elf_free(state, buffer->data, buffer->capacity);
	}
	else if (reference->type == ELF_OBJECT_USER)
	{
		elf_UserObject *user = (elf_UserObject *)reference;
		if (user->type && user->type->destroy) user->type->destroy(state, user->data);
	}

	elf_free(state, reference, reference->size);
}

static void gc_destroy_all(elf_State *state)
{
	for (u32 i = 0; i < state->gc_reference_count; ++i) {
		gc_free_object(state, state->gc_references[i]);
	}

	elf_free(state, state->gc_references,
		(u64)state->gc_reference_capacity * sizeof(*state->gc_references));
	elf_free(state, state->gc_scratch_references,
		(u64)state->gc_reference_capacity * sizeof(*state->gc_scratch_references));
}

static void gc_ensure_reference_capacity(elf_State *state)
{
	if (state->gc_reference_count < state->gc_reference_capacity) {
		return;
	}

	u32 old_capacity = state->gc_reference_capacity;
	u32 new_capacity = old_capacity ? old_capacity << 1 : 4096;

	state->gc_references = elf_realloc(state, state->gc_references,
		(u64)sizeof(*state->gc_references) * old_capacity,
		(u64)sizeof(*state->gc_references) * new_capacity);
	state->gc_scratch_references = elf_realloc(state, state->gc_scratch_references,
		(u64)sizeof(*state->gc_scratch_references) * old_capacity,
		(u64)sizeof(*state->gc_scratch_references) * new_capacity);
	ASSERT(state->gc_references != 0);
	ASSERT(state->gc_scratch_references != 0);

	zero_memory(state->gc_references + old_capacity, sizeof(*state->gc_references) * (new_capacity - old_capacity));
	zero_memory(state->gc_scratch_references + old_capacity, sizeof(*state->gc_scratch_references) * (new_capacity - old_capacity));
	state->gc_reference_capacity = new_capacity;
}

void *elf_gc_alloc(elf_State *state, elf_ObjectType type, u32 size)
{
	ASSERT((u32)type < ELF_OBJECT_COUNT);
	state->gc_allocated_objects += 1;
	state->gc_allocated_by_type[type] += 1;

	if (state->gc_mode == ELF_GC_ACTIVE)
	{
		gc_check(state);
	}

	elf_Object *reference = elf_alloc_zero(state, size);
	ASSERT(reference != 0);
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

	string_remove_dead(state);

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
			state->gc_freed_objects += 1;

			gc_free_object(state, reference);
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

static u32 gc_mark_prototypes(elf_State *state)
{
	u32 marked_count = 0;
	for (u32 prototype_index = 0; prototype_index < state->prototype_count; ++prototype_index)
	{
		elf_Prototype *prototype = state->prototypes[prototype_index];
		if (prototype)
		{
			for (u32 i = 0; i < prototype->string_count; ++i)
			{
				marked_count += gc_mark_reachable(state, (elf_Object *)prototype->strings[i]);
			}
		}
	}
	return marked_count;
}

static u32 gc_mark_metatables(elf_State *state)
{
	u32 marked_count = 0;

	if (state->metatables.string) {
		marked_count += gc_mark_reachable(state, (elf_Object *)state->metatables.string);
	}
	if (state->metatables.table) {
		marked_count += gc_mark_reachable(state, (elf_Object *)state->metatables.table);
	}
	if (state->metatables.array) {
		marked_count += gc_mark_reachable(state, (elf_Object *)state->metatables.array);
	}
	if (state->metatables.number) {
		marked_count += gc_mark_reachable(state, (elf_Object *)state->metatables.number);
	}
	if (state->metatables.integer) {
		marked_count += gc_mark_reachable(state, (elf_Object *)state->metatables.integer);
	}
	if (state->metatables.buffer) {
		marked_count += gc_mark_reachable(state, (elf_Object *)state->metatables.buffer);
	}
	if (state->metatables.error) {
		marked_count += gc_mark_reachable(state, (elf_Object *)state->metatables.error);
	}
	if (state->metatables.fs_file) {
		marked_count += gc_mark_reachable(state, (elf_Object *)state->metatables.fs_file);
	}

	return marked_count;
}

static u32 gc_collect(elf_State *state)
{
	u32 marked_count = gc_mark_stack(state);
	if (state->ref_table) {
		marked_count += gc_mark_reachable(state, (elf_Object *)state->ref_table);
	}
	marked_count += gc_mark_metatables(state);
	marked_count += gc_mark_prototypes(state);
	u32 freed_count = gc_sweep(state);
	return freed_count;
}

static void gc_update_next_cycle(elf_State *state)
{
	u64 next_cycle_bytes = state->gc_live_bytes > ELF_GC_NEXT_MAX / 2
		? ELF_GC_NEXT_MAX
		: state->gc_live_bytes * 2;
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
	state->gc_collection_count += 1;
	u64 start = dy_counter();
	u32 freed_count = gc_collect(state);
	u64 elapsed_ticks = dy_counter() - start;
	u64 frequency = dy_counter_frequency();
	u64 elapsed_ns = frequency
		? (u64)((f64)elapsed_ticks * 1000000000.0 / (f64)frequency)
		: 0;
	state->gc_collection_time_ns += elapsed_ns;
	state->gc_last_collection_time_ns = elapsed_ns;
	state->gc_max_collection_time_ns = MAX(state->gc_max_collection_time_ns, elapsed_ns);
	ASSERT(freed_count <= previous_reference_count);

	gc_update_next_cycle(state);

	if (state->gc_live_bytes > state->gc_next_cycle_bytes) {
		report_runtime_error(state, ERROR_OUT_OF_MEMORY, "out of memory, %lluMB allocated, %lluMB threshold"
		, state->gc_live_bytes / MEGABYTES(1)
		, state->gc_next_cycle_bytes / MEGABYTES(1));
	}
}

static u32 gc_mark_table(elf_State *state, elf_Table *table);
static u32 gc_mark_array(elf_State *state, elf_Array *array);

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
			elf_Prototype *function = closure->prototype;
			ASSERT(function);

			for (u32 i = 0; i < function->captures; ++ i)
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
		else if (reference->type == ELF_OBJECT_ARRAY)
		{
			marked_count += gc_mark_array(state, (elf_Array *)reference);
		}
		else if (reference->type == ELF_OBJECT_STRING)
		{
		}
		else if (reference->type == ELF_OBJECT_BUFFER)
		{
		}
		else if (reference->type == ELF_OBJECT_ERROR)
		{
			elf_GCError *error = (elf_GCError *)reference;
			marked_count += gc_mark_reachable(state, (elf_Object *)error->message);
			if (error->data)
			{
				marked_count += gc_mark_reachable(state, (elf_Object *)error->data);
			}
		}
		else if (reference->type == ELF_OBJECT_USER)
		{
			elf_UserObject *user = (elf_UserObject *)reference;
			if (user->metatable)
				marked_count += gc_mark_reachable(state, (elf_Object *)user->metatable);
		}
	}
	return marked_count;
}

static u32 gc_mark_array(elf_State *state, elf_Array *array)
{
	u32 marked_count = 0;
	for (u32 i = 0; i < array->count; ++i)
	{
		elf_Value value = array->items[i];
		if (value_is_object(value))
		{
			marked_count += gc_mark_reachable(state, value_as_object(value));
		}
	}
	return marked_count;
}

static u32 gc_mark_table(elf_State *state, elf_Table *table)
{
	u32 marked_count = 0;

	for (u32 i = 0; i < table->capacity; ++i)
	{
		Entry entry = table->entries[i];
		if (elf_table_entry_is_key(entry))
		{
			elf_Value key = entry.key;
			if (value_is_object(key))
			{
				marked_count += gc_mark_reachable(state, value_as_object(key));
			}
			if (value_is_object(entry.value))
			{
				marked_count += gc_mark_reachable(state, value_as_object(entry.value));
			}
		}
	}
	return marked_count;
}
