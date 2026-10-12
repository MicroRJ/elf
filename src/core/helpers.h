//
// See Copyright Notice In elf.h
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

static inline b32 value_length(elf_Value value, u32 *length)
{
	b32 result = true;
	if (!length)
	{
		result = false;
	}
	else if (value_is_table(value))
	{
		*length = value_as_table(value)->count;
	}
	else if (value_is_string(value))
	{
		*length = string_size(value_as_string(value));
	}
	else if (value_is_array(value))
	{
		*length = elf_array_length(value_as_array(value));
	}
	else if (value_is_buffer(value))
	{
		*length = value_as_buffer(value)->size;
	}
	else
	{
		result = false;
	}
	return result;
}

#define get_num_args(state) ((state)->frame->nargs)

typedef enum
{
	TBIT_NIL      = 1 << ELF_VALUE_TYPE_NIL,
	TBIT_NUMBER   = 1 << ELF_VALUE_TYPE_NUMBER,
	TBIT_INTEGER  = 1 << ELF_VALUE_TYPE_INTEGER,
	TBIT_FUNCTION = 1 << ELF_VALUE_TYPE_CFUNCTION,
	TBIT_USER     = 1 << ELF_VALUE_TYPE_USER_OBJECT,
	TBIT_CLOSURE  = 1 << ELF_VALUE_TYPE_CLOSURE,
	TBIT_STRING   = 1 << ELF_VALUE_TYPE_STRING,
	TBIT_TABLE    = 1 << ELF_VALUE_TYPE_TABLE,
	TBIT_ERROR    = 1 << ELF_VALUE_TYPE_ERROR,
	TBIT_BUFFER   = 1 << ELF_VALUE_TYPE_BUFFER,
	TBIT_ARRAY    = 1 << ELF_VALUE_TYPE_ARRAY,

	TBIT_ALLMASK  = (1 << ELF_VALUE_TYPE_COUNT_) - 1,
}
TypeBit;

typedef enum
{
	TRULE_NONE     = 0,
	TRULE_NIL      = TBIT_NIL,
	TRULE_NUMBER   = TBIT_NUMBER,
	TRULE_INTEGER  = TBIT_INTEGER,
	TRULE_FUNCTION = TBIT_FUNCTION,
	TRULE_USER     = TBIT_USER,
	TRULE_CLOSURE  = TBIT_CLOSURE,
	TRULE_STRING   = TBIT_STRING,
	TRULE_TABLE    = TBIT_TABLE,
	TRULE_ERROR    = TBIT_ERROR,
	TRULE_BUFFER   = TBIT_BUFFER,
	TRULE_ARRAY    = TBIT_ARRAY,

	TYPE_RULE_ANYTHING = TBIT_ALLMASK,
	TRULE_NONNIL       = TYPE_RULE_ANYTHING & ~TBIT_NIL,
	TRULE_OBJECT       = TBIT_USER | TBIT_CLOSURE | TBIT_TABLE | TBIT_ERROR | TBIT_BUFFER | TBIT_ARRAY,
	TRULE_CALLABLE     = TBIT_FUNCTION | TBIT_CLOSURE,

	TRULE_COUNT,
}
TypeRule;

static inline void check_value_type(elf_State *S, elf_Value value, int type)
{
	if (value.type != type) {
		elf_report_runtime_error(S, RUNTIME_ERROR_GENERIC, -1, "type error, expected '%s', got '%s'", value_type_name(type), value_type_name(value.type));
	}
}

static inline void check_value_type_rule(elf_State *S, elf_Value value, TypeRule rule)
{
	if (~rule & 1 << value.type) {
		elf_report_runtime_error(S, RUNTIME_ERROR_GENERIC, -1, "type rule violation, got '%s'", value_type_name(value.type));
	}
}

static inline void check_numeric(elf_State *state, elf_Value value)
{
	if (!value_is_numeric(value)) {
		elf_report_runtime_error(state, RUNTIME_ERROR_GENERIC, -1,
			"type error, expected numeric, got '%s'", value_type_name(value.type));
	}
}

static inline u32 check_array_index(elf_State *state, int instr, i64 index, u32 count)
{
	i64 resolved = index;
	if (resolved < 0) {
		resolved += count;
	}

	if (resolved < 0 || resolved >= count)
	{
		if (resolved != index)
		{
			elf_report_runtime_error(state, RUNTIME_ERROR_GENERIC, instr
			,	"array index out of bounds: index %lli resolved to %lli, length %u"
			,	index, resolved, count);
		}
		else
		{
			elf_report_runtime_error(state, RUNTIME_ERROR_GENERIC, instr
			,	"array index out of bounds: index %lli, length %u"
			,	index, count);
		}
	}

	return (u32)resolved;
}

static elf_i32 frame_size(elf_State *state)
{
	return state->frame->framesize;
}

static inline void vm_record_stack_usage(elf_State *state)
{
	u64 slots = (u64)(state->stack_ptr - state->stack);
	state->vm_max_stack_slots = MAX(state->vm_max_stack_slots, slots);
}

static inline void push_stack(elf_State *S, elf_Value value)
{
	ASSERT(S->stack_ptr < S->stack + S->stack_size);
	*S->stack_ptr++ = value;
	vm_record_stack_usage(S);
}

static inline void push_value(elf_State *S, elf_Value value)
{
	push_stack(S, value);
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
	if (x < 0 || x >= get_num_args(S)) {
		elf_report_runtime_error(S, RUNTIME_ERROR_GENERIC, -1, "invalid argument index: %i, got: %i", x, get_num_args(S));
	}
	return S->frame->framebase[x];
}
