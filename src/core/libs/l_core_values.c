//
// Core assertions, reflection, and value conversions.
//

static u32 core_mark_readonly(elf_State *state, elf_Object *object);

static u32 core_mark_table_readonly(elf_State *state, elf_Table *table)
{
	u32 count = 0;

	for (u32 index = 0; index < table->nentries; ++index)
	{
		elf_Value key = entry_key_value(table->entries[index]);
		if (value_is_object(key)) count += core_mark_readonly(state, value_as_object(key));
	}

	for (u32 index = 0; index < elf_array_length(table); ++index)
	{
		elf_Value value = elf_array_get(state, table, index);
		if (value_is_object(value)) count += core_mark_readonly(state, value_as_object(value));
	}

	return count;
}

static u32 core_mark_readonly(elf_State *state, elf_Object *object)
{
	ASSERT(object);
	if (object->status & ELF_OBJECT_READONLY) return 0;

	// Mark before following references so cyclic tables terminate.
	object->status |= ELF_OBJECT_READONLY;
	u32 count = 1;
	if (object->type == ELF_OBJECT_TABLE)
	{
		count += core_mark_table_readonly(state, (elf_Table *)object);
	}
	return count;
}

ELF_FUNCTION(l_core_assert)
{
	lib_check_arg_count(S, "assert", nargs, 1, 2);
	elf_Value condition = load_value(S, 1);
	check_value_type_rule(S, condition, TRULE_NUMERIC);

	const char *message = "assertion failed";
	if (nargs > 2) message = lib_load_cstr(S, 2);
	if (!value_to_integer(condition))
	{
		elf_report_runtime_error(S, RUNTIME_ERROR_GENERIC, -1, "%s", message);
	}
	return 0;
}

ELF_FUNCTION(l_core_metatable)
{
	lib_check_arg_count(S, "metatable", nargs, 1, 1);
	elf_Table *metatable = elf_get_type_metatable(S, load_value(S, 1));
	if (metatable) push_table(S, metatable);
	else elf_push_nil(S);
	return 1;
}

ELF_FUNCTION(l_core_freeze)
{
	lib_check_arg_count(S, "freeze", nargs, 1, 1);
	elf_Value value = load_value(S, 1);
	check_value_type_rule(S, value, TRULE_OBJECT);
	core_mark_readonly(S, value_as_object(value));
	push_value(S, value);
	return 1;
}

ELF_FUNCTION(l_core_is_readonly)
{
	lib_check_arg_count(S, "is_readonly", nargs, 1, 1);
	elf_Value value = load_value(S, 1);
	check_value_type_rule(S, value, TRULE_OBJECT);
	push_value(S, value_from_integer((value_as_object(value)->status & ELF_OBJECT_READONLY) != 0));
	return 1;
}

ELF_FUNCTION(l_core_type_of)
{
	lib_check_arg_count(S, "type_of", nargs, 1, 1);
	const char *name = value_type_name(value_type(load_value(S, 1)));
	push_value(S, value_from_atom(elf_atom_from_data(S, name)));
	return 1;
}

ELF_FUNCTION(l_core_is_atom)
{
	lib_check_arg_count(S, "is_atom", nargs, 1, 1);
	push_value(S, value_from_integer(value_is_atom(load_value(S, 1))));
	return 1;
}

ELF_FUNCTION(l_core_is_numeric)
{
	lib_check_arg_count(S, "is_numeric", nargs, 1, 1);
	push_value(S, value_from_integer(value_is_numeric(load_value(S, 1))));
	return 1;
}

ELF_FUNCTION(l_core_to_number)
{
	lib_check_arg_count(S, "to_number", nargs, 1, 1);
	elf_Value value = load_value(S, 1);
	check_value_type_rule(S, value, TRULE_NUMERIC);
	push_value(S, value_from_number(value_to_number(value)));
	return 1;
}

ELF_FUNCTION(l_core_to_integer)
{
	lib_check_arg_count(S, "to_integer", nargs, 1, 1);
	elf_Value value = load_value(S, 1);
	check_value_type_rule(S, value, TRULE_NUMERIC);
	push_value(S, value_from_integer(value_to_integer(value)));
	return 1;
}
