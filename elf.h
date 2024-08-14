/*
** See Copyright Notice Below.
** elf.h
** The λ elf language.
*/


#ifndef _elf_lang_
#define _elf_lang_


/*
** Configuration Macros (mostly temporary)
*/

#define elGC_MEM_THRESHOLD_MIN (elInteger) MEGABYTES(1)
#define elGC_MEM_THRESHOLD_MAX (elInteger) MEGABYTES(1024)

#define elGC_OBJ_THRESHOLD_MIN (elInteger) ((1024)*2)
#define elGC_OBJ_THRESHOLD_MAX (elInteger) ((2048)*4)


/* todo?: the stack isn't allocated dynamically... */
#define elDEFAULT_STACK_SIZE 4096



// #define elGC_MEM_THRESHOLD_MIN (elInteger) MEGABYTES(4)
// #define elGC_MEM_THRESHOLD_MAX (elInteger) MEGABYTES(512)
// #define elGC_OBJ_THRESHOLD_MIN (elInteger) ((2048)*1)
// #define elGC_OBJ_THRESHOLD_MAX (elInteger) ((2048)*2048)


#ifndef STB_SPRINTF_IMPLEMENTATION
#define STB_SPRINTF_IMPLEMENTATION
	#include "stb/stb_sprintf.h"
#endif
#ifndef STB_LEAKCHECK_IMPLEMENTATION
#define STB_LEAKCHECK_IMPLEMENTATION
	#include "stb/stb_leakcheck.h"
#endif


#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>


#if defined(PLATFORM_WEB)
#include <emscripten.h>
#include <unistd.h>
#endif


#if defined(__EMSCRIPTEN__)
	#define elAPI 		EMSCRIPTEN_KEEPALIVE
	#define elEXPORT 	EMSCRIPTEN_KEEPALIVE
	#define elTHREAD 	static
	#define elGLOBAL  static
#else
	#define elAPI 		static
	#define elEXPORT 	__declspec(dllexport)
	#define elTHREAD 	static __declspec(thread)
	#define elGLOBAL  static
#endif


#if !defined(__cplusplus)
	#define elLITERAL(X) (X)
#else
	#define elLITERAL(X) X
#endif


#define elTOTEXT_(X) #X
#define elTOTEXT(X) elTOTEXT_(X)

#define elFUSE_(X,Y) X##Y
#define elFUSE(X,Y) elFUSE_(X,Y)


#if !defined(MAX)
	#define MAX(x,y) ((x) > (y) ? (x) : (y))
#endif
#if !defined(MIN)
	#define MIN(x,y) ((x) < (y) ? (x) : (y))
#endif


#if !defined(MEGABYTES)
	#define MEGABYTES(x) ((x)*1024llu*1024llu)
#endif
#if !defined(GIGABYTES)
	#define GIGABYTES(x) ((x)*1024llu*1024llu*1024llu)
#endif


/* todo: */
#if !defined(_WIN32)
	#define MAX_PATH 0xff
#endif


#if !defined(COUNTOF)
	#define COUNTOF(X) (sizeof(X)/sizeof(X[0]))
#endif




/* ---------------------------------
	Forward Declarations
--------------------------------- */
typedef struct elModule 	elModule;
typedef struct elFileState elFileState;
typedef struct elState 		elState;
typedef struct elObject 	elObject;
typedef struct elTable 		elTable;
typedef struct elString 	elString;
typedef struct elClosure 	elClosure;
typedef struct elValue     elValue;



/* ---------------------------------
	Basic Types
--------------------------------- */



typedef long long int 	   elInteger;
typedef signed int 		   elBool;
typedef double 			   elNumber;
typedef void 			     *elHandle;
typedef void 			     *elAddr;
/* todo: I didn't know enum forward
declarations were a MS specific
extension */
typedef int 					elError;
typedef unsigned int 	   elHashId;
typedef int 				   elRegId;
typedef int 				   elByteId;
typedef int 				   elSymbolId;

typedef int (* elCFunction)(elState *);

/* todo: eventually convert this to an offset,
but this is great for debugging... */
typedef char 		        *elFileline;


typedef struct elBinding {
	char *name;
	elCFunction fn;
} elBinding;


elAPI elInteger elf_get_integer(elState *R, elRegId x);
elAPI elNumber elf_get_number(elState *R, elRegId x);
elAPI elString *elf_get_string(elState *R, elRegId x);
elAPI char *elf_get_text(elState *R, elRegId x);
elAPI elObject *elf_get_object(elState *R, elRegId x);
elAPI elTable *elf_get_table(elState *R, elRegId x);
elAPI elHandle elf_get_handle(elState *R, elRegId x);
elAPI elClosure *elf_get_closure(elState *R, elRegId x);


elAPI elString *elf_add_new_string(elState *, char *c);
elAPI elObject *elf_add_new_object(elState *, elInteger tell);
elAPI elTable *elf_add_new_table(elState *);
elAPI elString *elf_add_new_lstring(elState *, elInteger len);


elAPI void elf_register_bindings(elState *, elTable *, elBinding *list, int num);


#include "src/array.h"


#define elHERE (elSourceInfo){__FILE__,__LINE__,__func__}

typedef struct elSourceInfo {
	char const *fileName;
	int lineNumber;
	char const *func;
	char const *lineStart;
	char const *fileStart;
} elSourceInfo;



/* Memory Stuff, Allocators */


typedef struct elAllocator elAllocator;
typedef elError (* elAllocFn)(elAllocator *allocator, int flags, elInteger oldSize, elInteger newSize, void **oldAndNewMemory, elSourceInfo loca);


typedef struct elAllocator {
	char const *label;
	elAllocFn fn;
} elAllocator;



elAPI void elf_dealloc_(elAllocator *allocator, void const *memory, elSourceInfo loca);
elAPI void *elf_realloc_(elAllocator *allocator, elInteger size, void *memory, elSourceInfo loca);
elAPI void *elf_alloc_(elAllocator *allocator, elInteger size, elSourceInfo loca);
elAPI void *elf_calloc_(elAllocator *allocator, elInteger size, elSourceInfo loca);


#define elf_dealloc(cator,mem) elf_dealloc_(cator,mem,elHERE)
#define elf_realloc(cator,sze,mem) elf_realloc_(cator,sze,mem,elHERE)
#define elf_alloc(cator,sze) elf_alloc_(cator,sze,elHERE)
#define elf_calloc(cator,sze) elf_calloc_(cator,sze,elHERE)



/* ---------------------------------
	Objects
--------------------------------- */



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
	elByteId  byte;
	short     tell;
} elObject;


/* first object tag must be OBJ, all other
objects come after it, same order as object
types... */
#define TAGLIST(_) \
_(NIL) _(NUM) _(INT) _(SYS) _(CFN) \
_(OBJ) _(CLS) _(STR) _(TAB) /* end */


#define TAGENUM(NAME) elFUSE(TAG_,NAME),
typedef enum { TAGLIST(TAGENUM) } elValueTag;
#undef TAGENUM


typedef struct elValue {
	elValueTag tag;
	union {
		elInteger x_int;
		elNumber  n,  x_num;
		elAddr    p,  x_ptr;
		elHandle  h,  x_sys;
		elClosure *f,*x_cls;
		elObject  *j,*x_obj;
		elTable   *t,*x_tab;
		elString  *s,*x_str;
		elCFunction c,x_cfn;
	};
} elValue;


/* function prototypes are also for files, since files
are functions, nvalues refers to number of closure
values */
typedef struct elFileProto {
	short arity;
	short nvalues;
	short nlocals;
	/* this memory is managed automatically,
	which means that loaded files (to be closures)
	have to be kept in memory or referenced by
	other closures, otherwise they get collected.
	*/
	elString     *name;
	elString *contents;
	int 	      nbytes;
	int 	       bytes;
	int       **protos;
	/* to keep parents alive, as you should
	after all they've done for you? */
	int         parent;
} elFileProto;



/* ---------------------------------
	Closure
--------------------------------- */



/* closures are both for files and functions,
a file is a function so I don't know why keep
saying files and functions... */
typedef struct elClosure {
	elObject       obj;
	elFileProto  proto;
	elValue  values[1];
} elClosure;


#include "src/string.h"
#include "src/table.h"


/* I don't think this is portable to CPP as is, the idea is to
use C's "cast to union types" for typechecking:
	https://gcc.gnu.org/onlinedocs/gcc/Cast-to-Union.html#Cast-to-a-Union-Type

	I suppose CPP people can use templates or something...
*/
#define UCAST(D,T) ( ((union { T _; }){D})._ )



#define elGETTOP(S)   ((S)->stack_ptr)
#define elSETTOP(S,X) (elGETTOP(S) = UCAST(X, elValue *))
#define elPUSH(S,X)   (* elGETTOP(S) ++ = (X))

#define elGETFRAME(S) ((S)->frame)


#define elNUM(thing) (elLITERAL(elValue){ TAG_NUM, ((union { elNumber _; float __; elInteger I; }){thing}).I })
#define elINT(thing) (elLITERAL(elValue){ TAG_INT, (elInteger) UCAST(thing, elInteger) })
#define elSYS(thing) (elLITERAL(elValue){ TAG_SYS, (elInteger) UCAST(thing, elHandle) })
#define elTAB(thing) (elLITERAL(elValue){ TAG_TAB, (elInteger) UCAST(thing, elTable *) })
#define elOBJ(thing) (elLITERAL(elValue){ TAG_OBJ, (elInteger) UCAST(thing, elObject *) })
#define elSTR(thing) (elLITERAL(elValue){ TAG_STR, (elInteger) UCAST(thing, elString *) })
#define elCLS(thing) (elLITERAL(elValue){ TAG_CLS, (elInteger) UCAST(thing, elClosure *) })
#define elCFN(thing) (elLITERAL(elValue){ TAG_CFN, (elInteger) UCAST(thing, elCFunction) })
#define elNIL() (elLITERAL(elValue){TAG_NIL})


#define elGETNARGS(S)   (elGETFRAME(S)->nargs-1)
#define elGETLOCAL(S,X) (elGETFRAME(S)->locals[X])
#define elGETTHIS(S)    (elGETLOCAL(S,0).x_obj)
#define elGETARG(S,X)   (elGETLOCAL(S,X+1))
#define elGETTAG(S,X)   (elGETARG(S,X).tag)


#define elPUSHNIL(S) elPUSH(S,elNIL())
#define elPUSHCLS(S,V) elPUSH(S,elCLS(V))
#define elPUSHOBJ(S,V) elPUSH(S,elOBJ(V))
#define elPUSHTAB(S,V) elPUSH(S,elTAB(V))
#define elPUSHINT(S,V) elPUSH(S,elINT(V))
#define elPUSHNUM(S,V) elPUSH(S,elNUM(V))
#define elPUSHSTR(S,V) elPUSH(S,elSTR(V))
#define elPUSHSYS(S,V) elPUSH(S,elSYS(V))


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



/* todo: these are all deprecated, they should instead return the closure
object, and you use that however you want... */
/* todo: add support for arguments */
elAPI int elf_parse_code3(elState *, elString *name, elRegId ry, int ny, elString *contents);
elAPI int elf_parse_expr3(elState *, elString *name, elRegId ry, int ny, elString *contents);

elAPI int elf_Sfloadcode(elState *R, elFileState *fs, elString *filename, int nargs, elString *contents);
elAPI int elf_parse_expr3_fs(elState *, elFileState *fs, elString *name, elRegId ry, int ny, elString *contents);


elAPI int elf_Sloadfile(elState *, elString *name, int nargs);
elAPI int elf_Sfloadfile(elState *, elFileState *fs, elString *name, int nargs);

elAPI int elf_call_function(elState *R, int nargs, int nregs);
elAPI int elf_run(elState *);

elInteger elf_trigger_collection_cycle(elState *R);
elInteger elf_unmark_objects(elState *R);
elInteger elf_mark_object(elObject *obj);
void *elf_new_object(elState *R, elGCTy type, elInteger length);



#define TAGENUM(NAME) elTOTEXT(NAME),
elGLOBAL char const *tag2s[] = {
	TAGLIST(TAGENUM)
};
#undef TAGENUM


#include "src/lerror.h"
#include "src/ldebug.h"
#include "src/lmem.h"
#include "src/elf-sys.h"
#include "src/llog.h"
#include "src/elf-obj.h"
#include "src/elf-chr.h"
#include "src/elf-aux.h"
#include "src/elf-byte.h"
// #include "src/elf-mod.h"
// #include "src/elf-api.h"




/* ---------------------------------------------------------------------
** Interpreter State, Runtime, Garbage Collection...
** ---------------------------------------------------------------------
*/



/*
** Symbols are mapped at load time, so the code generator
** references globals by index...
*/
typedef struct elModule {
	union { elTable *globals, /* todo: @DEPRECATED */ *g; };
	/* todo: rename */
	int *track;
	elBytecode *bytes;
	elByteId nbytes;
	char **lines;
	elFileProto *files;
	elTable *strings;
	union { elNumber *numbers,      /* @DEPRECATED */ *kn; };
	union { elInteger *integers,    /* @DEPRECATED */ *ki; };
	union { elFileProto *functions, /* @DEPRECATED */ *prototypes, *p; };
} elModule;


elSymbolId elf_get_global_symbol(elModule *md, elString *name);
elSymbolId elf_add_global_value(elModule *md, elString *name, elValue v);
elSymbolId elf_add_proto(elModule *md, elFileProto p);


typedef struct elf_delaylist elf_delaylist;
typedef struct elf_delaylist {
	elf_delaylist *n;
	elByteId j;
} elf_delaylist;


typedef struct elStackFrame elStackFrame;
typedef struct elStackFrame {
	elStackFrame   *caller;
	elClosure      *closure;
	elValue        *locals;
	int            nlocals;
	char			    nargs;
	char			    nregs;
	elByteId        origin;
	elf_delaylist * delay_list;
	elBool 			 logging;
} elStackFrame;

typedef enum elGCColor elGCPhase;

#define elGC_PHASE_HOLD GC_WHITE
#define elGC_PHASE_FREE GC_BLACK

typedef struct elCollector {
	elBool     paused;
	elGCPhase  phase;
	elInteger  memory_allocated;
	elInteger  memory_threshold;
	elObject **new_objects;
	elObject **objects;
	/* this changes dynamically based on
	object min threshold, it tends to
	be around there... */
	elInteger  object_trigger_threshold;
} elCollector;


#define FLAG_DEBUGGER 			(1 << 0)
#define FLAG_DEBUGGER_ONCALL 	(1 << 1)
#define FLAG_BYTETRACKING 		(1 << 2)
#define FLAG_BYTELOGGING 		(1 << 3)


typedef struct elState {
	elModule *M;
	elValue  *stack;
	int 		 stack_max;
	elValue  *stack_ptr;

	elStackFrame *frame;
	int          nframe;
	int           flags;

	struct {
		elTable *integer;
		elTable *number;
		elTable *string;
		elTable *table;
	} metatables;
	struct {
		elValue oncall;
		elValue ongc;
	} hooks;
	struct {
		elString *x,*y,*z,*w;
		elString *width,*height;
		elString *__add,*__sub,*__mul,*__div;
		elString *__add1,*__sub1,*__mul1,*__div1;
		elString *__getfield,*__setfield;
		elString *__hash;
	} cache;
	/* the current instruction */
	elByteId byte;
	union { elCollector collector, memory; };
} elState;

#include "src/file.h"

#if defined(_MSC_VER)
# if !defined(ELF_KEEPWARNINGS)
#  pragma warning(push)
# endif
# pragma warning(disable:4100)
# pragma warning(disable:4245)
# pragma warning(disable:4057)
# pragma warning(disable:4189)
# pragma warning(disable:4201)
# pragma warning(disable:4244)
# pragma warning(disable:4267)
# pragma warning(disable:4389)
# pragma warning(disable:4996)
#endif
/* both __clang__ and _MSC_VER can be defined
at the same time when using clang-cl */
#if defined(__clang__)
# if !defined(ELF_KEEPWARNINGS)
#  pragma clang diagnostic push
# endif
# pragma clang diagnostic ignored "-Wparentheses-equality"
# pragma clang diagnostic ignored "-Wnon-literal-null-conversion"
# pragma clang diagnostic ignored "-Wmissing-braces"
# pragma clang diagnostic ignored "-Wunused-variable"
# pragma clang diagnostic ignored "-Wmissing-braces"
# pragma clang diagnostic ignored "-Wunused-function"
# pragma clang diagnostic ignored "-Wmissing-field-initializers"
# pragma clang diagnostic ignored "-Wsign-compare"
# pragma clang diagnostic ignored "-Wpointer-sign"
# pragma clang diagnostic ignored "-Wunused-function"
#endif


#include "src/elf-sys.c"
#include "src/elf-mem.c"
#include "src/ldebug.c"
#include "src/llog.c"
#include "src/elf-obj.c"
#include "src/elf-mod.c"
#include "src/elf-chr.c"
#include "src/elf-aux.c"
#include "src/string.c"
#include "src/elf-tab.c"
#include "src/elf-lex.c"
#include "src/elf-node.c"
#include "src/emit.c"
#include "src/file.c"
#include "src/elf-api.c"


#if !defined(ELF_NOLIBS)
# include "src/elf-lib.c"
# include "src/elf-web.c"
# include "src/lcrtlib.c"
# include "src/socketslib.c"
#endif


#include "src/elf-run.c"


#if !defined(ELF_KEEPWARNINGS)
# if defined(_MSC_VER)
#  pragma warning(pop)
# if defined(__clang__)
#  pragma clang diagnostic pop
# endif
#endif

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

