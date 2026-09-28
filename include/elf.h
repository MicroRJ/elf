#ifndef ELF_H
#define ELF_H

#define ELF_VERSION "0.2.0-dev"
#define ELF_API_VERSION 1

#include <stddef.h>
#include <stdint.h>

typedef int8_t      elf_i8;
typedef uint8_t     elf_u8;
typedef int16_t     elf_i16;
typedef uint16_t    elf_u16;
typedef int32_t     elf_i32;
typedef uint32_t    elf_u32;
typedef int64_t     elf_i64;
typedef uint64_t    elf_u64;
typedef float       elf_f32;
typedef double      elf_f64;
typedef elf_i32     elf_b32;

// slightly more readable semantic boolean type
typedef elf_b32 elf_Bool;
typedef elf_i32 elf_Index;
typedef size_t  elf_Size;
typedef elf_i64 elf_Int;
typedef elf_f64 elf_Num;

typedef elf_u32 elf_Ref;

#define ELF_NO_REF ((elf_Ref)(0))

typedef struct elf_State elf_State;

typedef struct
{
	char    *data;
	elf_Size size;
}
elf_StrSlice;

typedef enum
{
	ELF_ERROR_NONE = 0,
	ELF_ERROR_INVALID_ARGUMENT,
	ELF_ERROR_INVALID_INDEX,
	ELF_ERROR_INVALID_REFERENCE,
	ELF_ERROR_TYPE_MISMATCH,
	ELF_ERROR_STACK_UNDERFLOW,
	ELF_ERROR_OUT_OF_RANGE,
	ELF_ERROR_READONLY,
	ELF_ERROR_COMPILATION_FAILED,
}
elf_ErrorCode;

typedef enum
{
	ELF_DIAGNOSTIC_NOTE = 0,
	ELF_DIAGNOSTIC_WARNING,
	ELF_DIAGNOSTIC_ERROR,
}
elf_DiagnosticSeverity;

typedef enum
{
	ELF_DIAGNOSTIC_PHASE_LEXER = 0,
	ELF_DIAGNOSTIC_PHASE_PARSER,
	ELF_DIAGNOSTIC_PHASE_EVALUATION,
	ELF_DIAGNOSTIC_PHASE_LOWERING,
	ELF_DIAGNOSTIC_PHASE_BYTECODE,
}
elf_DiagnosticPhase;

typedef struct
{
	elf_DiagnosticSeverity severity;
	elf_DiagnosticPhase    phase;
	elf_StrSlice           source_name;
	elf_StrSlice           message;
	elf_u32                line;
	elf_u64                column;
}
elf_Diagnostic;

typedef struct
{
	const elf_Diagnostic *diagnostics;
	elf_Size              diagnostic_count;
	elf_Size              error_count;
	elf_Size              warning_count;
}
elf_CompileReport;

#define ELF_FUNCTION(NAME) int (NAME)(elf_State *S, int nargs, int nrets)
typedef ELF_FUNCTION(*elf_Function);

typedef enum
{
	ELF_VALUE_TYPE_NIL = 0,
	ELF_VALUE_TYPE_NUMBER,
	ELF_VALUE_TYPE_INTEGER,
	ELF_VALUE_TYPE_CFUNCTION,
	ELF_VALUE_TYPE_USER_OBJECT,
	ELF_VALUE_TYPE_CLOSURE,
	ELF_VALUE_TYPE_TABLE,
	ELF_VALUE_TYPE_STRING,
	ELF_VALUE_TYPE_COUNT_,
}
elf_ValueType;

elf_State *elf_create_state(void);
void elf_destroy_state(elf_State *state);
const char *elf_version(void);
void elf_set_user_data(elf_State *state, void *user_data);
void *elf_get_user_data(elf_State *state);
void elf_error(elf_State *state, const char *message);

elf_u32 elf_call(elf_State *state, elf_u32 nargs, elf_u32 nrets);
elf_u32 elf_tail_call(elf_State *state, elf_u32 nargs, elf_u32 nrets);

void elf_push_nil(elf_State *state);
void elf_push_int(elf_State *state, elf_Int value);
void elf_push_num(elf_State *state, elf_Num value);
void elf_push_fun(elf_State *state, elf_Function function);
void elf_push_cstr(elf_State *state, const char *data);
void elf_push_str(elf_State *state, const char *data, elf_Size size);

elf_ErrorCode elf_push_constant_expr(elf_State *state, const char *name, elf_StrSlice source, elf_CompileReport *report);
elf_ErrorCode elf_push_json(elf_State *state, const char *name, elf_StrSlice source, elf_CompileReport *report);
elf_ErrorCode elf_push_code_source(elf_State *state, const char *name, elf_StrSlice source, elf_CompileReport *report);
void elf_destroy_compile_report(elf_CompileReport *report);

elf_u32 elf_arg_count(elf_State *state);
elf_Index elf_get_top(elf_State *state);
elf_Index elf_abs_index(elf_State *state, elf_Index index);
elf_Bool elf_is_valid(elf_State *state, elf_Index index);
elf_ErrorCode elf_set_top(elf_State *state, elf_Index top);
elf_ErrorCode elf_pop(elf_State *state, elf_u32 count);
elf_ErrorCode elf_push_value(elf_State *state, elf_Index index);

elf_ValueType elf_type(elf_State *state, elf_Index index);
elf_Bool elf_is_nil(elf_State *state, elf_Index index);
elf_Bool elf_is_numeric(elf_State *state, elf_Index index);
elf_Bool elf_is_callable(elf_State *state, elf_Index index);
elf_Bool elf_to_int(elf_State *state, elf_Index index, elf_Int *value);
elf_Bool elf_to_num(elf_State *state, elf_Index index, elf_Num *value);
elf_Bool elf_to_str(elf_State *state, elf_Index index, elf_StrSlice *value);
elf_Bool elf_to_cstr(elf_State *state, elf_Index index, const char **value);
elf_Bool elf_push_value_text(elf_State *state, elf_Index index);
// Pushes constant-expression source for a supported acyclic data value.
elf_Bool elf_push_value_source(elf_State *state, elf_Index index);

void elf_new_table(elf_State *state);
elf_Bool elf_length(elf_State *state, elf_Index index, elf_u32 *length);
elf_Bool elf_get_field(elf_State *state, elf_Index index, const char *field);
elf_ErrorCode elf_set_field(elf_State *state, elf_Index index, const char *field);
elf_Bool elf_get_index(elf_State *state, elf_Index index, elf_u32 element);
elf_ErrorCode elf_set_index(elf_State *state, elf_Index index, elf_u32 element);
elf_ErrorCode elf_append(elf_State *state, elf_Index index);
elf_Bool elf_next(elf_State *state, elf_Index index, elf_u32 *cursor);
elf_Bool elf_equal(elf_State *state, elf_Index left, elf_Index right);

void elf_get_global(elf_State *state, const char *name);
elf_ErrorCode elf_set_global(elf_State *state, const char *name);

elf_Ref elf_create_ref(elf_State *state, elf_Index index);
elf_Bool elf_push_ref(elf_State *state, elf_Ref reference);
elf_ErrorCode elf_release_ref(elf_State *state, elf_Ref reference);

#endif

/*
** Copyright (C) 2023 Dayan Rodriguez
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
