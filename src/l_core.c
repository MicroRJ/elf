//
// See Copyright Notice In elf.h
//

//
// Root standard-library bindings.
//

//
// Core assertions, reflection, and value conversions.
//

static u32 core_mark_readonly(elf_State *state, elf_Object *object);

static u32 core_mark_array_readonly(elf_State *state, elf_Array *array)
{
	u32 count = 0;
	for (u32 index = 0; index < array->count; ++index)
	{
		elf_Value value = array->items[index];
		if (value_is_object(value))
		{
			count += core_mark_readonly(state, value_as_object(value));
		}
	}
	return count;
}

static u32 core_mark_table_readonly(elf_State *state, elf_Table *table)
{
	u32 count = 0;

	for (u32 index = 0; index < table->capacity; ++index)
	{
		Entry entry = table->entries[index];
		if (!elf_table_entry_is_key(entry))
		{
			continue;
		}
		elf_Value key = entry.key;
		if (value_is_object(key))
		{
			count += core_mark_readonly(state, value_as_object(key));
		}
		if (value_is_object(entry.value))
		{
			count += core_mark_readonly(state, value_as_object(entry.value));
		}
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
	else if (object->type == ELF_OBJECT_ARRAY)
	{
		count += core_mark_array_readonly(state, (elf_Array *)object);
	}
	return count;
}

ELF_FUNCTION(l_core_assert)
{
	lib_check_arg_count(S, "assert", nargs, 1, 2);
	elf_Value condition = load_value(S, 1);
	check_numeric(S, condition);

	const char *message = "assertion failed";
	if (nargs > 2) message = lib_load_cstr(S, 2);
	if (!value_to_integer(condition))
	{
		report_runtime_error(S, ERROR_RUNTIME, "%s", message);
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
	const char *name = value_name(load_value(S, 1));
	push_value(S, value_from_string(elf_string_from_data(S, name)));
	return 1;
}

ELF_FUNCTION(l_core_to_string)
{
	lib_check_arg_count(S, "to_string", nargs, 1, 1);
	elf_push_value_text(S, 1);
	return 1;
}

ELF_FUNCTION(l_core_error)
{
	lib_check_arg_count(S, "error", nargs, 1, 2);
	elf_String *message = lib_load_string(S, 1);
	elf_Table *data = 0;
	if (nargs > 2)
	{
		elf_Value data_value = load_value(S, 2);
		if (!value_is_nil(data_value))
		{
			check_value_type(S, data_value, ELF_VALUE_TYPE_TABLE);
			data = value_as_table(data_value);
		}
	}

	push_value(S, value_from_error(elf_error_new(S, message, data)));
	return 1;
}

ELF_FUNCTION(l_core_is_error)
{
	lib_check_arg_count(S, "is_error", nargs, 1, 1);
	push_value(S, value_from_integer(value_is_error(load_value(S, 1))));
	return 1;
}

ELF_FUNCTION(l_core_is_string)
{
	lib_check_arg_count(S, "is_string", nargs, 1, 1);
	push_value(S, value_from_integer(value_is_string(load_value(S, 1))));
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
	check_numeric(S, value);
	push_value(S, value_from_number(value_to_number(value)));
	return 1;
}

ELF_FUNCTION(l_core_to_integer)
{
	lib_check_arg_count(S, "to_integer", nargs, 1, 1);
	elf_Value value = load_value(S, 1);
	check_numeric(S, value);
	push_value(S, value_from_integer(value_to_integer(value)));
	return 1;
}

//
// Introspection for the bytecode frame that called a core function.
//

static StackFrame *core_caller_frame(elf_State *state)
{
	if (state->frame == state->frame_stack)
	{
		report_runtime_error(state, ERROR_RUNTIME,
			"caller introspection requires a bytecode caller");
	}
	StackFrame *caller = state->frame - 1;
	if (!caller->prototype)
	{
		report_runtime_error(state, ERROR_RUNTIME,
			"caller introspection requires a bytecode caller");
	}
	return caller;
}

ELF_FUNCTION(l_core_nvargs)
{
	lib_check_arg_count(S, "nvargs", nargs, 0, 0);
	StackFrame *caller = core_caller_frame(S);
	i64 count = caller->variadic ? caller->nargs - caller->arity : 0;
	push_value(S, value_from_integer(MAX(count, 0)));
	return 1;
}

ELF_FUNCTION(l_core_varg)
{
	lib_check_arg_count(S, "varg", nargs, 1, 1);
	StackFrame *caller = core_caller_frame(S);
	i64 index = lib_load_integer(S, 1);
	i64 count = caller->variadic ? caller->nargs - caller->arity : 0;

	if (index < 0 || index >= count) push_value(S, value_nil());
	else push_value(S, caller->arguments[caller->arity + index]);
	return 1;
}

ELF_FUNCTION(l_core_nrets)
{
	lib_check_arg_count(S, "nrets", nargs, 0, 0);
	push_value(S, value_from_integer(core_caller_frame(S)->nrets));
	return 1;
}

ELF_FUNCTION(l_core_nargs)
{
	lib_check_arg_count(S, "nargs", nargs, 0, 0);
	push_value(S, value_from_integer(core_caller_frame(S)->nargs));
	return 1;
}

ELF_FUNCTION(l_core_arg)
{
	lib_check_arg_count(S, "arg", nargs, 1, 1);
	StackFrame *caller = core_caller_frame(S);
	i64 index = lib_load_integer(S, 1);
	if (index < 0 || index >= caller->nargs) push_value(S, value_nil());
	else push_value(S, caller->arguments[index]);
	return 1;
}

//
// Compiler-backed core operations.
//

static b32 lib_push_constant_expr(elf_State *state, const char *name, elf_StrSlice source)
{
	(void)state;
	(void)name;
	(void)source;
	return false;
}

ELF_FUNCTION(l_core_const_expr)
{
	lib_check_arg_count(S, "const_expr", nargs, 1, 1);
	elf_String *contents = lib_load_string(S, 1);
	elf_StrSlice source = {(char *)elf_string_data(contents), elf_string_size(contents)};
	if (!lib_push_constant_expr(S, "<const_expr>", source)) {
		elf_push_nil(S);
	}
	return 1;
}

static const elf_Binding l_core[] = {
	{"assert",       l_core_assert},
	{"metatable",    l_core_metatable},
	{"freeze",       l_core_freeze},
	{"is_readonly",  l_core_is_readonly},

	{"type_of",      l_core_type_of},
	{"to_string",    l_core_to_string},
	{"error",        l_core_error},
	{"is_error",     l_core_is_error},
	{"is_string",    l_core_is_string},
	{"is_numeric",   l_core_is_numeric},
	{"to_number",    l_core_to_number},
	{"to_integer",   l_core_to_integer},

	{"nvargs",       l_core_nvargs},
	{"varg",         l_core_varg},
	{"nrets",        l_core_nrets},
	{"nargs",        l_core_nargs},
	{"arg",          l_core_arg},

	{"const_expr",   l_core_const_expr},
};

static elf_SymTable *elf_lib_core(void)
{
	return new_symbol_table(l_core, ARRAY_COUNT(l_core));
}
