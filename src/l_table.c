//
// See Copyright Notice In elf.h
//

static elf_Table *table_receiver(elf_State *state)
{
	elf_Value value = load_value(state, 0);
	check_value_type(state, value, ELF_VALUE_TYPE_TABLE);
	return value_as_table(value);
}

ELF_FUNCTION(l_table_length)
{
	if (!lib_check_arg_count(S, "table.length", nargs - 1, 0, 0)) {
		return 1;
	}
	push_value(S, int_value(table_receiver(S)->count));
	return 1;
}

ELF_FUNCTION(l_table_has)
{
	if (!lib_check_arg_count(S, "table.has", nargs - 1, 1, 1)) {
		return 1;
	}
	push_value(S, int_value(elf_table_contains(S, table_receiver(S), load_value(S, 1))));
	return 1;
}

ELF_FUNCTION(l_table_delete)
{
	if (!lib_check_arg_count(S, "table.delete", nargs - 1, 1, 1)) {
		return 1;
	}
	elf_Value removed = value_nil();
	elf_table_delete(S, table_receiver(S), load_value(S, 1), &removed);
	push_value(S, removed);
	return 1;
}

ELF_FUNCTION(l_table_clear)
{
	if (!lib_check_arg_count(S, "table.clear", nargs - 1, 0, 0)) {
		return 1;
	}
	elf_Table *table = table_receiver(S);
	elf_table_clear(S, table);
	push_table(S, table);
	return 1;
}

ELF_FUNCTION(l_table_clone)
{
	if (!lib_check_arg_count(S, "table.clone", nargs - 1, 0, 0)) {
		return 1;
	}
	elf_Table *table = table_receiver(S);
	elf_Table *result = push_new_table(S);
	for (u32 i = 0; i < table->capacity; ++i)
	{
		Entry entry = table->entries[i];
		if (elf_table_entry_is_key(entry))
		{
			elf_table_set(S, result, entry.key, entry.value);
		}
	}
	return 1;
}

ELF_FUNCTION(l_table_keys)
{
	if (!lib_check_arg_count(S, "table.keys", nargs - 1, 0, 0)) {
		return 1;
	}
	elf_Table *table = table_receiver(S);
	elf_Array *result = push_new_array(S);
	for (u32 i = 0; i < table->capacity; ++i)
	{
		Entry entry = table->entries[i];
		if (elf_table_entry_is_key(entry))
		{
			elf_array_add(S, result, entry.key);
		}
	}
	return 1;
}

static const elf_Binding l_table[] = {
	{"length", l_table_length},
	{"size",   l_table_length},
	{"has",    l_table_has},
	{"delete", l_table_delete},
	{"clear",  l_table_clear},
	{"clone",  l_table_clone},
	{"keys",   l_table_keys},
};

static elf_Table *elf_lib_table(elf_State *state)
{
	return new_binding_table(state, l_table, ARRAY_COUNT(l_table));
}
