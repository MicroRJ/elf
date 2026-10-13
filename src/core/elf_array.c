//
// See Copyright Notice In elf.h
//

#define ARRAY_INITIAL_CAPACITY 8

static void array_check_write(elf_State *state, elf_Array *array)
{
	if (array->obj.status & ELF_OBJECT_READONLY)
	{
		elf_report_runtime_error(state, RUNTIME_ERROR_GENERIC, NO_BYTE, "attempted to write to readonly object");
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
	check_array_index(state, NO_BYTE, index, array->count);
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
		elf_report_runtime_error(state, RUNTIME_ERROR_GENERIC, NO_BYTE, "array range out of bounds: index %u, count %u, length %u", index, count, array->count);
	}

	if (count != 0)
	{
		u32 tail_count = array->count - index - count;
		memmove(array->items + index, array->items + index + count, (u64)tail_count * sizeof(*array->items));
		array->count -= count;
		value_zero_many(array->items + array->count, count);
	}
}

u32 elf_array_insert(elf_State *state, elf_Array *array, u32 index, elf_Value value)
{
	array_check_write(state, array);
	if (index > array->count)
	{
		elf_report_runtime_error(state, RUNTIME_ERROR_GENERIC, NO_BYTE, "array insert index %u is out of bounds for length %u", index, array->count);
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
	value_swap(array_slot(state, array, left), array_slot(state, array, right));
}
