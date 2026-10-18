//
// See Copyright Notice In elf.h
//

static elf_Array *array_receiver(elf_State *state)
{
	elf_Value value = load_value(state, 0);
	check_value_type(state, value, ELF_VALUE_TYPE_ARRAY);
	return value_as_array(value);
}

static b32 array_normalize_index(elf_State *state, i64 index, u32 length, b32 allow_end, u32 *result)
{
	if (index < 0)
	{
		index += length;
	}

	i64 upper = allow_end ? length : (i64)length - 1;
	if (index < 0 || index > upper)
	{
		elf_report_runtime_error(state, ERROR_OUT_OF_RANGE, "array index %lld is out of bounds for length %u", index, length);
		return false;
	}

	*result = (u32)index;
	return true;
}

static elf_Value array_call(elf_State *state, elf_Value function, elf_Array *receiver, elf_Value a, elf_Value b, b32 pass_b)
{
	push_value(state, function);
	push_array(state, receiver);
	push_value(state, a);
	if (pass_b)
	{
		push_value(state, b);
	}
	elf_call(state, pass_b ? 3 : 2, 1);
	return pop_value(state);
}

ELF_FUNCTION(l_array_length)
{
	lib_check_arg_count(S, "array.length", nargs, 0, 0);
	push_value(S, value_from_integer(elf_array_length(array_receiver(S))));
	return 1;
}

ELF_FUNCTION(l_array_get)
{
	lib_check_arg_count(S, "array.get", nargs, 1, 1);
	elf_Array *array = array_receiver(S);
	u32 index = 0;
	if (!array_normalize_index(S, lib_load_integer(S, 1), array->count, false, &index))
	{
		return 0;
	}
	push_value(S, elf_array_get(S, array, index));
	return 1;
}

ELF_FUNCTION(l_array_set)
{
	lib_check_arg_count(S, "array.set", nargs, 2, 2);
	elf_Array *array = array_receiver(S);
	u32 index = 0;
	if (!array_normalize_index(S, lib_load_integer(S, 1), array->count, false, &index))
	{
		return 0;
	}
	elf_array_set(S, array, index, load_value(S, 2));
	push_array(S, array);
	return 1;
}

ELF_FUNCTION(l_array_add)
{
	elf_Array *array = array_receiver(S);
	u32 first = array->count;
	for (int arg = 1; arg < nargs; ++arg)
	{
		elf_array_add(S, array, load_value(S, arg));
	}
	push_value(S, value_from_integer(first));
	return 1;
}

ELF_FUNCTION(l_array_insert)
{
	lib_check_arg_count(S, "array.insert", nargs, 2, 2);
	elf_Array *array = array_receiver(S);
	u32 index = 0;
	if (!array_normalize_index(S, lib_load_integer(S, 1), array->count, true, &index))
	{
		return 0;
	}
	elf_array_insert(S, array, index, load_value(S, 2));
	push_array(S, array);
	return 1;
}

ELF_FUNCTION(l_array_pop)
{
	lib_check_arg_count(S, "array.pop", nargs, 0, 0);
	elf_Array *array = array_receiver(S);
	if (array->count == 0)
	{
		push_value(S, value_nil());
	}
	else
	{
		elf_Value value = array->items[array->count - 1];
		elf_array_remove(S, array, array->count - 1, 1);
		push_value(S, value);
	}
	return 1;
}

ELF_FUNCTION(l_array_remove)
{
	lib_check_arg_count(S, "array.remove", nargs, 1, 2);
	elf_Array *array = array_receiver(S);
	u32 index = 0;
	if (!array_normalize_index(S, lib_load_integer(S, 1), array->count, false, &index))
	{
		return 0;
	}

	u32 count = 1;
	if (nargs > 2)
	{
		i64 requested = lib_load_integer(S, 2);
		if (requested < 0 || (u64)requested > array->count - index)
		{
			elf_report_runtime_error(S, ERROR_OUT_OF_RANGE, "array remove count %lld is out of bounds", requested);
			return 0;
		}
		count = (u32)requested;
	}

	elf_Value first = array->items[index];
	elf_array_remove(S, array, index, count);
	push_value(S, first);
	return 1;
}

ELF_FUNCTION(l_array_splice)
{
	lib_check_arg_count(S, "array.splice", nargs, 3, 3);
	elf_Array *array = array_receiver(S);
	u32 index = 0;
	if (!array_normalize_index(S, lib_load_integer(S, 1), array->count, true, &index)) {
		return 0;
	}
	i64 requested = lib_load_integer(S, 2);
	if (requested < 0 || (u64)requested > array->count - index)
	{
		elf_report_runtime_error(S, ERROR_OUT_OF_RANGE, "array splice delete count %lld is out of bounds", requested);
		return 0;
	}
	elf_Value replacement_value = load_value(S, 3);
	check_value_type(S, replacement_value, ELF_VALUE_TYPE_ARRAY);
	elf_array_splice(S, array, index, (u32)requested, value_as_array(replacement_value));
	push_array(S, array);
	return 1;
}

ELF_FUNCTION(l_array_slice)
{
	lib_check_arg_count(S, "array.slice", nargs, 0, 2);
	elf_Array *array = array_receiver(S);
	u32 first = 0;
	u32 end = array->count;
	if (nargs > 1 && !array_normalize_index(S, lib_load_integer(S, 1), array->count, true, &first))
	{
		return 0;
	}
	if (nargs > 2 && !array_normalize_index(S, lib_load_integer(S, 2), array->count, true, &end))
	{
		return 0;
	}
	if (end < first)
	{
		elf_report_runtime_error(S, ERROR_OUT_OF_RANGE, "array slice end %u precedes start %u", end, first);
		return 0;
	}

	elf_Array *result = push_new_array(S);
	for (u32 i = first; i < end; ++i)
	{
		elf_array_add(S, result, array->items[i]);
	}
	return 1;
}

ELF_FUNCTION(l_array_clone)
{
	lib_check_arg_count(S, "array.clone", nargs, 0, 0);
	elf_Array *array = array_receiver(S);
	elf_Array *result = push_new_array(S);
	for (u32 i = 0; i < array->count; ++i)
	{
		elf_array_add(S, result, array->items[i]);
	}
	return 1;
}

ELF_FUNCTION(l_array_extend)
{
	elf_Array *array = array_receiver(S);
	for (int arg = 1; arg < nargs; ++arg)
	{
		elf_Value source_value = load_value(S, arg);
		check_value_type(S, source_value, ELF_VALUE_TYPE_ARRAY);
		elf_Array *source = value_as_array(source_value);
		u32 count = source->count;
		for (u32 i = 0; i < count; ++i)
		{
			elf_array_add(S, array, source->items[i]);
		}
	}
	push_array(S, array);
	return 1;
}

ELF_FUNCTION(l_array_swap)
{
	lib_check_arg_count(S, "array.swap", nargs, 2, 2);
	elf_Array *array = array_receiver(S);
	u32 left = 0;
	u32 right = 0;
	if (!array_normalize_index(S, lib_load_integer(S, 1), array->count, false, &left))
	{
		return 0;
	}
	if (!array_normalize_index(S, lib_load_integer(S, 2), array->count, false, &right))
	{
		return 0;
	}
	elf_array_swap(S, array, left, right);
	push_array(S, array);
	return 1;
}

ELF_FUNCTION(l_array_reverse)
{
	lib_check_arg_count(S, "array.reverse", nargs, 0, 0);
	elf_Array *array = array_receiver(S);
	for (u32 i = 0; i < array->count / 2; ++i)
	{
		elf_array_swap(S, array, i, array->count - i - 1);
	}
	push_array(S, array);
	return 1;
}

ELF_FUNCTION(l_array_map)
{
	lib_check_arg_count(S, "array.map", nargs, 1, 1);
	elf_Array *array = array_receiver(S);
	elf_Value function = load_value(S, 1);
	check_value_type_rule(S, function, TRULE_CALLABLE);
	elf_Array *result = push_new_array(S);
	for (u32 i = 0; i < array->count; ++i)
	{
		elf_Value mapped = array_call(S, function, array, array->items[i], value_from_integer(i), true);
		elf_array_add(S, result, mapped);
	}
	return 1;
}

ELF_FUNCTION(l_array_filter)
{
	lib_check_arg_count(S, "array.filter", nargs, 1, 1);
	elf_Array *array = array_receiver(S);
	elf_Value function = load_value(S, 1);
	check_value_type_rule(S, function, TRULE_CALLABLE);
	elf_Array *result = push_new_array(S);
	for (u32 i = 0; i < array->count; ++i)
	{
		elf_Value keep = array_call(S, function, array, array->items[i], value_from_integer(i), true);
		check_value_type(S, keep, ELF_VALUE_TYPE_INTEGER);
		if (value_as_integer(keep))
		{
			elf_array_add(S, result, array->items[i]);
		}
	}
	return 1;
}

static i64 array_compare(elf_State *state, elf_Value function, elf_Array *array, elf_Value left, elf_Value right)
{
	elf_Value result = array_call(state, function, array, left, right, true);
	check_value_type(state, result, ELF_VALUE_TYPE_INTEGER);
	return value_as_integer(result);
}

static void array_quicksort(elf_State *state, elf_Value function, elf_Array *array, u32 first, u32 end)
{
	while (end - first > 1)
	{
		elf_Value pivot = array->items[first];
		u32 lower = first;
		u32 upper = end - 1;
		u32 i = first + 1;
		while (i <= upper)
		{
			i64 order = array_compare(state, function, array, array->items[i], pivot);
			if (order < 0)
			{
				elf_array_swap(state, array, i++, lower++);
			}
			else if (order > 0)
			{
				elf_array_swap(state, array, i, upper--);
			}
			else
			{
				++i;
			}
		}

		if (lower - first < end - i)
		{
			array_quicksort(state, function, array, first, lower);
			first = i;
		}
		else
		{
			array_quicksort(state, function, array, i, end);
			end = lower;
		}
	}
}

ELF_FUNCTION(l_array_sort)
{
	lib_check_arg_count(S, "array.sort", nargs, 1, 1);
	elf_Array *array = array_receiver(S);
	elf_Value function = load_value(S, 1);
	check_value_type_rule(S, function, TRULE_CALLABLE);
	array_quicksort(S, function, array, 0, array->count);
	push_array(S, array);
	return 1;
}

static const elf_Binding l_array[] = {
	{"length",  l_array_length},
	{"size",    l_array_length},
	{"get",     l_array_get},
	{"idx",     l_array_get},
	{"set",     l_array_set},
	{"repl",    l_array_set},
	{"add",     l_array_add},
	{"push",    l_array_add},
	{"insert",  l_array_insert},
	{"pop",     l_array_pop},
	{"remove",  l_array_remove},
	{"splice",  l_array_splice},
	{"slice",   l_array_slice},
	{"clone",   l_array_clone},
	{"extend",  l_array_extend},
	{"swap",    l_array_swap},
	{"reverse", l_array_reverse},
	{"map",     l_array_map},
	{"filter",  l_array_filter},
	{"sort",    l_array_sort},
};

static elf_Table *elf_lib_array(elf_State *state)
{
	return new_binding_table(state, l_array, ARRAY_COUNT(l_array));
}
