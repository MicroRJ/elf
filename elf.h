/*
** See Copyright Notice Below.
** elf.h
** The λ elf language.
*/


#ifndef _elf_lang_
#define _elf_lang_



#if defined(__EMSCRIPTEN__)
	#define elAPI 		EMSCRIPTEN_KEEPALIVE
	#define elEXPORT 	EMSCRIPTEN_KEEPALIVE
	#define elTHREAD 	static
	#define elGLOBAL  static
#else
	#define elEXPORT 	__declspec(dllexport)
	#define elIMPORT 	__declspec(dllimport)
	#define elTHREAD 	static __declspec(thread)
	#define elGLOBAL  static

	#if defined(BUILD_STATIC)
		#define elAPI static
	#else
		#define elAPI
	#endif
#endif


/* todo: remove these from here?? */
#if !defined(__cplusplus)
	#define LITERAL(X) (X)
#else
	#define LITERAL(X) X
#endif


#define TO_TEXT_(X) #X
#define TO_TEXT(X) TO_TEXT_(X)

#define FUSE_(X,Y) X##Y
#define FUSE(X,Y) FUSE_(X,Y)

#if !defined(COUNTOF)
	#define COUNTOF(X) (sizeof(X)/sizeof(X[0]))
#endif


/* I don't think this is portable to CPP as is, the idea is to
use C's "cast to union types" for typechecking:
	https://gcc.gnu.org/onlinedocs/gcc/Cast-to-Union.html#Cast-to-a-Union-Type

	I suppose CPP people can use templates or something...
*/
#define UCAST(D,T) ( ((union { T _; }){D})._ )


/* todo: remove this from here */
#define NO_BYTE (-1)


#include "src/types.h"


/* todo: convert this to an offset  */
typedef char *Source;


#if defined(__cplusplus)
extern "C" {
#endif
#if defined(CRAPPY_FORMATTER)
}
#endif


elAPI void elf_put_nil(elState *S);
elAPI void elf_put_integer(elState *S, elInteger);
elAPI void elf_put_number(elState *S, elNumber);
elAPI void elf_put_object(elState *S, elObject *);
elAPI void elf_put_string(elState *S, elString *);
elAPI void elf_put_handle(elState *S, elHandle);
elAPI void elf_put_table(elState *S, elTable *);
elAPI void elf_put_closure(elState *S, elClosure *);
elAPI void elf_put_cfunction(elState *S, elCFunction);


elAPI elInteger elf_get_integer(elState *R, int arg);
elAPI elNumber elf_get_number(elState *R, int arg);
elAPI elString *elf_get_string(elState *R, int arg);
elAPI char *elf_get_text(elState *R, int arg);
elAPI elObject *elf_get_object(elState *R, int arg);
elAPI elTable *elf_get_table(elState *R, int arg);
elAPI elHandle elf_get_handle(elState *R, int arg);
elAPI elClosure *elf_get_closure(elState *R, int arg);


elAPI elString *elf_put_new_string(elState *, const char *text);
elAPI elObject *elf_put_new_object(elState *, elInteger size);
elAPI elTable *elf_put_new_table(elState *);
elAPI elString *elf_put_new_string2(elState *, elInteger length);


/* todo: move to table */
elAPI void elf_tsetx_bindings(elState *, elTable *, elCBinding *list, int num);

elAPI void elf_gset_bindings(elState *, elCBinding *list, int num);


elAPI void elf_gsetx_cfn(elState *R, char *name, elCFunction thing);
elAPI void elf_gsetx_int(elState *R, char *name, elInteger thing);
elAPI void elf_gsetx_tab(elState *R, char *name, elTable *thing);





typedef enum elGCColor {
	GC_WHITE = 0, GC_BLACK, GC_RED, GC_PINK, GC_TRAP,
} elGCColor;


typedef enum elGCTy {
	GC_OBJ = 0, GC_CLS, GC_STR, GC_TAB
} elGCTy;


typedef struct elObject {
	/* todo: compact this */
	elGCTy    type;
	elGCColor color;
	elTable  *metatable;
	/* todo: please remove this... */
	int       byte;
	short     tell;
} elObject;


/* first object tag must be OBJ, all other
objects come after it, same order as object
types... */
#define TAGLIST(_) _(NIL)_(NUM)_(INT)_(SYS)_(CFN)_(OBJ)_(CLS)_(STR)_(TAB)

#define TAGENUM(NAME) FUSE(TAG_,NAME),
typedef enum { TAGLIST(TAGENUM) } ValueTag;
#undef TAGENUM

#define TAGENUM(NAME) TO_TEXT(NAME),
elGLOBAL char const *tag2s[] = {
	TAGLIST(TAGENUM)
};
#undef TAGENUM




#define elNUM(thing) (LITERAL(elValue){ TAG_NUM, ((union { elNumber _; float __; elInteger I; }){thing}).I })
#define elINT(thing) (LITERAL(elValue){ TAG_INT, {(elInteger) UCAST(thing, elInteger)} })
#define elSYS(thing) (LITERAL(elValue){ TAG_SYS, {(elInteger) UCAST(thing, elHandle)} })
#define elTAB(thing) (LITERAL(elValue){ TAG_TAB, {(elInteger) UCAST(thing, elTable *)} })
#define elOBJ(thing) (LITERAL(elValue){ TAG_OBJ, {(elInteger) UCAST(thing, elObject *)} })
#define elSTR(thing) (LITERAL(elValue){ TAG_STR, {(elInteger) UCAST(thing, elString *)} })
#define elCLS(thing) (LITERAL(elValue){ TAG_CLS, {(elInteger) UCAST(thing, elClosure *)} })
#define elCFN(thing) (LITERAL(elValue){ TAG_CFN, {(elInteger) UCAST(thing, elCFunction)} })
#define elNIL() (LITERAL(elValue){TAG_NIL})


typedef struct elValue {
	unsigned char tag;
	union {
		elInteger x_int;
		elNumber  x_num;
		elAddr    x_ptr;
		elHandle  x_sys;
		elClosure *x_cls;
		elObject  *x_obj;
		elTable   *x_tab;
		elString  *x_str;
		elCFunction x_cfn;
	};
} elValue;


typedef struct elClosure {
	elObject       obj;
	elFunction   proto;
	elValue  values[1];
} elClosure;


#include "src/string.h"
#include "src/table.h"






#define elGETTOP(S)   ((S)->stack_ptr)
#define elSETTOP(S,X) (elGETTOP(S) = UCAST(X, elValue *))
#define elPUSH(S,X)   (* elGETTOP(S) ++ = (X))

#define elGETFRAME(S) ((S)->frame)




// #define elGETNARGS(S)   (elGETFRAME(S)->nargs-1)
#define elGETLOCAL(S,X) (elGETFRAME(S)->locals[X])
// #define elGETTHIS(S)    (elGETLOCAL(S,0).x_obj)
// #define elGETARG(S,X)   (elGETLOCAL(S,X+1))
#define elGETTAG(S,X)   (elGETARG(S,X).tag)


elAPI void elf_check_args(elState *R, char *fnname, int n, char *usage);
elAPI elObject *elGETTHIS(elState *S);
elAPI elValue elGETARG(elState *S, int X);
elAPI int elGETNARGS(elState *S);



#define elISOBJTAG(tag) ((tag) >= TAG_OBJ)
#define elISNUMTAG(tag) ((tag) == TAG_NUM || (tag) == TAG_INT)
#define elISFUNTAG(tag) ((tag) == TAG_CLS || (tag) == TAG_CFN)


#define elISNILOBJ(val) (elISOBJTAG((val).tag) && (val).x_obj == 0)


#define elISNIL(val) ((val).tag == TAG_NIL || elISNILOBJ(val))
#define elTONUM(val) ((val).tag == TAG_INT ? (elNumber)  (val).x_int : (val).x_num)
#define elTOINT(val) ((val).tag == TAG_NUM ? (elInteger) (val).x_num : (val).x_int)


#define elTOOBJ(thing) ((elObject*)(thing))
#define elOBJCOLOR(thing) (elTOOBJ(thing)->color)
#define elOBJTOTAG(typ) ((typ) + TAG_OBJ)



elAPI int elf_load_file(elState *, elString *name, int nargs, int nregs);
elAPI int elf_call_function(elState *R, int nargs, int nregs);
elAPI int elf_run(elState *);

/* todo: why are these public */
elInteger elf_trigger_collection_cycle(elState *R);
elInteger elf_mark_object(elObject *obj);

void *elf_new_object(elState *R, elGCTy type, elInteger length);


elAPI void elf_debugger(char *message);
elAPI void elf_fail(elState *R, int instr, const char *error);



elAPI elSymbolId elf_get_global_symbol(elModule *md, elString *name);
elAPI elSymbolId elf_gset(elModule *md, elString *name, elValue v);
elAPI elSymbolId elf_add_function(elModule *md, elFunction p);

elAPI int elf_get_instr_file(elModule *M, int instr);
elAPI char *elf_get_instr_line(elModule *M, int instr);
elAPI void elf_get_line_source_info(char *q, char *loc, int *linenum, char **lineloc);


#if defined(__cplusplus)
}
#endif




/* Notes on T: Stack top pointer...

T: Stack top pointer, (one past last live value).
This pointer has 2 main purposes, namely:

1) Serves for passing in arguments to other functions,
by acting as the reference pointer.

2) Marks the end of the live value range, so values
BEFORE top are live and kept.

Consequently, there are a few points which are
crucial to keep in mind.

Let's begin with stack frames, and the idea
of coverage and frame coverage.

First, a stack frame is a portion of the stack reserved
for a function call, when you call a function a new
stack frame is created of appropriate size.

	- The stack frame structure itself
contains additional information about the call, such
as the number of arguments, the closure (if any)
and more...

Only one stack frame is active at once, since we can
only be in one function at any given time.

When the function returns control, the stack frame
is removed, and the previous stack frame is restored.

Every function prototype states how many locals (nlocals)
are active at any given time.

For instance if a function has three variables say
'let a, b, c' then (nlocals) must be at least 3 plus any other
temporary registers or locals which the code emitter might have
used for some intermediate computations.

The process of determining how many register or locals a
function might necessitate depends on the code generator.

But one thing must be guaranteed, values MUST remain alive
for as long as they are used.

Keep in mind that when a local or register is overwritten
or not 'covered' (explained later)  anymore it could get
deallocated (if it is an object).

So (nlocals) effectively tells us the space on the stack
that must 'fully-covered' (explained later) at all times.

Therefore (nlocals) is then used to determine the size of
the stack frame for that function.


* Second, coverage.
One of the purposes of 'T' is to denote all the values
which are visible, reachable, or alive to the GC.
When the GC runs, it only sees values before T, and those
values are consequently kept.
So all values before T are said to be 'covered'.

When a new call frame is created, 'T' is incremented
to match the length of the call frame.

Therefore, the frame is 'fully covered', this is what I
refer to as 'full frame coverage'.

However, 'T' may not always cover the entire frame
(explained later), which is what I refer to
as 'partial frame coverage'.


The call instruction specifies which 'register' or local
contains the first call-argument in operand (x), and the
'number' of user-arguments in operand (y).
By call arguments I'm referring to the arguments of the
bytecode instruction itself, which include the
'user-arguments'.


call(function,context,user-arguments...)


Now, the function that actually performs the call
which is 'call_function' assumes 'T' points
to (closure + object + user-arguments), such
that 'T - 1' is the last user-argument.

[closure] <- instruction (x) operand
[context]
[user-arg-0]
[user-arg-1]
[unknown-value] <- 'T' (new location)
[unknown-value]
[unknown-value]
[unknown-value]
[unknown-value]
[unknown-value]
[unknown-value] <- 'T' (old location) (end of frame)

For this reason, 'T' at (new location) will no longer
fully cover our frame anymore as it was at (old location),
because we had to regress 'T' to (new location).

Given that the GC could trigger whilst our current
frame is only partially covered, you might ask
yourself, what if we collect objects from our
current frame?

The answer is this, the GC can't collect any
values that haven't come to be yet, technically...

The code generator emits instruction in order
of execution, therefore, even though our frame is only
partially covered, we don't have any values from 'T'
at (new-location) that need coverage because we haven't
even gotten to the instructions that will populate
those registers.

This means that no instruction should ever be able to
reference a register that is past or at 'T'. If it does
this is strictly an error.

After the call is completed, we must 'recover', ensure
'T' offers full coverage of our frame once more.
	*/

/* 7/25/24 10:50 PM

Notes on the collection cycle:

When I was putting this thing together I thought
that I could initialize objects to black to delay
their collection for at least one more cycle,
which at the time for some reason I thought was
useful.

Since I never really got any GC related errors
and it seemed like such a benign or arbitrary
thing, I didn't realize then is that this is
actually totally wrong.

So recently, now that I've been doing
more memory intensive stuff, I've been getting
constant GC crashes it was now that after
debugging virtually every other aspect of the
interpreter that I finally made the right
connection.

CYCLES!

Here's the explanation:

Collection works in 2 phases, the starting
phase, checks reachability, this is called
marking.
In this stage a white object means it was
never encountered before and we can mark it
now and also mark its children.
Here's the important part: in THIS phase,
BLACK objects mean they've already been
marked already, and thus we ignore them
and their children entirely.

As a side note:
If you encounter a black object it simply
means you've found another path that leads
to that object.
It also means that object is referenced
multiple times, by multiple paths.

Anyways, on to the second or last phase.
After we've conducted our reachability
pass, all reachable objects are black and
those that aren't remained white.
So in this phase, we collect all WHITE
objects, and turn BLACK object WHITE
to complete the CYCLE.

So note how the last phase sets up
objects so that they are ready to
enter the first phase.
Since both of these phases run together
one right after the other, the GC is
always ready to cycle, that is, to repeat
the process once more.

Objects always enter the GC cycle
from the start, never in between.
An object is never allocated in
between any of the phases.

And this is why marking an object black
initially is simply wrong.

Because if you were to mark an object as black,
it'd be like starting up that object in the
wrong phase entirely.

If an object is black, during the first phase
of the collection cycle, the collector thinks
that it has already been marked and its
children won't be marked.

Now, I haven't really thought about this that
much further ahead, but think that if you actually
wanted to flip the cycle, (objects are black initially)
you'd have to reverse the collection cycle,
collect objects first and then mark them, which I would
assume would bring other complications.

So here's an scenario that was crashing the
GC when objects where initially marked as black.

For instance, say you're enumerating a folder,
for each file, you create a corresponding file
object or table, you set the name, the path
and some other attributes, during that, the
file object enters the GC cycle, and it gets
marked as WHITE.
Now, after that you add the file object
to a global table where you store all the
files. The table is... initially black.

So now you have something like this:

	list_of_files: (BLACK)
		file_a (WHITE)
		file_b (WHITE)
		file_c (WHITE)

So now, the GC triggers again and all objects
are passed through the GC cycle.

Since the parent object, 'list_of_files' is
black, none of its children are marked as
reachable, consequently, they are all freed.

The lesson was:

PHASES, CYCLES AND STATES!

In summary, the color of an object is not just
whether it is reachable or not, but also which
phase of the GC cycle is on, and thus must be
colored accordingly.

Objects don't have to be marked black initially
in order to "delay" their collection:

When a new object is allocated it is guaranteed
to remain valid till the next collection pass,
by that time the object should already be somewhere
reachable, like the stack or globals.

	new_object() * GC could run, however, it runs
					   before the object is added to
					   the GC list.
					   Since the object is white,
					   it is ready to enter the GC
					   cycle.

	At this point the object is safe to access

	collect()	 * GC runs and it collects the object

	At this point the object isn't safe to unless
	it was added to the stack or globals.
	If the GC triggered the object was definitely
	collected if not reachable, since white.
*/


#endif
/*
** Copyright (C) 2023-2024 Dayan Rodriguez
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

