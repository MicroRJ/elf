//
// See Copyright Notice In elf.h
//

#define ARRAY_INITIAL_CAPACITY 8

static void array_check_write(elf_State *state, elf_Array *array)
{
	if (array->obj.status & ELF_OBJECT_READONLY)
	{
		elf_report_runtime_error(state, ERROR_READONLY, "attempted to write to readonly object");
	}
}

static void array_reserve(elf_State *state, elf_Array *array, u32 min_capacity)
{
	if (array->capacity < min_capacity)
	{
		u32 capacity = array->capacity ? array->capacity : ARRAY_INITIAL_CAPACITY;
		while (capacity < min_capacity)
		{
			capacity <<= 1;
		}

		elf_Value *items = elf_realloc(state, array->items, (u64)array->capacity * sizeof(*array->items), (u64)capacity * sizeof(*array->items));
		ASSERT(items != 0);
		array->items = items;
		value_zero_many(array->items + array->capacity, capacity - array->capacity);
		array->capacity = capacity;
	}
}

static elf_Value *array_slot(elf_State *state, elf_Array *array, u32 index)
{
	check_array_index(state, index, array->count);
	return array->items + index;
}

elf_Array *elf_array_new(elf_State *state)
{
	return elf_gc_alloc(state, ELF_OBJECT_ARRAY, sizeof(elf_Array));
}

u32 elf_array_length(elf_Array *array)
{
	return array->count;
}

u32 elf_array_add(elf_State *state, elf_Array *array, elf_Value value)
{
	array_check_write(state, array);
	array_reserve(state, array, array->count + 1);
	u32 index = array->count++;
	array->items[index] = value;
	return index;
}

elf_Value elf_array_get(elf_State *state, elf_Array *array, u32 index)
{
	return *array_slot(state, array, index);
}

void elf_array_set(elf_State *state, elf_Array *array, u32 index, elf_Value value)
{
	array_check_write(state, array);
	*array_slot(state, array, index) = value;
}

void elf_array_remove(elf_State *state, elf_Array *array, u32 index, u32 count)
{
	array_check_write(state, array);
	if (index > array->count || count > array->count - index)
	{
		elf_report_runtime_error(state, ERROR_OUT_OF_RANGE, "array range out of bounds: index %u, count %u, length %u", index, count, array->count);
	}

	if (count != 0)
	{
		u32 tail_count = array->count - index - count;
		memmove(array->items + index, array->items + index + count, (u64)tail_count * sizeof(*array->items));
		array->count -= count;
		value_zero_many(array->items + array->count, count);
	}
}

void elf_array_splice(elf_State *state, elf_Array *array, u32 index, u32 delete_count, elf_Array *replacement)
{
	array_check_write(state, array);
	if (index > array->count || delete_count > array->count - index)
	{
		elf_report_runtime_error(state, ERROR_OUT_OF_RANGE, "array splice range is out of bounds");
	}
	u32 insert_count = replacement->count;
	u32 count_after_delete = array->count - delete_count;
	if (insert_count > UINT32_MAX - count_after_delete)
	{
		elf_report_runtime_error(state, ERROR_OUT_OF_RANGE, "array size exceeds the supported range");
	}

	elf_Value *insert_items = replacement->items;
	elf_Value *insert_copy = 0;
	if (replacement == array && insert_count)
	{
		u64 insert_size = (u64)insert_count * sizeof(*insert_copy);
		insert_copy = elf_alloc(state, insert_size);
		if (!insert_copy) {
			elf_report_runtime_error(state, ERROR_OUT_OF_MEMORY, "could not allocate %u values for array splice", insert_count);
		}
		copy_memory(insert_copy, insert_items, insert_size);
		insert_items = insert_copy;
	}

	u32 old_count = array->count;
	u32 new_count = count_after_delete + insert_count;
	array_reserve(state, array, new_count);
	u32 tail_count = old_count - index - delete_count;
	if (tail_count) {
		memmove(array->items + index + insert_count, array->items + index + delete_count, (u64)tail_count * sizeof(*array->items));
	}
	if (insert_count) {
		copy_memory(array->items + index, insert_items, (u64)insert_count * sizeof(*array->items));
	}
	if (new_count < old_count) {
		value_zero_many(array->items + new_count, old_count - new_count);
	}
	array->count = new_count;
	elf_free(state, insert_copy, insert_copy ? (u64)insert_count * sizeof(*insert_copy) : 0);
}

u32 elf_array_insert(elf_State *state, elf_Array *array, u32 index, elf_Value value)
{
	array_check_write(state, array);
	if (index > array->count)
	{
		elf_report_runtime_error(state, ERROR_OUT_OF_RANGE, "array insert index %u is out of bounds for length %u", index, array->count);
	}

	array_reserve(state, array, array->count + 1);
	memmove(array->items + index + 1, array->items + index, (u64)(array->count - index) * sizeof(*array->items));
	array->items[index] = value;
	array->count += 1;
	return index;
}

void elf_array_swap(elf_State *state, elf_Array *array, u32 left, u32 right)
{
	array_check_write(state, array);
	elf_Value *left_slot = array_slot(state, array, left);
	elf_Value *right_slot = array_slot(state, array, right);
	elf_Value temporary = *left_slot;
	*left_slot = *right_slot;
	*right_slot = temporary;
}
