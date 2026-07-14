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
		i64 x_i64;
		struct { i32 x_i32, y_i32; };
		i64 x_int;
		f64 x_num;
		elf_Handle x_sys;
		elf_Object *x_obj;
		elf_Table *x_tab;
		elf_Atom *x_atom;
		elf_Function x_proc;
		elf_Closure *x_closure;
		void *x_ptr;
	};
};

STATIC_ASSERT(sizeof(elf_Value) == 16);

static inline b32 elf_value_type_is_object(ELF_ValueType type)
{
	return type == ELF_VALUE_TYPE_USER_OBJECT ||
		type == ELF_VALUE_TYPE_CLOSURE ||
		type == ELF_VALUE_TYPE_TABLE ||
		type == ELF_VALUE_TYPE_ATOM;
}

static inline b32 elf_value_type_is_dead(ELF_ValueType type)
{
	return type == ELF_VALUE_TYPE_NIL;
}

static inline b32 elf_value_type_is_nil(ELF_ValueType type)      { return type == ELF_VALUE_TYPE_NIL; }
static inline b32 elf_value_type_is_number(ELF_ValueType type)   { return type == ELF_VALUE_TYPE_NUMBER; }
static inline b32 elf_value_type_is_integer(ELF_ValueType type)  { return type == ELF_VALUE_TYPE_INTEGER; }
static inline b32 elf_value_type_is_atom(ELF_ValueType type)     { return type == ELF_VALUE_TYPE_ATOM; }
static inline b32 elf_value_type_is_table(ELF_ValueType type)    { return type == ELF_VALUE_TYPE_TABLE; }
static inline b32 elf_value_type_is_user(ELF_ValueType type)     { return type == ELF_VALUE_TYPE_USER_OBJECT; }
static inline b32 elf_value_type_is_function(ELF_ValueType type) { return type == ELF_VALUE_TYPE_CFUNCTION; }
static inline b32 elf_value_type_is_closure(ELF_ValueType type)  { return type == ELF_VALUE_TYPE_CLOSURE; }
static inline b32 elf_value_type_is_handle(ELF_ValueType type)   { return type == ELF_VALUE_TYPE_HANDLE; }

static inline b32 elf_value_type_is_numeric(ELF_ValueType type)
{
	return elf_value_type_is_number(type) || elf_value_type_is_integer(type);
}

static inline b32 elf_value_type_is_callable(ELF_ValueType type)
{
	return elf_value_type_is_closure(type) || elf_value_type_is_function(type);
}

static inline ELF_ValueType value_type(elf_Value value) { return value.type; }

const char *value_type_name(ELF_ValueType type);

static inline b32 value_is_nil(elf_Value value)      { return value.type == ELF_VALUE_TYPE_NIL; }
static inline b32 value_is_number(elf_Value value)   { return value.type == ELF_VALUE_TYPE_NUMBER; }
static inline b32 value_is_integer(elf_Value value)  { return value.type == ELF_VALUE_TYPE_INTEGER; }
static inline b32 value_is_atom(elf_Value value)     { return value.type == ELF_VALUE_TYPE_ATOM; }
static inline b32 value_is_table(elf_Value value)    { return value.type == ELF_VALUE_TYPE_TABLE; }
static inline b32 value_is_user(elf_Value value)     { return value.type == ELF_VALUE_TYPE_USER_OBJECT; }
static inline b32 value_is_function(elf_Value value) { return value.type == ELF_VALUE_TYPE_CFUNCTION; }
static inline b32 value_is_closure(elf_Value value)  { return value.type == ELF_VALUE_TYPE_CLOSURE; }
static inline b32 value_is_handle(elf_Value value)   { return value.type == ELF_VALUE_TYPE_HANDLE; }

static inline b32 value_is_dead(elf_Value value) { return value_is_nil(value); }
static inline b32 value_is_key(elf_Value value)  { return !value_is_nil(value); }

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
	return elf_value_type_is_object(value.type);
}

static inline i64 value_as_integer(elf_Value value)          { return value.x_int; }
static inline f64 value_as_number(elf_Value value)           { return value.x_num; }
static inline elf_Handle value_as_handle(elf_Value value)    { return value.x_sys; }
static inline elf_Object *value_as_object(elf_Value value)   { return value.x_obj; }
static inline elf_Atom *value_as_atom(elf_Value value)       { return value.x_atom; }
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

static inline elf_Value value_from_handle(elf_Handle handle)
{
	elf_Value value = {};
	value.type = ELF_VALUE_TYPE_HANDLE;
	value.x_sys = handle;
	return value;
}

static inline elf_Value value_from_atom(elf_Atom *atom)
{
	ASSERT(atom != 0);
	elf_Value value = {};
	value.type = ELF_VALUE_TYPE_ATOM;
	value.x_atom = atom;
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

#endif
