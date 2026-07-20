#ifndef ELF_H
#define ELF_H

#define ELF_VERSION "0.1.0-dev"
#define ELF_STACK_API_VERSION 1

typedef signed char        elf_i8;
typedef unsigned char      elf_u8;
typedef signed short       elf_i16;
typedef unsigned short     elf_u16;
typedef signed int         elf_i32;
typedef unsigned int       elf_u32;
typedef signed long long   elf_i64;
typedef unsigned long long elf_u64;
typedef float              elf_f32;
typedef double             elf_f64;
typedef elf_i32            elf_b32;

typedef elf_i64 elf_Integer;
typedef elf_f64 elf_Number;
typedef elf_u32 elf_Ref;

#define ELF_NO_REF ((elf_Ref)0)

typedef struct elf_State elf_State;
typedef struct elf_Table elf_Table;
typedef struct elf_String elf_String;

typedef struct
{
	char   *data;
	elf_u64 size;
}
elf_StrSlice;

#define ELF_FUNCTION(NAME) int (NAME)(elf_State *S, int nargs, int nrets)
typedef ELF_FUNCTION(*elf_Function);

typedef struct
{
	char         *name;
	elf_Function function;
}
elf_Binding;

typedef enum
{
	ELF_VALUE_TYPE_NIL = 0,
	ELF_VALUE_TYPE_NUMBER,
	ELF_VALUE_TYPE_INTEGER,
	ELF_VALUE_TYPE_CFUNCTION,
	ELF_VALUE_TYPE_USER_OBJECT,
	ELF_VALUE_TYPE_CLOSURE,
	ELF_VALUE_TYPE_TABLE,
	ELF_VALUE_TYPE_ATOM,
	ELF_VALUE_TYPE_STRING = ELF_VALUE_TYPE_ATOM,
	ELF_VALUE_TYPE_COUNT_,
}
elf_ValueType;

typedef struct
{
	elf_ValueType type;
	union
	{
		elf_Integer  integer;
		elf_Number   number;
		elf_String  *string;
		elf_Table   *table;
	} as;
}
elf_ValueView;

typedef enum
{
	ELF_GC_ACTIVE = 0,
	ELF_GC_PAUSED,
	ELF_GC_GETSTATE = 255,
}
elf_GCMode;

elf_State *elf_create_state(void);
void elf_destroy_state(elf_State *state);
const char *elf_version(void);
void elf_set_user_data(elf_State *state, void *user_data);
void *elf_get_user_data(elf_State *state);
void elf_register_library(elf_State *state, const char *name, const elf_Binding *bindings, elf_u32 count);

elf_u32 elf_call(elf_State *state, elf_u32 nargs, elf_u32 nrets);
elf_u32 elf_tail_call(elf_State *state, elf_u32 nargs, elf_u32 nrets);

void elf_push_nil(elf_State *state);
void elf_push_int(elf_State *state, elf_Integer value);
void elf_push_num(elf_State *state, elf_Number value);
void elf_push_fun(elf_State *state, elf_Function function);
void elf_push_cstr(elf_State *state, const char *data);
void elf_push_str(elf_State *state, const char *data, int size);

elf_ValueView elf_peek_value(elf_State *state, elf_u32 depth);
void elf_pop_values(elf_State *state, elf_u32 count);

// Todo, we may want to pass in the state for the public API
elf_String *elf_retain_str(elf_String *string);
void elf_release_str(elf_String *string);
const char *elf_str_data(elf_String *string);
elf_u32 elf_str_size(elf_String *string);
elf_u32 elf_str_hash(elf_String *string);

elf_Table *elf_retain_table(elf_Table *table);
void elf_release_table(elf_Table *table);
elf_u32 elf_table_length(const elf_Table *table);
elf_ValueView elf_get_field(elf_State *state, elf_Table *table, const char *field);
elf_ValueView elf_get_index(elf_State *state, elf_Table *table, elf_u32 index);

// Todo, remove this?
elf_b32 elf_table_next(elf_Table *table, elf_u32 *cursor, elf_ValueView *key, elf_ValueView *value);
// Todo, remove this!
void elf_push_field(elf_State *state, elf_Table *table, const char *field);

int elf_push_constant_expr(elf_State *state, const char *name, elf_StrSlice source);
int elf_push_json(elf_State *state, const char *name, elf_StrSlice source);
int elf_push_code_source(elf_State *state, const char *name, elf_StrSlice source);
int elf_push_code_file(elf_State *state, const char *name);

elf_ValueType elf_arg_type(elf_State *state, int index);
elf_String   *elf_arg_str(elf_State *state, int index);
elf_Table    *elf_arg_table(elf_State *state, int index);
elf_Number    elf_arg_num(elf_State *state, int index);
elf_Integer   elf_arg_int(elf_State *state, int index);

/*
** Stack API (version 1)
**
** Indices preserve Elf's existing zero-based frame convention: index 0 is
** the receiver (`this`) in a native call, index 1 is its first explicit
** argument, and negative indices address values back from the stack top.
*/
elf_i32 elf_stack_get_top(elf_State *state);
elf_i32 elf_stack_arg_count(elf_State *state);
elf_i32 elf_stack_abs_index(elf_State *state, elf_i32 index);
elf_b32 elf_stack_is_valid(elf_State *state, elf_i32 index);
elf_b32 elf_stack_set_top(elf_State *state, elf_i32 top);
elf_b32 elf_stack_pop(elf_State *state, elf_u32 count);
elf_b32 elf_stack_push_value(elf_State *state, elf_i32 index);

elf_ValueType elf_stack_type(elf_State *state, elf_i32 index);
elf_b32 elf_stack_is_nil(elf_State *state, elf_i32 index);
elf_b32 elf_stack_is_numeric(elf_State *state, elf_i32 index);
elf_b32 elf_stack_is_callable(elf_State *state, elf_i32 index);
elf_b32 elf_stack_to_int(elf_State *state, elf_i32 index, elf_Integer *value);
elf_b32 elf_stack_to_num(elf_State *state, elf_i32 index, elf_Number *value);
elf_b32 elf_stack_to_str(elf_State *state, elf_i32 index, elf_StrSlice *value);

void elf_stack_new_table(elf_State *state);
elf_b32 elf_stack_length(elf_State *state, elf_i32 index, elf_u32 *length);
elf_b32 elf_stack_get_field(elf_State *state, elf_i32 index, const char *field);
elf_b32 elf_stack_set_field(elf_State *state, elf_i32 index, const char *field);
elf_b32 elf_stack_get_index(elf_State *state, elf_i32 index, elf_u32 element);
elf_b32 elf_stack_set_index(elf_State *state, elf_i32 index, elf_u32 element);
elf_b32 elf_stack_add(elf_State *state, elf_i32 index);
elf_b32 elf_stack_next(elf_State *state, elf_i32 index, elf_u32 *cursor);
elf_b32 elf_stack_equal(elf_State *state, elf_i32 left, elf_i32 right);

void elf_stack_get_global(elf_State *state, const char *name);
elf_b32 elf_stack_set_global(elf_State *state, const char *name);

elf_Ref elf_create_ref(elf_State *state, elf_i32 index);
elf_b32 elf_push_ref(elf_State *state, elf_Ref reference);
elf_b32 elf_release_ref(elf_State *state, elf_Ref reference);

#endif

/*
** Copyright (C) 2023-2025 Dayan Rodriguez
**
** Permission is hereby granted, free of charge, to any person obtaining a copy
** of this software and associated documentation files (the "Software"), to deal
** in the Software without restriction, including without limitation the rights
** to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
** copies of the Software, and to permit persons to whom the Software is
** furnished to do so, subject to the following conditions:
**
** The above copyright notice and this permission notice shall be included in all
** copies or substantial portions of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
** AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
** LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
** OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
** SOFTWARE.
*/
