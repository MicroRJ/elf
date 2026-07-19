//
// See Copyright Notice In elf.h
//

static inline elf_Table *elf_get_type_metatable(elf_State *state, elf_Value value)
{
	switch (value.type)
	{
		case ELF_VALUE_TYPE_ATOM:    return state->metatables.atom;
		case ELF_VALUE_TYPE_TABLE:   return state->metatables.table;
		case ELF_VALUE_TYPE_NUMBER:  return state->metatables.number;
		case ELF_VALUE_TYPE_INTEGER: return state->metatables.integer;
		default:                     return 0;
	}
}

#define get_num_args(state) ((state)->frame.nargs)

typedef enum
{
	TBIT_NIL      = 1 << ELF_VALUE_TYPE_NIL,
	TBIT_NUMBER   = 1 << ELF_VALUE_TYPE_NUMBER,
	TBIT_INTEGER  = 1 << ELF_VALUE_TYPE_INTEGER,
	TBIT_FUNCTION = 1 << ELF_VALUE_TYPE_CFUNCTION,
	TBIT_USER     = 1 << ELF_VALUE_TYPE_USER_OBJECT,
	TBIT_CLOSURE  = 1 << ELF_VALUE_TYPE_CLOSURE,
	TBIT_ATOM     = 1 << ELF_VALUE_TYPE_ATOM,
	TBIT_TABLE    = 1 << ELF_VALUE_TYPE_TABLE,

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
	TRULE_ATOM     = TBIT_ATOM,
	TRULE_TABLE    = TBIT_TABLE,

	TYPE_RULE_ANYTHING = TBIT_ALLMASK,
	TRULE_NONNIL       = TYPE_RULE_ANYTHING & ~TBIT_NIL,
	TRULE_OBJECT       = TBIT_USER | TBIT_CLOSURE | TBIT_TABLE,
	TRULE_NUMERIC      = TBIT_INTEGER | TBIT_NUMBER,
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
			,	"array index out of bounds: index %lli resolved to %lli, length %lli"
			,	index, resolved, count);
		}
		else
		{
			elf_report_runtime_error(state, RUNTIME_ERROR_GENERIC, instr
			,	"array index out of bounds: index %lli, length %lli"
			,	index, count);
		}
	}

	return (u32)resolved;
}

#define stack2index(S) ((S)->stack_ptr - (S)->stack)

static inline void push_stack(elf_State *S, elf_Value value)
{
	ASSERT(S->stack_ptr < S->stack + S->stack_size);
	*S->stack_ptr++ = value;
}

static inline void push_value(elf_State *S, elf_Value value)
{
	push_stack(S, value);
}

static inline void push_table(elf_State *S, elf_Table *table)
{
	push_value(S, value_from_table(table));
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
	return S->frame.framebase[x];
}

static inline elf_Value make_atom_value_from_data(elf_State *state, char *data)
{
	return value_from_atom(elf_atom_from_data(state, data));
}
