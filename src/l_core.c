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
	if (!lib_check_arg_count(S, "assert", nargs, 1, 2)) {
		return 1;
	}
	elf_Value condition = load_value(S, 0);
	check_numeric(S, condition);

	const char *message = "assertion failed";
	if (nargs > 1) message = lib_load_cstr(S, 1);
	if (!value_to_integer(condition))
	{
		report_runtime_error(S, ELF_ERROR_RUNTIME, "%s", message);
	}
	return 0;
}

ELF_FUNCTION(l_core_echo)
{
	for (u32 index = 0; index < nargs; ++ index)
	{
		push_value(S, load_value(S, index));
	}
	return nargs;
}

ELF_FUNCTION(l_core_call)
{
	if (!lib_check_arg_count(S, "call", nargs, 1, INT_MAX))
	{
		return 1;
	}

	for (u32 index = 0; index < nargs; ++ index)
	{
		push_value(S, load_value(S, index));
	}
	return elf_call(S, nargs - 1);
}

ELF_FUNCTION(l_core_metatable)
{
	if (!lib_check_arg_count(S, "metatable", nargs, 1, 1)) {
		return 1;
	}
	elf_Table *metatable = elf_get_type_metatable(S, load_value(S, 0));
	if (metatable) push_table(S, metatable);
	else elf_push_nil(S);
	return 1;
}

ELF_FUNCTION(l_core_freeze)
{
	if (!lib_check_arg_count(S, "freeze", nargs, 1, 1)) {
		return 1;
	}
	elf_Value value = load_value(S, 0);
	check_value_type_rule(S, value, TRULE_OBJECT);
	core_mark_readonly(S, value_as_object(value));
	push_value(S, value);
	return 1;
}

ELF_FUNCTION(l_core_is_readonly)
{
	if (!lib_check_arg_count(S, "is_readonly", nargs, 1, 1)) {
		return 1;
	}
	elf_Value value = load_value(S, 0);
	check_value_type_rule(S, value, TRULE_OBJECT);
	push_value(S, int_value((value_as_object(value)->status & ELF_OBJECT_READONLY) != 0));
	return 1;
}

ELF_FUNCTION(l_core_type_of)
{
	if (!lib_check_arg_count(S, "type_of", nargs, 1, 1)) {
		return 1;
	}
	const char *name = value_name(load_value(S, 0));
	push_value(S, str_value(elf_string_from_data(S, name)));
	return 1;
}

ELF_FUNCTION(l_core_to_string)
{
	if (!lib_check_arg_count(S, "to_string", nargs, 1, 1)) {
		return 1;
	}
	elf_push_value_text(S, 0);
	return 1;
}

ELF_FUNCTION(l_core_error)
{
	if (!lib_check_arg_count(S, "error", nargs, 1, 2)) {
		return 1;
	}
	elf_String *message = lib_load_string(S, 0);
	elf_Table *data = 0;
	if (nargs > 1)
	{
		elf_Value data_value = load_value(S, 1);
		if (!value_is_nil(data_value))
		{
			check_value_type(S, data_value, ELF_VALUE_TYPE_TABLE);
			data = value_as_table(data_value);
		}
	}

	push_value(S, value_from_error(elf_error_new(S, ELF_ERROR_RUNTIME, message, data)));
	return 1;
}

ELF_FUNCTION(l_core_is_error)
{
	if (!lib_check_arg_count(S, "is_error", nargs, 1, 1)) {
		return 1;
	}
	push_value(S, int_value(value_is_error(load_value(S, 0))));
	return 1;
}

ELF_FUNCTION(l_core_is_string)
{
	if (!lib_check_arg_count(S, "is_string", nargs, 1, 1)) {
		return 1;
	}
	push_value(S, int_value(value_is_string(load_value(S, 0))));
	return 1;
}

ELF_FUNCTION(l_core_is_numeric)
{
	if (!lib_check_arg_count(S, "is_numeric", nargs, 1, 1)) {
		return 1;
	}
	push_value(S, int_value(value_is_numeric(load_value(S, 0))));
	return 1;
}

ELF_FUNCTION(l_core_to_number)
{
	if (!lib_check_arg_count(S, "to_number", nargs, 1, 1)) {
		return 1;
	}
	elf_Value value = load_value(S, 0);
	check_numeric(S, value);
	push_value(S, num_value(value_to_number(value)));
	return 1;
}

ELF_FUNCTION(l_core_to_integer)
{
	if (!lib_check_arg_count(S, "to_integer", nargs, 1, 1)) {
		return 1;
	}
	elf_Value value = load_value(S, 0);
	check_numeric(S, value);
	push_value(S, int_value(value_to_integer(value)));
	return 1;
}

//
// Introspection for the bytecode frame that called a core function.
//

static StackFrame *core_caller_frame(elf_State *state)
{
	StackFrame *caller = frame_previous(state, frame_current(state));
	if (!caller || !frame_proto(caller))
	{
		report_runtime_error(state, ELF_ERROR_RUNTIME,
			"caller introspection requires a bytecode caller");
	}
	return caller;
}

ELF_FUNCTION(l_core_nvargs)
{
	if (!lib_check_arg_count(S, "nvargs", nargs, 0, 0)) {
		return 1;
	}
	StackFrame *caller = core_caller_frame(S);
	elf_Prototype *proto = frame_proto(caller);
	i64 count = proto->variadic ? (i64)frame_nargs(caller) - proto->arity : 0;
	push_value(S, int_value(MAX(count, 0)));
	return 1;
}

ELF_FUNCTION(l_core_varg)
{
	if (!lib_check_arg_count(S, "varg", nargs, 1, 1)) {
		return 1;
	}
	StackFrame *caller = core_caller_frame(S);
	elf_Prototype *proto = frame_proto(caller);
	i64 index = lib_load_integer(S, 0);
	i64 count = proto->variadic ? (i64)frame_nargs(caller) - proto->arity : 0;

	if (index < 0 || index >= count) push_value(S, value_nil());
	else push_value(S, frame_arg(caller, proto->arity + (u32)index));
	return 1;
}

ELF_FUNCTION(l_core_nargs)
{
	if (!lib_check_arg_count(S, "nargs", nargs, 0, 0)) {
		return 1;
	}
	push_value(S, int_value(frame_nargs(core_caller_frame(S))));
	return 1;
}

ELF_FUNCTION(l_core_arg)
{
	if (!lib_check_arg_count(S, "arg", nargs, 1, 1)) {
		return 1;
	}
	StackFrame *caller = core_caller_frame(S);
	i64 index = lib_load_integer(S, 0);
	if (index < 0 || index >= frame_nargs(caller)) push_value(S, value_nil());
	else push_value(S, frame_arg(caller, (u32)index));
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
	if (!lib_check_arg_count(S, "const_expr", nargs, 1, 1)) {
		return 1;
	}
	elf_String *contents = lib_load_string(S, 0);
	elf_StrSlice source = {(char *)elf_string_data(contents), elf_string_size(contents)};
	if (!lib_push_constant_expr(S, "<const_expr>", source)) {
		elf_push_nil(S);
	}
	return 1;
}

static const elf_Binding l_core[] = {
	{"assert",       l_core_assert},
	{"echo",         l_core_echo},
	{"call",         l_core_call},
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
	{"nargs",        l_core_nargs},
	{"arg",          l_core_arg},

	{"const_expr",   l_core_const_expr},
};

static elf_SymTable *elf_lib_core(void)
{
	return new_symbol_table(l_core, ARRAY_COUNT(l_core));
}
