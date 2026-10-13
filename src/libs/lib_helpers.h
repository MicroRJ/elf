#ifndef ELF_LIB_HELPERS_H
#define ELF_LIB_HELPERS_H

typedef elf_b32 b32;
typedef elf_i32 i32;
typedef elf_u32 u32;
typedef elf_i64 i64;
typedef elf_u64 u64;
typedef elf_f64 f64;

typedef struct
{
	char *name;
	elf_Function function;
}
elf_Binding;

static void lib_check_arg_count(elf_State *state, const char *name, int nargs, int minimum, int maximum)
{
	int count = nargs - 1;
	if (count < minimum || count > maximum)
	{
		if (minimum == maximum)
		{
			elf_report_runtime_error(state, RUNTIME_ERROR_INVALID_ARGUMENT_COUNT, -1, "%s expected %i argument(s), got %i", name, minimum, count);
		}
		else
		{
			elf_report_runtime_error(state, RUNTIME_ERROR_INVALID_ARGUMENT_COUNT, -1, "%s expected %i to %i arguments, got %i", name, minimum, maximum, count);
		}
	}
}

static elf_String *lib_load_string(elf_State *state, u32 index)
{
	elf_Value value = load_value(state, index);
	check_value_type(state, value, ELF_VALUE_TYPE_STRING);
	return value_as_string(value);
}

static elf_StrSlice lib_load_string_slice(elf_State *state, u32 index)
{
	elf_String *value = lib_load_string(state, index);
	return (elf_StrSlice){ .data = (char *)string_data(value), .size = string_size(value) };
}

static const char *lib_load_cstr(elf_State *state, u32 index)
{
	return string_data(lib_load_string(state, index));
}

static i64 lib_load_integer(elf_State *state, u32 index)
{
	elf_Value value = load_value(state, index);
	check_numeric(state, value);
	return value_to_integer(value);
}

static i64 lib_load_stack_integer(elf_State *state, elf_i32 index)
{
	elf_Int value = 0;
	if (!elf_to_int(state, index, &value))
	{
		elf_error(state, "expected integer value");
	}
	return value;
}

static void lib_push_string(elf_State *state, const char *data, u32 size)
{
	elf_push_str(state, data, (int)size);
}

static void lib_set_integer_field(elf_State *state, elf_i32 table, const char *name, i64 value)
{
	elf_push_int(state, value);
	elf_set_field(state, table, name);
}

static void table_set_string_table(elf_State *state, elf_Table *parent, const char *name, elf_Table *table)
{
	elf_Value key = value_from_string(elf_string_from_data(state, name));
	elf_Value value = value_from_table(table);
	elf_table_set(state, parent, key, value);
}

static elf_Table *new_binding_table(elf_State *state, const elf_Binding *bindings, u32 count)
{
	elf_Table *table = push_new_table(state);
	for (u32 i = 0; i < count; ++i)
	{
		elf_Value key = value_from_string(elf_string_from_data(state, bindings[i].name));
		elf_Value value = value_from_function(bindings[i].function);
		elf_table_set(state, table, key, value);
	}
	return table;
}

#endif
