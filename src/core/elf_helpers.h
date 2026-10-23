//
// See Copyright Notice In elf.h
//

#ifndef ELF_CORE_HELPERS_H
#define ELF_CORE_HELPERS_H

//
// Value type predicates.
//

static inline b32 type_is_object(elf_ValueType type)
{
	return type == ELF_VALUE_TYPE_USER_OBJECT
	||     type == ELF_VALUE_TYPE_CLOSURE
	||     type == ELF_VALUE_TYPE_TABLE
	||     type == ELF_VALUE_TYPE_STRING
	||     type == ELF_VALUE_TYPE_ERROR
	||     type == ELF_VALUE_TYPE_BUFFER
	||     type == ELF_VALUE_TYPE_ARRAY;
}

//
// Value predicates.
//

static inline elf_ValueType value_type(elf_Value value) { return value.type; }

static inline b32 value_is_nil(elf_Value value)      { return value.type == ELF_VALUE_TYPE_NIL; }
static inline b32 value_is_number(elf_Value value)   { return value.type == ELF_VALUE_TYPE_NUMBER; }
static inline b32 value_is_integer(elf_Value value)  { return value.type == ELF_VALUE_TYPE_INTEGER; }
static inline b32 value_is_string(elf_Value value)   { return value.type == ELF_VALUE_TYPE_STRING; }
static inline b32 value_is_table(elf_Value value)    { return value.type == ELF_VALUE_TYPE_TABLE; }
static inline b32 value_is_error(elf_Value value)    { return value.type == ELF_VALUE_TYPE_ERROR; }
static inline b32 value_is_buffer(elf_Value value)   { return value.type == ELF_VALUE_TYPE_BUFFER; }
static inline b32 value_is_array(elf_Value value)    { return value.type == ELF_VALUE_TYPE_ARRAY; }
static inline b32 value_is_user(elf_Value value)     { return value.type == ELF_VALUE_TYPE_USER_OBJECT; }
static inline b32 value_is_function(elf_Value value) { return value.type == ELF_VALUE_TYPE_CFUNCTION; }
static inline b32 value_is_closure(elf_Value value)  { return value.type == ELF_VALUE_TYPE_CLOSURE; }

static inline b32 value_is_numeric(elf_Value value)
{
	return value_is_number(value) || value_is_integer(value);
}

static inline b32 value_is_callable(elf_Value value)
{
	return value_is_closure(value) || value_is_function(value);
}

static inline b32 value_is_object(elf_Value value)
{
	return type_is_object(value.type);
}

//
// Value accessors and conversions.
//

static inline i64 value_as_integer(elf_Value value)          { return value.x_int; }
static inline f64 value_as_number(elf_Value value)           { return value.x_num; }
static inline elf_Object *value_as_object(elf_Value value)   { return value.x_obj; }
static inline elf_String *value_as_string(elf_Value value)   { return value.x_string; }
static inline elf_Table *value_as_table(elf_Value value)     { return value.x_tab; }
static inline elf_Array *value_as_array(elf_Value value)     { return value.x_array; }
static inline elf_Table *value_as_error(elf_Value value)     { return value.x_tab; }
static inline elf_ByteBuffer *value_as_buffer(elf_Value value){ return value.x_buffer; }
static inline elf_UserObject *value_as_user(elf_Value value)  { return value.x_user; }
static inline elf_Closure *value_as_closure(elf_Value value) { return value.x_closure; }
static inline elf_Function value_as_function(elf_Value value){ return value.x_proc; }

static inline f64 value_to_number(elf_Value value)
{
	return value_is_integer(value) ? (f64)value_as_integer(value) : value_as_number(value);
}

static inline i64 value_to_integer(elf_Value value)
{
	return value_is_number(value) ? (i64)value_as_number(value) : value_as_integer(value);
}

//
// Value storage operations.
//

static inline void value_copy(elf_Value *dst, elf_Value src)
{
	copy_memory(dst, &src, sizeof(src));
}

static inline void value_copy_many(elf_Value *dst, elf_Value *src, int count)
{
	copy_memory(dst, src, count * sizeof(*src));
}

static inline void value_zero_many(elf_Value *dst, int count)
{
	zero_memory(dst, count * sizeof(*dst));
}

//
// Value constructors.
//

static inline elf_Value value_nil(void)
{
	return (elf_Value){ELF_VALUE_TYPE_NIL};
}

static inline elf_Value value_from_integer(i64 integer)
{
	elf_Value value = {};
	value.type = ELF_VALUE_TYPE_INTEGER;
	value.x_int = integer;
	return value;
}

static inline elf_Value value_from_number(f64 number)
{
	elf_Value value = {};
	value.type = ELF_VALUE_TYPE_NUMBER;
	value.x_num = number;
	return value;
}

static inline elf_Value value_from_string(elf_String *string)
{
	ASSERT(string != 0);
	elf_Value value = {};
	value.type = ELF_VALUE_TYPE_STRING;
	value.x_string = string;
	return value;
}

static inline elf_Value value_from_table(elf_Table *table)
{
	ASSERT(table != 0);
	elf_Value value = {};
	value.type = ELF_VALUE_TYPE_TABLE;
	value.x_tab = table;
	return value;
}

static inline elf_Value value_from_array(elf_Array *array)
{
	ASSERT(array != 0);
	elf_Value value = {};
	value.type = ELF_VALUE_TYPE_ARRAY;
	value.x_array = array;
	return value;
}

static inline elf_Value value_from_error(elf_Table *info)
{
	ASSERT(info != 0);
	elf_Value value = {};
	value.type = ELF_VALUE_TYPE_ERROR;
	value.x_tab = info;
	return value;
}

static inline elf_Value value_from_buffer(elf_ByteBuffer *buffer)
{
	ASSERT(buffer != 0);
	elf_Value value = {};
	value.type = ELF_VALUE_TYPE_BUFFER;
	value.x_buffer = buffer;
	return value;
}

static inline elf_Value value_from_user(elf_UserObject *user)
{
	ASSERT(user != 0);
	elf_Value value = {};
	value.type = ELF_VALUE_TYPE_USER_OBJECT;
	value.x_user = user;
	return value;
}

static inline elf_Value value_from_function(elf_Function function)
{
	ASSERT(function != 0);
	elf_Value value = {};
	value.type = ELF_VALUE_TYPE_CFUNCTION;
	value.x_proc = function;
	return value;
}

static inline elf_Value value_from_closure(elf_Closure *closure)
{
	ASSERT(closure != 0);
	elf_Value value = {};
	value.type = ELF_VALUE_TYPE_CLOSURE;
	value.x_closure = closure;
	return value;
}

//
// Value names.
//

static const char *value_type_name(elf_ValueType type)
{
	static const char *names[] =
	{
		[ELF_VALUE_TYPE_NIL]         = "nil",
		[ELF_VALUE_TYPE_NUMBER]      = "number",
		[ELF_VALUE_TYPE_INTEGER]     = "integer",
		[ELF_VALUE_TYPE_USER_OBJECT] = "user object",
		[ELF_VALUE_TYPE_CFUNCTION]   = "function",
		[ELF_VALUE_TYPE_CLOSURE]     = "closure",
		[ELF_VALUE_TYPE_STRING]      = "string",
		[ELF_VALUE_TYPE_TABLE]       = "table",
		[ELF_VALUE_TYPE_ERROR]       = "error",
		[ELF_VALUE_TYPE_BUFFER]      = "buffer",
		[ELF_VALUE_TYPE_ARRAY]       = "array",
	};

	if ((u32)type >= ELF_VALUE_TYPE_COUNT_ || !names[type]) {
		return "unknown";
	}
	return names[type];
}

static const char *type_rule_name(TypeRule rule)
{
	switch (rule)
	{
		case TRULE_NONE:     return "no value";
		case TRULE_ALL:      return "any value";
		case TRULE_NUMERIC:  return "numeric";
		case TRULE_NONNIL:   return "non-nil";
		case TRULE_OBJECT:   return "object";
		case TRULE_CALLABLE: return "callable";
		default:
		{
			for (u32 type = 0; type < ELF_VALUE_TYPE_COUNT_; ++type)
			{
				if (rule == (TypeRule)(1 << type)) {
					return value_type_name(type);
				}
			}
		}
		break;
	}
	return "unknown";
}

//
// Core object helpers.
//

static inline u32 elf_string_size(elf_String *string)
{
	ASSERT(string);
	ASSERT(string->obj.size >= ELF_STRING_HEADER_SIZE + 1);
	return string->obj.size - ELF_STRING_HEADER_SIZE - 1;
}

static inline const char *elf_string_data(elf_String *string)
{
	return string->data;
}

static inline b32 elf_string_equal(elf_String *left, elf_String *right)
{
	return left == right;
}

static inline elf_String *elf_string_from_data(elf_State *state, const char *data)
{
	ASSERT(data);
	return elf_string_from_data_size(state, data, (u32)strlen(data));
}

static inline b32 elf_table_entry_is_key(Entry entry)
{
	return entry.key.type != ELF_VALUE_TYPE_NIL && entry.key.type != ENTRY_TOMB_TYPE;
}

static inline b32 prototype_index_is_valid(elf_State *state, u32 prototype_index)
{
	return prototype_index < state->prototype_count && state->prototypes[prototype_index];
}

static inline elf_Prototype *prototype_from_index(elf_State *state, u32 prototype_index)
{
	ASSERT(prototype_index_is_valid(state, prototype_index));
	return state->prototypes[prototype_index];
}

//
// Cross-type runtime helpers.
//

static inline elf_Table *elf_get_type_metatable(elf_State *state, elf_Value value)
{
	switch (value.type)
	{
		case ELF_VALUE_TYPE_STRING:  return state->metatables.string;
		case ELF_VALUE_TYPE_TABLE:   return state->metatables.table;
		case ELF_VALUE_TYPE_ARRAY:   return state->metatables.array;
		case ELF_VALUE_TYPE_NUMBER:  return state->metatables.number;
		case ELF_VALUE_TYPE_INTEGER: return state->metatables.integer;
		case ELF_VALUE_TYPE_BUFFER:  return state->metatables.buffer;
		case ELF_VALUE_TYPE_USER_OBJECT: return value_as_user(value)->metatable;
		default:                     return 0;
	}
}

static inline const char *value_name(elf_Value value)
{
	if (value_is_user(value))
	{
		elf_UserObject *user = value_as_user(value);
		if (user->type && user->type->name) return user->type->name;
	}
	return value_type_name(value.type);
}

static inline b32 value_measure(elf_Value value, u32 *measure)
{
	b32 result = true;
	if (!measure)
	{
		result = false;
	}
	else if (value_is_nil(value))
	{
		*measure = 0;
	}
	else if (value_is_table(value))
	{
		*measure = value_as_table(value)->count;
	}
	else if (value_is_string(value))
	{
		*measure = elf_string_size(value_as_string(value));
	}
	else if (value_is_array(value))
	{
		*measure = elf_array_length(value_as_array(value));
	}
	else if (value_is_buffer(value))
	{
		*measure = value_as_buffer(value)->size;
	}
	else
	{
		result = false;
	}
	return result;
}

//
// Runtime argument and type validation.
//

static inline void check_value_type(elf_State *S, elf_Value value, int type)
{
	if (value.type != type) {
		report_runtime_error(S, ERROR_TYPE_MISMATCH, "type error, expected '%s', got '%s'", value_type_name(type), value_type_name(value.type));
	}
}

static inline void check_value_type_rule(elf_State *S, elf_Value value, TypeRule rule)
{
	if (~rule & 1 << value.type) {
		report_runtime_error(S, ERROR_TYPE_MISMATCH, "type error, expected '%s', got '%s'", type_rule_name(rule), value_type_name(value.type));
	}
}

static inline void check_numeric(elf_State *state, elf_Value value)
{
	if (!value_is_numeric(value)) {
		report_runtime_error(state, ERROR_TYPE_MISMATCH, "type error, expected numeric, got '%s'", value_type_name(value.type));
	}
}

static inline u32 check_array_index(elf_State *state, i64 index, u32 count)
{
	i64 resolved = index;
	if (resolved < 0) {
		resolved += count;
	}

	if (resolved < 0 || resolved >= count)
	{
		if (resolved != index)
		{
			report_runtime_error(state, ERROR_OUT_OF_RANGE, "array index out of bounds: index %lli resolved to %lli, length %u", index, resolved, count);
		}
		else
		{
			report_runtime_error(state, ERROR_OUT_OF_RANGE, "array index out of bounds: index %lli, length %u", index, count);
		}
	}

	return (u32)resolved;
}

//
// Stack and frame helpers.
//

static inline void vm_record_stack_usage(elf_State *state)
{
	u64 slots = (u64)(state->stack_ptr - state->stack);
	state->vm_max_stack_slots = MAX(state->vm_max_stack_slots, slots);
}

static inline void push_value(elf_State *S, elf_Value value)
{
	ASSERT(S->stack_ptr < S->stack + S->stack_size);
	*S->stack_ptr++ = value;
	vm_record_stack_usage(S);
}

static inline void push_table(elf_State *S, elf_Table *table)
{
	push_value(S, value_from_table(table));
}

static inline void push_array(elf_State *state, elf_Array *array)
{
	push_value(state, value_from_array(array));
}

static inline elf_Table *push_new_table(elf_State *state)
{
	elf_Table *table = elf_table_new(state);
	push_table(state, table);
	return table;
}

static inline elf_Array *push_new_array(elf_State *state)
{
	elf_Array *array = elf_array_new(state);
	push_array(state, array);
	return array;
}

static inline elf_Value pop_value(elf_State *S)
{
	ASSERT(S->stack_ptr > S->stack);
	return *--S->stack_ptr;
}

static inline elf_Value load_value(elf_State *S, int x)
{
	if (x < 0 || x >= S->frame->nargs) {
		report_runtime_error(S, ERROR_INVALID_ARGUMENT, "invalid argument index: %i, got: %i", x, S->frame->nargs);
	}
	return S->frame->framebase[x];
}

//
// Standard library helpers.
//

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
			report_runtime_error(state, ERROR_INVALID_ARGUMENT_COUNT, "%s expected %i argument(s), got %i", name, minimum, count);
		}
		else
		{
			report_runtime_error(state, ERROR_INVALID_ARGUMENT_COUNT, "%s expected %i to %i arguments, got %i", name, minimum, maximum, count);
		}
	}
}

static elf_String *lib_load_string(elf_State *state, u32 index)
{
	elf_Value value = load_value(state, index);
	check_value_type(state, value, ELF_VALUE_TYPE_STRING);
	return value_as_string(value);
}

static dy_String lib_load_string_view(elf_State *state, u32 index)
{
	elf_String *value = lib_load_string(state, index);
	return dy_string_from_data(elf_string_data(value), elf_string_size(value));
}

static const char *lib_load_cstr(elf_State *state, u32 index)
{
	return elf_string_data(lib_load_string(state, index));
}

static i64 lib_load_integer(elf_State *state, u32 index)
{
	elf_Value value = load_value(state, index);
	check_numeric(state, value);
	return value_to_integer(value);
}

static void lib_set_integer_field(elf_State *state, elf_i32 table, const char *name, i64 value)
{
	elf_push_int(state, value);
	elf_set_field(state, table, name);
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

static elf_SymTable *new_symbol_table(const elf_Binding *bindings, u32 count)
{
	elf_SymTable *table = 0;
	elf_sym_create(&table);
	for (u32 i = 0; i < count; ++i)
	{
		elf_StrSlice name = {bindings[i].name, strlen(bindings[i].name)};
		elf_sym_fun(table, name, bindings[i].function);
	}
	return table;
}

#endif
