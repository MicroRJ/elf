#ifndef ELF_H
#define ELF_H

#include <stdarg.h>

#if !defined(HAS_BOOL)
typedef signed int bool;
#endif

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
typedef elf_u64 elf_Handle;

#define ELF_HINVALID ((elf_Handle)0)
#define ELF_IS_HANDLE_INVALID(H) ((H) == ELF_HINVALID)

typedef struct elf_State elf_State;
typedef struct elf_Table elf_Table;
typedef struct elf_Atom elf_Atom;

typedef struct
{
	char   *data;
	elf_u64 size;
}
elf_StrSlice;

typedef struct elf_Arena elf_Arena;
typedef struct
{
	elf_Arena *arena;
	elf_u64    regress;
}
elf_Scratch;

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
	ELF_VALUE_TYPE_HANDLE,
	ELF_VALUE_TYPE_VECTOR,
	ELF_VALUE_TYPE_CFUNCTION,
	ELF_VALUE_TYPE_USER_OBJECT,
	ELF_VALUE_TYPE_CLOSURE,
	ELF_VALUE_TYPE_TABLE,
	ELF_VALUE_TYPE_ATOM,
	ELF_VALUE_TYPE_COUNT_,
}
elf_ValueType;

typedef enum
{
	ELF_GC_ACTIVE = 0,
	ELF_GC_PAUSED,
	ELF_GC_GETSTATE = 255,
}
elf_GCMode;

elf_State *elf_create_state(void);
void elf_destroy_state(elf_State *state);

elf_Arena *elf_create_arena(elf_u64 initial_reserve);
void elf_destroy_arena(elf_Arena *arena);
void *elf_arena_push(elf_Arena *arena, elf_u64 size);
void *elf_arena_push_zero(elf_Arena *arena, elf_u64 size);
void *elf_arena_push_copy(elf_Arena *arena, elf_u64 size, const void *data);
char *elf_arena_push_data(elf_Arena *arena, const void *data, elf_u64 size);
char *elf_arena_push_text(elf_Arena *arena, const char *text);
char *elf_arena_push_char(elf_Arena *arena, char chr);
void elf_arena_push_repeat(elf_Arena *arena, char chr, elf_u32 count);
char *elf_arena_pushfv(elf_Arena *arena, const char *format, va_list args);
char *elf_arena_pushf(elf_Arena *arena, const char *format, ...);

elf_Scratch elf_get_scratch(void);
void elf_end_scratch(elf_Scratch scratch);

elf_u32 elf_call(elf_State *state, elf_u32 nargs, elf_u32 nrets);
elf_u32 elf_do_tail_call(elf_State *state, elf_u32 nargs, elf_u32 nrets);

void elf_error(elf_State *state, int error, const char *message, ...);

void elf_push_nil(elf_State *state);
void elf_push_int(elf_State *state, elf_Integer value);
void elf_push_num(elf_State *state, elf_Number value);
void elf_push_fun(elf_State *state, elf_Function function);
void elf_push_hnd(elf_State *state, elf_Handle handle);
void elf_push_cstr(elf_State *state, const char *data);
void elf_push_str(elf_State *state, const char *data, int size);
elf_Table *elf_push_new_table(elf_State *state);

void elf_push_env(elf_State *state);
void elf_tab_set(elf_State *state);
void elf_arr_add(elf_State *state);
void elf_arr_get(elf_State *state);

int elf_push_constant_expr(elf_State *state, const char *name, elf_StrSlice source);
int elf_push_json(elf_State *state, const char *name, elf_StrSlice source);
int elf_push_code_source(elf_State *state, const char *name, elf_StrSlice source);
int elf_push_code_file(elf_State *state, const char *name);

elf_ValueType elf_arg_type(elf_State *state, int index);
elf_StrSlice  elf_arg_str_copy(elf_State *state, int index, elf_Arena *arena);
elf_Number    elf_arg_num(elf_State *state, int index);
elf_Integer   elf_arg_int(elf_State *state, int index);
elf_Handle    elf_arg_hnd(elf_State *state, int index);

elf_StrSlice elf_atom_copy_text(elf_Arena *arena, elf_Atom *atom);

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
