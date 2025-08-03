//
// See Copyright Notice Below.
//
#ifndef _elf_lang_
#define _elf_lang_


#if defined(__EMSCRIPTEN__)
	#define elf_userapi EMSCRIPTEN_KEEPALIVE
	#define ELF_EXPORT  EMSCRIPTEN_KEEPALIVE
#else
	#define ELF_EXPORT __declspec(dllexport)

	#if defined(BUILD_STATIC)
		#define elf_userapi static
	#else
		#define elf_userapi
	#endif
#endif


typedef struct elf_State 	elf_State;
typedef struct elf_Object 	elf_Object;
typedef struct elf_Table 	elf_Table;
typedef struct elf_String 	elf_String;
typedef struct elf_Closure elf_Closure;
typedef struct elf_Value   elf_Value;


#include "elf_coretypes.h"


typedef elf_u32 elf_HashInt;
typedef elf_i64 elf_IndexInt;


/*
todo: #TOMORROW
the stack address of the first argument and the number of arguments,
the 'this' argument is always -1, the closure is always -2 */
// #define ELF_FUNCTION(NAME) int (NAME)(elf_State *, int args, int nargs, int nrets)

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
	elf_tag_Nil = 0,
	/* the value is a tombstone, values of this type only reside in closed systems */
	elf_tag_Tomb,
	/* the value is a 64 bit floating point number */
	elf_tag_Num,
	/* the value is a 64 bit integer */
	elf_tag_Int,
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

// todo: delete!
elf_userapi elf_State *elf_new();

/* push call arguments, push the (name of the file),
push the (file contents or the file handle) */
elf_userapi int elf_exec(elf_State *, int nargs, int nrets, bool asexpr);

/* push Closure or Function push call arguments  */
elf_userapi int elf_call(elf_State *, int nargs, int nrets);


elf_userapi int elf_read_file(elf_State *, int size);


elf_userapi int elf_push_this(elf_State *);
elf_userapi int elf_push_globals(elf_State *);
elf_userapi int elf_push_int(elf_State *, elf_Int);
elf_userapi int elf_push_nil(elf_State *);
elf_userapi int elf_push_table(elf_State *);
elf_userapi int elf_push_string(elf_State *, const char *);
elf_userapi int elf_push_num(elf_State *, elf_Num);
elf_userapi int elf_push_function(elf_State *, elf_Function);
elf_userapi int elf_push_handle(elf_State *, elf_Handle);

elf_userapi char *elf_get_text_from_string_on_stack(elf_State *, int stk);

/* the following are stack based instructions, they require the
arguments on the stack.
the arguments except for the subject are popped.
todo: version of this that takes the stack address? */
elf_userapi elf_IndexInt elf_table_set(elf_State *);
elf_userapi elf_IndexInt elf_array_add(elf_State *);

elf_userapi int elf_get_num_args(elf_State *);


// todo: TOMORROW, instead have a single API for getting
// anything anywhere on the stack, and the elf function signature
// tells the user the first argument index
// todo: @deprecated
elf_userapi elf_Tag    elf_get_argtag(elf_State *, int argi);
// todo: @deprecated
elf_userapi elf_Int    elf_get_intarg(elf_State *, int argi);
// todo: @deprecated
elf_userapi elf_Num    elf_get_numarg(elf_State *, int argi);
// todo: @deprecated
elf_userapi elf_Handle elf_get_sysarg(elf_State *, int argi);
// todo: @deprecated
// todo: also this should return constant memory!
elf_userapi char      *elf_get_text_arg(elf_State *, int argi);



/* garbage collector */

typedef enum {
	ELF_GC_PAUSED = 0,
	ELF_GC_ACTIVE,
	/* get the state of the garbage collector, not a valid state */
	ELF_GC_GETSTATE = 255,
} elf_GC_State;

/* returns the prior state of the GC */
int elf_gc_state(elf_State *, elf_GC_State state);

/* performs a garbage collection check */
void elf_gc_check(elf_State *);


void elf_error(elf_State *, int instr, const char *error);



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

