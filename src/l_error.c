//
// See Copyright Notice In elf.h
//

static elf_GCError *error_receiver(elf_State *state)
{
	elf_Value value = load_value(state, 0);
	check_value_type(state, value, ELF_VALUE_TYPE_ERROR);
	return value_as_error(value);
}

ELF_FUNCTION(l_error_message)
{
	lib_check_arg_count(S, "error.message", nargs, 0, 0);
	push_value(S, value_from_string(error_receiver(S)->message));
	return 1;
}

ELF_FUNCTION(l_error_data)
{
	lib_check_arg_count(S, "error.data", nargs, 0, 0);
	elf_Table *data = error_receiver(S)->data;
	push_value(S, data ? value_from_table(data) : value_nil());
	return 1;
}

static const elf_Binding l_error[] = {
	{"message", l_error_message},
	{"data",    l_error_data},
};

static elf_Table *elf_lib_error(elf_State *state)
{
	return new_binding_table(state, l_error, ARRAY_COUNT(l_error));
}
