//
// Shared helpers for native standard-library bindings.
//

static void lib_check_arg_count(elf_State *state, const char *name, int nargs, int minimum, int maximum)
{
	int count = nargs - 1; // Slot zero is the receiver/implicit this value.
	if (count < minimum || count > maximum)
	{
		if (minimum == maximum)
		{
			elf_report_runtime_error(state, RUNTIME_ERROR_INVALID_ARGUMENT_COUNT, -1,
				"%s expected %i argument(s), got %i", name, minimum, count);
		}
		else
		{
			elf_report_runtime_error(state, RUNTIME_ERROR_INVALID_ARGUMENT_COUNT, -1,
				"%s expected %i to %i arguments, got %i", name, minimum, maximum, count);
		}
	}
}

static elf_String *lib_load_string(elf_State *state, u32 index)
{
	elf_Value value = load_value(state, index);
	check_value_type(state, value, ELF_VALUE_TYPE_ATOM);
	return value_as_atom(value);
}

static const char *lib_load_cstr(elf_State *state, u32 index)
{
	return atom_data(lib_load_string(state, index));
}

static i64 lib_load_integer(elf_State *state, u32 index)
{
	elf_Value value = load_value(state, index);
	check_value_type_rule(state, value, TRULE_NUMERIC);
	return value_to_integer(value);
}

static void lib_push_string(elf_State *state, const char *data, u32 size)
{
	push_value(state, value_from_atom(elf_atom_from_data_size(state, data, size)));
}

static elf_Value lib_string_value(elf_State *state, const char *text)
{
	return value_from_atom(elf_atom_from_data(state, text));
}

static void lib_set_integer_field(elf_State *state, elf_Table *table, const char *name, i64 value)
{
	elf_table_set(state, table, lib_string_value(state, name), value_from_integer(value));
}

static void lib_set_string_field(elf_State *state, elf_Table *table, const char *name,
	const char *data, u32 size)
{
	elf_Value value = value_from_atom(elf_atom_from_data_size(state, data, size));
	elf_table_set(state, table, lib_string_value(state, name), value);
}

static void lib_set_nil_field(elf_State *state, elf_Table *table, const char *name)
{
	elf_table_set(state, table, lib_string_value(state, name), value_nil());
}
