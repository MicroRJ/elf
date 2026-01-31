//
// See Copyright Notice Below.
//
#ifndef _elf_lang_
#define _elf_lang_




#if defined(__EMSCRIPTEN__)
   #define elf_pubapi  EMSCRIPTEN_KEEPALIVE
   #define ELF_EXPORT  EMSCRIPTEN_KEEPALIVE
#else
   #define ELF_EXPORT __declspec(dllexport)

   #if defined(BUILD_STATIC)
      #define elf_pubapi static
   #else
      #define elf_pubapi
   #endif
#endif





typedef struct elf_State   elf_State;


#include "elf_coretypes.h"



/*
* Standard elf function signature:
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





typedef enum {
   // the value is nil
   ELF_TNIL = 0,
   // the value is a tombstone, should not be seen outside
   // of table code
   ELF_TTOMB,
   // the value is a 64 - bit floating point number
   ELF_TNUMBER,
   // the value is a 64 - bit integer
   ELF_TINTEGER,
   // the value is a non-arithmetic 64 - bit integer
   ELF_THANDLE,
   // the value is a 4 component 32 - bit vector
   ELF_TVEC4,

   ELF_TFUNCTION,
   ELF_TUSER,
   ELF_TCLOSURE,
   ELF_TTABLE,
   ELF_TSTRING,
   ELF_TBUFFER,
   ELF_TCOUNT_,
} elf_Tag;






elf_State *elf_new();
void elf_end(elf_State *);




//
//
//	todo: the results are placed starting where the function is at,
// but this is pending, due to internal bytecode limitations,
// the user facing API should be able to leave the function
// on the stack and the results below it.
//
//
// To call a function:
// Push the function and push the 'this' arg, then push
// additional arguments.
// * nargs does not include the closure.
// * 'this' can be nil.
//
//  elf_pushfun(...)
//  elf_push_nil(...)
//  elf_call()
//
//  The result is the number of returns,
//  the stack pointer is below the return values, you can pop them.
//
//     STACK LAYOUT:
//
//     | PRE-CALL | POST-CALL
//  0  |  FUNC    | RET 0
//  1  | 'THIS'   | RET 1
//  2  |  ARG-0   | RET 2
//  3  |  ARG-1   | RET 3
//  N  |  ARG-N   | RET N
//
//
//
int elf_call(elf_State *, int nargs, int nrets);
int elf_tailcall(elf_State *, int nargs, int nrets);

void elf_setfield(elf_State *);
void elf_arrayadd(elf_State *);
void elf_arrayget(elf_State *);




void elf_error(elf_State *, int error, const char *message, ...);


#define elf_pushbool(S, x) elf_pushint(S, x)
#define elf_pushtrue(S) elf_pushbool(S, 1)
#define elf_pushfalse(S) elf_pushbool(S, 0)

void elf_push_nil(elf_State *);
void elf_pushint(elf_State *, elf_Integer);
void elf_push_num(elf_State *, elf_Number);
void elf_pushtab(elf_State *);
void elf_pushtext(elf_State *, const char *);
void elf_push_textl(elf_State *, const char *, int length);
void elf_pushfun(elf_State *, elf_Function);
void elf_pushsys(elf_State *, elf_Handle);


// allocates managed memory, the result is visible
// in the form of an USER object.
// You must provide a table on the stack that is the
// meta-table for this object.
void *elf_pushuser(elf_State *S, int size);





// push the globals table onto the stack
void elf_pushglobals(elf_State *);





// The following is true for
//
// elf_pushconstexpr
// elf_pushcodefile
//
// If no text  is provided then name indicates the file to load
// the text from.
//
// If text is provided, then name is simply used for printouts.
//
//	The integer result indicates success.
//


// push a constant expression onto the stack, value is on the stack.
int elf_pushconstexpr(elf_State *S, const char *name, const char *text);



// push an executable script onto the stack, closure is on the stack.
int elf_pushcodefile(elf_State *S, const char *name, const char *text);





elf_Tag elf_loadtype(elf_State *, int x);
elf_Tag elf_loadpush(elf_State *, int x);
const char *elf_loadtext(elf_State *, int x);
const char *elf_loadtextl(elf_State *, int x, int *l);
elf_Number elf_load_num(elf_State *, int x);
elf_Integer elf_loadint(elf_State *, int x);
elf_Handle elf_loadsys(elf_State *, int x);








/* garbage collector */

enum {
   ELF_GC_ACTIVE = 0,
   ELF_GC_PAUSED,
   /* get the state of the garbage collector, not a valid state */
   ELF_GC_GETSTATE = 255,
};

/* returns the prior state of the GC */
int elf_gcstate(elf_State *, int state);







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

