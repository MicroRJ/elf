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
#define ELF_FUNCTION(NAME) int (NAME)(elf_State *S, int nargs, int nrets)
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
up with GCType */
typedef enum {
	/* the value is nil */
	ELF_TNIL = 0,
	/* the value is a tombstone, values of this type only reside in closed systems */
	ELF_TTOMB,
	/* the value is a 64 bit floating point number */
	ELF_TNUMBER,
	/* the value is a 64 bit integer */
	ELF_TINTEGER,
	/* the value is a handle */
	ELF_THANDLE,
	/* the value is a function */
	ELF_TFUNCTION,
	/* the value is a custom object */
	ELF_TUSER,
	/* the value is a closure object */
	ELF_TCLOSURE,
	/* the value is string object */
	ELF_TSTRING,
	/* the value is table object */
	ELF_TTABLE,
} elf_Tag;



elf_pubapi elf_State *elf_new();
elf_pubapi void elf_end(elf_State *);


// pass in a file name, the result is on the stack
// todo: support for loading code files from memory is pending
elf_pubapi int elf_pushcodefile(elf_State *S, const char *name);



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

elf_pubapi void elf_setfield(elf_State *);
elf_pubapi void elf_arrayadd(elf_State *);
elf_pubapi void elf_arrayget(elf_State *);

#define elf_pushtrue(S) elf_pushint(S, 1)
#define elf_pushfalse(S) elf_pushint(S, 0)

elf_pubapi void elf_pushnil(elf_State *);
elf_pubapi void elf_pushint(elf_State *, elf_Integer);
elf_pubapi void elf_pushnum(elf_State *, elf_Number);
elf_pubapi void elf_pushtab(elf_State *);
elf_pubapi void elf_pushtext(elf_State *, const char *);
elf_pubapi void elf_pushtext2(elf_State *, const char *, int length);
elf_pubapi void elf_pushfun(elf_State *, elf_Function);
elf_pubapi void elf_pushsys(elf_State *, elf_Handle);

elf_pubapi void elf_pushglobals(elf_State *);


elf_pubapi elf_Tag elf_loadtype(elf_State *, int x);
elf_pubapi const char *elf_loadtext(elf_State *, int x);
elf_pubapi elf_Number elf_loadnum(elf_State *, int x);
elf_pubapi elf_Integer elf_loadint(elf_State *, int x);
elf_pubapi elf_Handle elf_loadsys(elf_State *, int x);

int elf_load_const_expr_from_text(elf_State *S, const char *name, const char *contents);



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

