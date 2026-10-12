//
// See Copyright Notice In elf.h
//

static elf_Table *table_receiver(elf_State *state)
{
	elf_Value value = load_value(state, 0);
	check_value_type(state, value, ELF_VALUE_TYPE_TABLE);
	return value_as_table(value);
}

typedef struct
{
	elf_Table *left;
	elf_Table *right;
}
Table_Map_Equal_Pair;

typedef struct
{
	elf_State           *state;
	Table_Map_Equal_Pair *pairs;
	u32                    count;
	u32                 capacity;
}
Table_Map_Equal_Context;

static b32 table_values_map_equal(Table_Map_Equal_Context *context, elf_Value left, elf_Value right);

static u32 table_live_entry_count(elf_Table *table)
{
	u32 count = 0;
	for (u32 i = 0; i < table->nentries; ++i) {
		if (entry_is_key(table->entries[i])) count += 1;
	}
	return count;
}

static void table_map_equal_add_pair(Table_Map_Equal_Context *context, elf_Table *left, elf_Table *right)
{
	if (context->count == context->capacity)
	{
		u32 capacity = context->capacity ? context->capacity * 2 : 16;
		Table_Map_Equal_Pair *pairs = elf_realloc(context->state, context->pairs,
			(u64)context->capacity * sizeof(*context->pairs),
			(u64)capacity * sizeof(*context->pairs));
		ASSERT(pairs != 0);
		context->pairs = pairs;
		context->capacity = capacity;
	}
	context->pairs[context->count++] = (Table_Map_Equal_Pair){left, right};
}


// Note, RJ
// :map_equal
//
// * Number of live entries must be the same; dead keys add superficial volume.
// * Each entry must be found, each entry must match, each value of each entry must match. Ordering
// does not matter.
//
static b32 tables_map_equal(Table_Map_Equal_Context *context, elf_Table *left, elf_Table *right)
{
	for (u32 i = 0; i < context->count; ++i)
	{
		Table_Map_Equal_Pair pair = context->pairs[i];
		if (pair.left == left && pair.right == right) return true;
	}

	table_map_equal_add_pair(context, left, right);

	if (left->count != right->count || table_live_entry_count(left) != table_live_entry_count(right)) {
		return false;
	}

	for (u32 i = 0; i < left->nentries; ++i)
	{
		Entry left_entry = left->entries[i];
		if (!entry_is_key(left_entry)) continue;
		Entry *right_entry = table_find_entry(right->entries, right->nentries, entry_key_value(left_entry));
		if (!right_entry || !table_values_map_equal(context, left->array[entry_index(left_entry)], right->array[entry_index(*right_entry)])) {
			return false;
		}
	}
	return true;
}

static b32 table_values_map_equal(Table_Map_Equal_Context *context, elf_Value left, elf_Value right)
{
	if (value_is_numeric(left) && value_is_numeric(right))
	{
		if (value_is_number(left) || value_is_number(right)) {
			return value_to_number(left) == value_to_number(right);
		}
		return value_as_integer(left) == value_as_integer(right);
	}
	if (value_type(left) != value_type(right)) return false;
	if (value_is_string(left)) {
		return strings_equal(value_as_string(left), value_as_string(right));
	}
	if (value_is_table(left)) {
		return tables_map_equal(context, value_as_table(left), value_as_table(right));
	}
	return left.x_i64 == right.x_i64;
}

ELF_FUNCTION(l_table_length)
{
	push_value(S, value_from_integer(elf_deprecated_array_length(table_receiver(S))));
	return 1;
}

ELF_FUNCTION(l_table_map_equal)
{
	lib_check_arg_count(S, "table.map_equal", nargs, 1, 1);
	elf_Table *left = table_receiver(S);
	elf_Value right_value = load_value(S, 1);
	if (!value_is_table(right_value))
	{
		push_value(S, value_from_integer(false));
		return 1;
	}

	Table_Map_Equal_Context context = {.state = S};
	b32 equal = tables_map_equal(&context, left, value_as_table(right_value));
	elf_free(S, context.pairs,
		(u64)context.capacity * sizeof(*context.pairs));
	push_value(S, value_from_integer(equal));
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
	elf_Array *result = push_new_array(S);
	for (u32 value_index = 0; value_index < table->count; ++value_index)
	{
		for (u32 slot = 0; slot < table->nentries; ++slot)
		{
			Entry entry = table->entries[slot];
			if (entry_is_key(entry) && entry_index(entry) == value_index)
			{
				elf_array_add(S, result, entry_key_value(entry));
			}
		}
	}
	return 1;
}

ELF_FUNCTION(l_table_values)
{
	elf_Table *table = table_receiver(S);
	elf_Array *result = push_new_array(S);
	for (u32 i = 0; i < table->count; ++i)
	{
		elf_array_add(S, result, table->array[i]);
	}
	return 1;
}

ELF_FUNCTION(l_table_pairs)
{
	elf_Table *table = table_receiver(S);
	elf_Array *result = push_new_array(S);
	for (u32 value_index = 0; value_index < table->count; ++value_index)
	{
		for (u32 slot = 0; slot < table->nentries; ++slot)
		{
			Entry entry = table->entries[slot];
			if (!entry_is_key(entry) || entry_index(entry) != value_index)
			{
				continue;
			}
			elf_Array *pair = elf_array_new(S);
			elf_array_add(S, pair, entry_key_value(entry));
			elf_array_add(S, pair, table->array[value_index]);
			elf_array_add(S, result, value_from_array(pair));
		}
	}
	return 1;
}

static elf_Table *table_clone(elf_State *state, elf_Table *source)
{
	elf_Table *result = elf_new_table_rogue(state);
	for (u32 i = 0; i < source->count; ++i) {
		elf_deprecated_array_add(state, result, source->array[i]);
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

static const elf_Binding l_table[] = {
	{"length",  l_table_length},
	{"size",    l_table_length},
	{"map_equal", l_table_map_equal},
	{"has",     l_table_has},
	{"haskey",  l_table_has},
	{"delete",  l_table_delete},
	{"clear",   l_table_clear},
	{"keys",    l_table_keys},
	{"values",  l_table_values},
	{"pairs",   l_table_pairs},
	{"clone",   l_table_clone},
	{"merge",   l_table_merge},
};

static elf_Table *elf_lib_table(elf_State *state)
{
	return new_binding_table(state, l_table, ARRAY_COUNT(l_table));
}
