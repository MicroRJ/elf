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
	if (!lib_check_arg_count(S, "error.message", nargs - 1, 0, 0)) {
		return 1;
	}
	push_value(S, str_value(error_receiver(S)->message));
	return 1;
}

ELF_FUNCTION(l_error_code)
{
	if (!lib_check_arg_count(S, "error.code", nargs - 1, 0, 0)) {
		return 1;
	}
	elf_push_cstr(S, elf_error_code_name(error_receiver(S)->code));
	return 1;
}

ELF_FUNCTION(l_error_data)
{
	if (!lib_check_arg_count(S, "error.data", nargs - 1, 0, 0)) {
		return 1;
	}
	elf_Table *data = error_receiver(S)->data;
	push_value(S, data ? value_from_table(data) : value_nil());
	return 1;
}

static const elf_Binding l_error[] = {
	{"code",    l_error_code},
	{"message", l_error_message},
	{"data",    l_error_data},
};

static elf_Table *elf_lib_error(elf_State *state)
{
	return new_binding_table(state, l_error, ARRAY_COUNT(l_error));
}
