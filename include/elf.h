//
// See Copyright Notice Below.
//
#ifndef _elf_lang_
#define _elf_lang_

typedef struct elf_State 	elf_State;
typedef struct elf_Object 	elf_Object;
typedef struct elf_Table 	elf_Table;
typedef struct elf_String 	elf_String;
typedef struct elf_Closure elf_Closure;
typedef struct elf_Value   elf_Value;


#include "elf_configs.h"
#include "elf_coretypes.h"


typedef elf_u32 elf_HashInt;
typedef elf_i64 elf_IndexInt;


#define ELF_FUNCTION(NAME) int (NAME)(elf_State *S)
typedef ELF_FUNCTION(* elf_Function);

//
// this is just a helper struct for creating libraries
//
typedef struct {
	char         *name;
	elf_Function  function;
} elf_Binding;


/* first object tag must be OBJ, all other
objects come after it, same order as object
types... if this changes then ensure it lines
up with elf_GC_Ty */
typedef enum {
	/* the value is nil */
	elf_tag_nil = 0,
	/* the value is a tombstone, values of this type only reside in closed systems */
	elf_tag_tomb,
	/* the value is a 64 bit floating point number */
	elf_tag_num,
	/* the value is a 64 bit integer */
	elf_tag_int,
	/* the value is a handle */
	elf_tag_Handle,
	/* the value is a function */
	elf_tag_Function,
	/* the value is a custom object */
	elf_tag_UserObject,
	/* the value is a closure object */
	elf_tag_Closure,
	/* the value is string object */
	elf_tag_String,
	/* the value is table object */
	elf_tag_Table,
} elf_Tag;

// todo: init is internal stuff because it takes a pointer
ELF_API void elf_init(elf_State *S);

ELF_API int elf_exec(elf_State *R, bool as_expr, int nargs, int nrets, elf_String *name, elf_String *contents);
ELF_API elf_b32 elf_exec_file(elf_State *S, char *name, int nargs, int nrets);
ELF_API int elf_call(elf_State *S, int nargs, int nrets);

int elf_push_this(elf_State *S);
int elf_push_globals(elf_State *S);
int elf_push_int(elf_State *S, elf_Int);
int elf_push_nil(elf_State *S);
int elf_push_table(elf_State *S);
int elf_push_string(elf_State *S, const char *);
int elf_push_num(elf_State *S, elf_Num);
int elf_push_function(elf_State *S, elf_Function);
int elf_push_handle(elf_State *S, elf_Handle);

/* the following are stack based instructions, they require the
arguments on the stack
todo: need version of this that takes the stack address */
void elf_table_set(elf_State *S);
void elf_array_add(elf_State *S);



elf_Value *elf_get_stack(elf_State *S);
elf_Value *elf_get_stack_ptr(elf_State *S);



elf_Object  *elf_get_this   (elf_State *S);
/* all of these add 1 to the address passed in, so
argument 0 is one past 'this' argument, you can pass
in -1 to get the 'this' argument or use 'get_this'
alternatively */
elf_Value    elf_get_arg    (elf_State *S, int stk);

elf_Int      elf_get_int     (elf_State *S, int stk);
elf_Num      elf_get_num     (elf_State *S, int stk);
elf_Handle   elf_get_sysobj  (elf_State *S, int stk);

elf_String  *elf_get_string  (elf_State *S, int stk);
char        *elf_get_text    (elf_State *S, int stk);
elf_Object  *elf_get_object  (elf_State *S, int stk);
elf_Table   *elf_get_table   (elf_State *S, int stk);
elf_Closure *elf_get_closure (elf_State *S, int stk);

ELF_API elf_Tag elf_get_tag(elf_State *S, int x);

/* getting call frame information */
ELF_API int elf_get_num_args(elf_State *S);



/* garbage collector */

typedef enum {
	ELF_GC_PAUSED = 0,
	ELF_GC_ACTIVE,
} elf_GC_State;

/* changes the status of the collector to be active or
inactive, if inactive no gc checks are issued when
allocating objects */
void elf_gc_state(elf_State *, elf_GC_State state);

/* performs a garbage collection check */
void elf_gc_check(elf_State *);


void elf_error(elf_State *S, int instr, const char *error);



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

