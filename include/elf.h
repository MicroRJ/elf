//
// See Copyright Notice Below.
//
#ifndef _elf_lang_
#define _elf_lang_

#if defined(__EMSCRIPTEN__)
	#define elf_pubapi EMSCRIPTEN_KEEPALIVE
	#define ELF_EXPORT  EMSCRIPTEN_KEEPALIVE
#else
	#define ELF_EXPORT __declspec(dllexport)

	#if defined(BUILD_STATIC)
		#define elf_pubapi static
	#else
		#define elf_pubapi
	#endif
#endif


typedef struct elf_State 	elf_State;
typedef struct elf_Object 	elf_Object;
typedef struct elf_Table 	elf_Table;
typedef struct elf_String 	elf_String;
typedef struct elf_Closure elf_Closure;
typedef struct elf_Value   elf_Value;


#include "elf_coretypes.h"


typedef int elf_stkid;


/*
* The core elf function has the following signature:
* args: which is the stack address of the first argument, 'this' is 0.
* nargs: the number of arguments
* nrets: the number of expected results
*/
#define ELF_FUNCTION(NAME) int (NAME)(elf_State *S, int args, int nargs, int nrets)
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
elf_pubapi elf_State *elf_new();


/* push call arguments, push the (name of the file),
push the (file contents or the file handle) */
elf_pubapi int elf_loadcode(elf_State *inter, bool asexpr);



/*
* Push the function and push the 'this' arg, then push additional arguments.
* nargs does not include the closure.
* nargs must be atleast 1 because since the 'this' arg is always present,
* 'this' can be nil.
*
* 	elf_pushfun(...)
* 	elf_pushnil(...)
* 	elf_call()
*
*	the result is the number of returns,
*  the stack pointer is below the return values, such that you can
*  pop them.
*
*		[ FUNCTION ] = [ RET-0 ]
*		[ 'THIS'   ] = [ RET-1 ]
*		[ ARG-0    ] = [ RET-2 ]
*		[ ARG-1    ] = [ RET-3 ]
*		[ ARG-N    ] = [ RET-N ]
*
*	* the number of returns does not have to match the number of arguments,
*  and the stack pointer will on top of the last return.
*
*
*/
elf_pubapi int elf_call(elf_State *, int nargs, int nrets);

// todo: should these return instead the numer of returns?
// similar to the elf signature?
elf_pubapi int elf_readfileh(elf_State *, elf_Handle file, int size);
elf_pubapi int elf_readfilen(elf_State *, const char *name, int size);
elf_pubapi int elf_readfile(elf_State *, int stk, int size);
elf_pubapi void elf_setfield(elf_State *);
elf_pubapi void elf_arrayadd(elf_State *);


elf_pubapi elf_stkid elf_gettop(elf_State *);
elf_pubapi elf_Tag elf_gettag(elf_State *, elf_stkid from);

elf_pubapi elf_stkid elf_pushmetatab(elf_State *, elf_stkid from);
elf_pubapi elf_stkid elf_pushglobals(elf_State *);
elf_pubapi elf_stkid elf_pushint(elf_State *, elf_Integer);
elf_pubapi elf_stkid elf_pushnil(elf_State *);
elf_pubapi elf_stkid elf_pushnum(elf_State *, elf_Number);
elf_pubapi elf_stkid elf_pushtab(elf_State *);
elf_pubapi elf_stkid elf_pushstr(elf_State *, const char *);
elf_pubapi elf_stkid elf_pushstrl(elf_State *, const char *, int length);
elf_pubapi elf_stkid elf_pushfun(elf_State *, elf_Function);
elf_pubapi elf_stkid elf_pushsys(elf_State *, elf_Handle);

elf_pubapi elf_Number elf_tonum(elf_State *, elf_stkid stk);
elf_pubapi elf_Integer elf_toint(elf_State *, elf_stkid stk);
elf_pubapi const char *elf_tostr(elf_State *, elf_stkid stk);
elf_pubapi elf_Handle elf_tosys(elf_State *, elf_stkid stk);




// todo: @deprecated
elf_pubapi elf_Integer    elf_get_intarg(elf_State *, int argi);
// todo: @deprecated
// todo: @deprecated
elf_pubapi elf_Handle elf_get_sysarg(elf_State *, int argi);
// todo: @deprecated
elf_pubapi char      *elf_get_text_arg(elf_State *, int argi);



/* garbage collector */

typedef enum {
	ELF_GC_PAUSED = 0,
	ELF_GC_ACTIVE,
	/* get the state of the garbage collector, not a valid state */
	ELF_GC_GETSTATE = 255,
} elf_GC_State;

/* returns the prior state of the GC */
int elf_gcstate(elf_State *, int state);

/* performs a garbage collection check */
void elf_gccheck(elf_State *);


void elf_error(elf_State *, int instr, const char *error);


// names of all the overloads you can do in elf
#define ELF_OVERLOAD_ADD   "__add"
#define ELF_OVERLOAD_SUB   "__sub"
#define ELF_OVERLOAD_MUL   "__mul"
#define ELF_OVERLOAD_DIV   "__div"
#define ELF_OVERLOAD_INDEX "__index"
#define ELF_OVERLOAD_FIELD "__field"


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

