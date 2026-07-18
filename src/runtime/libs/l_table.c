//
// Tables are Elf's single map/array collection. The array operations work on
// the table's ordered value storage; map fields retain their slot bindings.
//

static elf_Table *table_receiver(elf_State *state)
{
	elf_Value value = load_value(state, 0);
	check_value_type(state, value, ELF_VALUE_TYPE_TABLE);
	return value_as_table(value);
}

static b32 table_normalize_index(elf_State *state, i64 index, u32 length,
	b32 allow_end, u32 *result)
{
	if (index < 0) index += length;
	i64 upper = allow_end ? length : (i64)length - 1;
	if (index < 0 || index > upper)
	{
		report_runtime_error(state, RUNTIME_ERROR_GENERIC, NO_BYTE,
			"array index %lld is out of bounds for length %u", index, length);
		return false;
	}
	*result = (u32)index;
	return true;
}

static elf_Value table_call(elf_State *state, elf_Value function,
	elf_Table *receiver, elf_Value a, elf_Value b, b32 pass_b)
{
	push_value(state, function);
	push_table(state, receiver);
	push_value(state, a);
	if (pass_b) push_value(state, b);
	elf_call(state, pass_b ? 3 : 2, 1);
	return pop_value(state);
}

ELF_FUNCTION(l_table_length)
{
	push_value(S, value_from_integer(elf_array_len(table_receiver(S))));
	return 1;
}

ELF_FUNCTION(l_table_has)
{
	push_value(S, value_from_integer(elf_table_contains(S, table_receiver(S), load_value(S, 1))));
	return 1;
}

ELF_FUNCTION(l_table_delete)
{
	elf_Value removed = value_nil();
	elf_table_delete(S, table_receiver(S), load_value(S, 1), &removed);
	push_value(S, removed);
	return 1;
}

ELF_FUNCTION(l_table_clear)
{
	elf_Table *table = table_receiver(S);
	elf_table_clear(S, table);
	push_table(S, table);
	return 1;
}

ELF_FUNCTION(l_table_keys)
{
	elf_Table *table = table_receiver(S);
	elf_Table *result = elf_push_new_table(S);
	for (u32 value_index = 0; value_index < table->count; ++value_index) {
		for (u32 slot = 0; slot < table->nentries; ++slot) {
			Entry entry = table->entries[slot];
			if (entry_is_key(entry) && entry_index(entry) == value_index) {
				elf_array_add(S, result, entry_key_value(entry));
			}
		}
	}
	return 1;
}

ELF_FUNCTION(l_table_values)
{
	elf_Table *table = table_receiver(S);
	elf_Table *result = elf_push_new_table(S);
	for (u32 i = 0; i < table->count; ++i) {
		elf_array_add(S, result, table->array[i]);
	}
	return 1;
}

ELF_FUNCTION(l_table_pairs)
{
	elf_Table *table = table_receiver(S);
	elf_Table *result = elf_push_new_table(S);
	for (u32 value_index = 0; value_index < table->count; ++value_index)
	{
		for (u32 slot = 0; slot < table->nentries; ++slot)
		{
			Entry entry = table->entries[slot];
			if (!entry_is_key(entry) || entry_index(entry) != value_index) continue;
			elf_Table *pair = elf_table_new_unrooted(S);
			elf_array_add(S, pair, entry_key_value(entry));
			elf_array_add(S, pair, table->array[value_index]);
			elf_array_add(S, result, value_from_table(pair));
		}
	}
	return 1;
}

static elf_Table *table_clone(elf_State *state, elf_Table *source)
{
	elf_Table *result = elf_table_new_unrooted(state);
	for (u32 i = 0; i < source->count; ++i) {
		elf_array_add(state, result, source->array[i]);
	}
	for (u32 i = 0; i < source->nentries; ++i) {
		Entry entry = source->entries[i];
		if (entry_is_key(entry)) {
			elf_table_bind_to_index(state, result, entry_key_value(entry), entry_index(entry));
		}
	}
	return result;
}

ELF_FUNCTION(l_table_clone)
{
	push_table(S, table_clone(S, table_receiver(S)));
	return 1;
}

ELF_FUNCTION(l_table_merge)
{
	elf_Table *result = table_clone(S, table_receiver(S));
	push_table(S, result);
	for (int arg = 1; arg < nargs; ++arg)
	{
		elf_Value source_value = load_value(S, arg);
		check_value_type(S, source_value, ELF_VALUE_TYPE_TABLE);
		elf_Table *source = value_as_table(source_value);
		for (u32 i = 0; i < source->nentries; ++i) {
			Entry entry = source->entries[i];
			if (entry_is_key(entry)) {
				elf_table_set(S, result, entry_key_value(entry), source->array[entry_index(entry)]);
			}
		}
	}
	return 1;
}

ELF_FUNCTION(l_array_get)
{
	elf_Table *table = table_receiver(S);
	u32 index = 0;
	if (!table_normalize_index(S, lib_load_integer(S, 1), table->count, false, &index)) return 0;
	push_value(S, elf_array_get(S, table, index));
	return 1;
}

ELF_FUNCTION(l_array_set)
{
	elf_Table *table = table_receiver(S);
	u32 index = 0;
	if (!table_normalize_index(S, lib_load_integer(S, 1), table->count, false, &index)) return 0;
	elf_array_set(S, table, index, load_value(S, 2));
	push_table(S, table);
	return 1;
}

ELF_FUNCTION(l_array_add)
{
	elf_Table *table = table_receiver(S);
	u32 first = table->count;
	for (int arg = 1; arg < nargs; ++arg) {
		elf_array_add(S, table, load_value(S, arg));
	}
	push_value(S, value_from_integer(first));
	return 1;
}

ELF_FUNCTION(l_array_insert)
{
	elf_Table *table = table_receiver(S);
	u32 index = 0;
	if (!table_normalize_index(S, lib_load_integer(S, 1), table->count, true, &index)) return 0;
	elf_array_insert(S, table, index, load_value(S, 2));
	push_table(S, table);
	return 1;
}

ELF_FUNCTION(l_array_pop)
{
	elf_Table *table = table_receiver(S);
	if (table->count == 0) {
		push_value(S, value_nil());
		return 1;
	}
	elf_Value value = table->array[table->count - 1];
	elf_array_remove(S, table, table->count - 1, 1);
	push_value(S, value);
	return 1;
}

ELF_FUNCTION(l_array_remove)
{
	elf_Table *table = table_receiver(S);
	u32 index = 0;
	if (!table_normalize_index(S, lib_load_integer(S, 1), table->count, false, &index)) return 0;
	u32 count = 1;
	if (nargs > 2) {
		i64 requested = lib_load_integer(S, 2);
		if (requested < 0 || (u64)requested > table->count - index) {
			report_runtime_error(S, RUNTIME_ERROR_GENERIC, NO_BYTE,
				"array remove count %lld is out of bounds", requested);
			return 0;
		}
		count = (u32)requested;
	}
	elf_Value first = table->array[index];
	elf_array_remove(S, table, index, count);
	push_value(S, first);
	return 1;
}

ELF_FUNCTION(l_array_slice)
{
	elf_Table *table = table_receiver(S);
	u32 first = 0;
	u32 end = table->count;
	if (nargs > 1 && !table_normalize_index(S, lib_load_integer(S, 1), table->count, true, &first)) return 0;
	if (nargs > 2 && !table_normalize_index(S, lib_load_integer(S, 2), table->count, true, &end)) return 0;
	if (end < first) {
		report_runtime_error(S, RUNTIME_ERROR_GENERIC, NO_BYTE,
			"array slice end %u precedes start %u", end, first);
		return 0;
	}
	elf_Table *result = elf_push_new_table(S);
	for (u32 i = first; i < end; ++i) elf_array_add(S, result, table->array[i]);
	return 1;
}

ELF_FUNCTION(l_array_extend)
{
	elf_Table *table = table_receiver(S);
	for (int arg = 1; arg < nargs; ++arg)
	{
		elf_Value source_value = load_value(S, arg);
		check_value_type(S, source_value, ELF_VALUE_TYPE_TABLE);
		elf_Table *source = value_as_table(source_value);
		u32 count = source->count;
		for (u32 i = 0; i < count; ++i) elf_array_add(S, table, source->array[i]);
	}
	push_table(S, table);
	return 1;
}

ELF_FUNCTION(l_array_swap)
{
	elf_Table *table = table_receiver(S);
	u32 left = 0;
	u32 right = 0;
	if (!table_normalize_index(S, lib_load_integer(S, 1), table->count, false, &left)) return 0;
	if (!table_normalize_index(S, lib_load_integer(S, 2), table->count, false, &right)) return 0;
	elf_array_swap(S, table, left, right);
	push_table(S, table);
	return 1;
}

ELF_FUNCTION(l_array_reverse)
{
	elf_Table *table = table_receiver(S);
	for (u32 i = 0; i < table->count / 2; ++i) {
		elf_array_swap(S, table, i, table->count - i - 1);
	}
	push_table(S, table);
	return 1;
}

ELF_FUNCTION(l_array_map)
{
	elf_Table *table = table_receiver(S);
	elf_Value function = load_value(S, 1);
	check_value_type_rule(S, function, TRULE_CALLABLE);
	elf_Table *result = elf_push_new_table(S);
	for (u32 i = 0; i < table->count; ++i) {
		elf_Value mapped = table_call(S, function, table, table->array[i], value_from_integer(i), true);
		elf_array_add(S, result, mapped);
	}
	return 1;
}

ELF_FUNCTION(l_array_filter)
{
	elf_Table *table = table_receiver(S);
	elf_Value function = load_value(S, 1);
	check_value_type_rule(S, function, TRULE_CALLABLE);
	elf_Table *result = elf_push_new_table(S);
	for (u32 i = 0; i < table->count; ++i) {
		elf_Value keep = table_call(S, function, table, table->array[i], value_from_integer(i), true);
		check_value_type(S, keep, ELF_VALUE_TYPE_INTEGER);
		if (value_as_integer(keep)) elf_array_add(S, result, table->array[i]);
	}
	return 1;
}

static i64 table_compare(elf_State *state, elf_Value function, elf_Table *table,
	elf_Value left, elf_Value right)
{
	elf_Value result = table_call(state, function, table, left, right, true);
	check_value_type(state, result, ELF_VALUE_TYPE_INTEGER);
	return value_as_integer(result);
}

static void table_quicksort(elf_State *state, elf_Value function, elf_Table *table,
	u32 first, u32 end)
{
	while (end - first > 1)
	{
		elf_Value pivot = table->array[first];
		u32 lower = first;
		u32 upper = end - 1;
		u32 i = first + 1;
		while (i <= upper) {
			i64 order = table_compare(state, function, table, table->array[i], pivot);
			if (order < 0) elf_array_swap(state, table, i++, lower++);
			else if (order > 0) elf_array_swap(state, table, i, upper--);
			else ++i;
		}
		if (lower - first < end - i) {
			table_quicksort(state, function, table, first, lower);
			first = i;
		} else {
			table_quicksort(state, function, table, i, end);
			end = lower;
		}
	}
}

ELF_FUNCTION(l_array_sort)
{
	elf_Table *table = table_receiver(S);
	elf_Value function = load_value(S, 1);
	check_value_type_rule(S, function, TRULE_CALLABLE);
	table_quicksort(S, function, table, 0, table->count);
	push_table(S, table);
	return 1;
}

static const elf_Binding l_table[] = {
	{"length",  l_table_length},
	{"size",    l_table_length},
	{"has",     l_table_has},
	{"haskey",  l_table_has},
	{"delete",  l_table_delete},
	{"clear",   l_table_clear},
	{"keys",    l_table_keys},
	{"values",  l_table_values},
	{"pairs",   l_table_pairs},
	{"clone",   l_table_clone},
	{"merge",   l_table_merge},
	{"get",     l_array_get},
	{"idx",     l_array_get},
	{"set",     l_array_set},
	{"repl",    l_array_set},
	{"add",     l_array_add},
	{"push",    l_array_add},
	{"insert",  l_array_insert},
	{"pop",     l_array_pop},
	{"remove",  l_array_remove},
	{"slice",   l_array_slice},
	{"extend",  l_array_extend},
	{"swap",    l_array_swap},
	{"reverse", l_array_reverse},
	{"map",     l_array_map},
	{"filter",  l_array_filter},
	{"sort",    l_array_sort},
};

static elf_Table *elf_lib_table(elf_State *state)
{
	return new_binding_table(state, l_table, ARRAY_COUNT(l_table));
}
