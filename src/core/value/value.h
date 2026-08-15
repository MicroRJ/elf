//
// See Copyright Notice In elf.h
//

#ifndef ELF_CORE_VALUE_H
#define ELF_CORE_VALUE_H

#define NIL_VALUE ((elf_Value) { ELF_VALUE_TYPE_NIL })
#define VALUE_READONLY 2

typedef struct elf_Value elf_Value;
struct elf_Value
{
	u8  type;
	u8  status;
	u16 unused;
	union {
		i64           x_i64;
		i64           x_int;
		f64           x_num;
		elf_Object   *x_obj;
		elf_Table    *x_tab;
		elf_String   *x_string;
		elf_Function  x_proc;
		elf_Closure  *x_closure;
		void         *x_ptr;
	};
};

STATIC_ASSERT(sizeof(elf_Value) == 16);

static inline b32 type_is_object(elf_ValueType type)
{
	return type == ELF_VALUE_TYPE_USER_OBJECT
	||     type == ELF_VALUE_TYPE_CLOSURE
	||     type == ELF_VALUE_TYPE_TABLE
	||     type == ELF_VALUE_TYPE_STRING;
}

static inline b32 type_is_nil(elf_ValueType type)      { return type == ELF_VALUE_TYPE_NIL; }
static inline b32 type_is_number(elf_ValueType type)   { return type == ELF_VALUE_TYPE_NUMBER; }
static inline b32 type_is_integer(elf_ValueType type)  { return type == ELF_VALUE_TYPE_INTEGER; }
static inline b32 type_is_string(elf_ValueType type)   { return type == ELF_VALUE_TYPE_STRING; }
static inline b32 type_is_table(elf_ValueType type)    { return type == ELF_VALUE_TYPE_TABLE; }
static inline b32 type_is_user(elf_ValueType type)     { return type == ELF_VALUE_TYPE_USER_OBJECT; }
static inline b32 type_is_function(elf_ValueType type) { return type == ELF_VALUE_TYPE_CFUNCTION; }
static inline b32 type_is_closure(elf_ValueType type)  { return type == ELF_VALUE_TYPE_CLOSURE; }

static inline b32 type_is_numeric(elf_ValueType type)
{
	return type_is_number(type) || type_is_integer(type);
}

static inline b32 type_is_callable(elf_ValueType type)
{
	return type_is_closure(type) || type_is_function(type);
}

static inline elf_ValueType value_type(elf_Value value) { return value.type; }

static inline b32 value_is_nil(elf_Value value)      { return value.type == ELF_VALUE_TYPE_NIL; }
static inline b32 value_is_number(elf_Value value)   { return value.type == ELF_VALUE_TYPE_NUMBER; }
static inline b32 value_is_integer(elf_Value value)  { return value.type == ELF_VALUE_TYPE_INTEGER; }
static inline b32 value_is_string(elf_Value value)   { return value.type == ELF_VALUE_TYPE_STRING; }
static inline b32 value_is_table(elf_Value value)    { return value.type == ELF_VALUE_TYPE_TABLE; }
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

static inline i64 value_as_integer(elf_Value value)          { return value.x_int; }
static inline f64 value_as_number(elf_Value value)           { return value.x_num; }
static inline elf_Object *value_as_object(elf_Value value)   { return value.x_obj; }
static inline elf_String *value_as_string(elf_Value value)   { return value.x_string; }
static inline elf_Table *value_as_table(elf_Value value)     { return value.x_tab; }
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

static inline void value_swap(elf_Value *left, elf_Value *right)
{
	elf_Value temp = *left;
	*left = *right;
	*right = temp;
}

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

static const char *value_type_name(elf_ValueType type)
{
	static const char *names[] =
	{
		[ELF_VALUE_TYPE_NIL]         = "nil",
		[ELF_VALUE_TYPE_NUMBER]      = "number",
		[ELF_VALUE_TYPE_INTEGER]     = "integer",
		[ELF_VALUE_TYPE_USER_OBJECT] = "resource",
		[ELF_VALUE_TYPE_CFUNCTION]   = "function",
		[ELF_VALUE_TYPE_CLOSURE]     = "closure",
		[ELF_VALUE_TYPE_STRING]      = "string",
		[ELF_VALUE_TYPE_TABLE]       = "table",
	};

	if ((u32)type >= ELF_VALUE_TYPE_COUNT_ || !names[type]) {
		return "unknown";
	}
	return names[type];
}


#endif
