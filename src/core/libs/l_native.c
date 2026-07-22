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
	check_numeric(state, value);
	return value_to_integer(value);
}
