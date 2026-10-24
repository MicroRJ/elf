//
// See Copyright Notice In elf.h
//

#define ARRAY_INITIAL_CAPACITY 8

static void array_check_write(elf_State *state, elf_Array *array)
{
	if (array->obj.status & ELF_OBJECT_READONLY)
	{
		report_runtime_error(state, ERROR_READONLY, "attempted to write to readonly object");
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
