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

/*
** Auxiliary Macros (all macros are auxilary...)
*/


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
	#define COUNTOF _countof
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
typedef char 		        *elFileLine;


typedef struct elBinding {
	char *name;
	elCFunction fn;
} elBinding;



/* ---------------------------------
	Common User API
--------------------------------- */



elAPI elInteger elf_get_integer(elState *R, elRegId x);
elAPI elNumber elf_get_number(elState *R, elRegId x);
elAPI elString *elf_get_string(elState *R, elRegId x);
elAPI char *elf_get_charstring(elState *R, elRegId x);
elAPI elObject *elf_get_object(elState *R, elRegId x);
elAPI elTable *elf_get_table(elState *R, elRegId x);
elAPI elHandle elf_get_handle(elState *R, elRegId x);
elAPI elClosure *elf_get_closure(elState *R, elRegId x);

elAPI elString *elf_add_new_string(elState *, char *c);
elAPI elObject *elf_add_new_object(elState *, elInteger tell);
elAPI elTable *elf_add_new_table(elState *);
elAPI elString *elf_pushnewstrlen(elState *, elInteger len);


elAPI void elf_register_bindings(elState *, elTable *, elBinding *list, int num);



/*
** Arrays are not exposed directly to elf, instead,
** we use them internally as a core data type,
** they work the same as STB's stretchy buffer.
**
**
** Usage is as follows:
**
** T *array = 0; // ensure it is initialized to 0
**
** ARRAY_ADD(array,T-thing)
**
** Take special care with array add, as it will
** evaluate the array twice, so if you plug
** in some very expensive expression, or something
** that is state sensitive, it could lead to
** hard to find bugs...
** Generally, I try to make it so that macros only evaluate
** things once, but unfortunately, I couldn't find a way to
** make it work the same for this one.
**
*/
typedef struct elArray {
	elInteger max;
	elInteger min;
   /* contents are allocated past this point */
} elArray;


#define ARRAY(var) ((elArray*)(var))[-1]
#define ARRAY_MAX(var) ((var != 0) ? ARRAY(var).max : 0)
#define ARRAY_MIN(var) ((var != 0) ? ARRAY(var).min : 0)
#define ARRAY_POP(var) ((var != 0) ? ARRAY(var).min -= 1 : 0)
#define ARRAY_LENGTH ARRAY_MIN



/* Some debug utilities */

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
		elInteger i,  x_int;
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


/*
** A file prototype describes a function,
** files themselves are also functions and
** are treated no differently.
** A function may have other functions within,
** we keep an additional array here, **protos,
** which stores the id's of each within the
** module.
** source and length are for storing primarily
** the contents of a file.
*/
typedef struct elFileProto {
	short x,y;
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



/* ---------------------------------
	String
--------------------------------- */



typedef struct elString {
	elObject    obj;
	elHashId   hash;
	/* Hear me out... do you even	use the length of
	the string that often, and when you do use it,
	you cache it somewhere, if you really want to
	compute the length of a string without using
	strlen (like when you're looking up a string),
	you can use the size of the object minus the
	size of the string header... */
	int     	length;
	union {
		char   contents[1];
		char   string[1];
		char   c[1];
	};
} elString;


elString *elf_new_lstring(elState *R, elInteger length);
elString *elf_new_string(elState *R, char *contents);


int elf_libS_length(elState *R);
int elf_libS_match(elState *R);
int elf_libS_pop(elState *R);
int elf_libS_append(elState *R);
int elf_libS_append_char(elState *R);
int elf_libS_get_hash(elState *R);
int elf_libS_uppercase(elState *R);
int elf_libS_lowercase(elState *R);
int elf_libS_split_by_lines(elState *R);
int elf_libS_get_index(elState *R);
int elf_libS_find(elState *R);
int elf_libS_split(elState *R);


elGLOBAL elBinding elf_libS_[] = {
	{"length",elf_libS_length},
	{"match",elf_libS_match},
	{"uppercase",elf_libS_uppercase},
	{"lowercase",elf_libS_lowercase},
	{"__add",elf_libS_append},
	{"__add1",elf_libS_append},
	{"append",elf_libS_append},
	{"append_char",elf_libS_append_char},
	{"pop",elf_libS_pop},
	{"get_hash",elf_libS_get_hash},
	{"split_by_lines",elf_libS_split_by_lines},
	{"idx",elf_libS_get_index},
	{"find",elf_libS_find},
};



/* ---------------------------------
	Table
--------------------------------- */



typedef struct elEntry {
	union { elValue key, k; };
	union { elInteger index, i; };
} elEntry;


typedef struct elTable {
	elObject obj;
	elInteger ntotal;
	elInteger nslots;
	elInteger ncollisions;
	union { elEntry *entries, /* @DEPRECATED */ *slots;};
	union { elValue *values, /* @DEPRECATED */ *array;};
} elTable;


elTable *elf_new_table_metatable(elState *);
elTable *elf_new_ltable(elState *, elInteger);
elTable *elf_new_table(elState *);


/* todo: all of these have to revised,
there are a bunch of inconsistencies and
incoherences with this API. */
int elf_libH_add(elState *);
int elf_libH_xadd(elState *);
int elf_libH_xremove(elState *);
int elf_libH_xdelete(elState *);
int elf_libH_index(elState *);
int elf_libH_tally(elState *);
int elf_libH_length(elState *);
int elf_libH_delete(elState *);
int elf_libH_itemize(elState *);
int elf_libH_inject(elState *);
int elf_libH_alias(elState *);
int elf_libH_contains(elState *);
int elf_libH_lookup(elState *);
int elf_libH_foreach(elState *);
int elf_libH_get_collisions(elState *);
int elf_libH_bubble_sort(elState *);
int elf_libH_find_aliases(elState *);
int elf_libH_array(elState *);
int elf_libH_xset(elState *);
int elf_libH_merge(elState *);
int elf_libH_xmerge(elState *);
int elf_libH_diff(elState *);
int elf_libH_xclone(elState *);
int elf_libH_reverse(elState *);
int elf_libH_clone(elState *);
int elf_libH_slice(elState *);
int elf_libH_swap(elState *);


elGLOBAL elBinding elf_libH_[] = {
	{"length",elf_libH_length},
	{"tally",elf_libH_tally},
	{"delete",elf_libH_delete},
	{"haskey",elf_libH_contains},
	{"lookup",elf_libH_lookup},
	{"foreach",elf_libH_foreach},
	{"collisions",elf_libH_get_collisions},
	{"add",elf_libH_add},
	{"xadd",elf_libH_xadd},
	{"itemize",elf_libH_itemize},
	{"inject",elf_libH_inject},
	{"idx",elf_libH_index},
	{"xrem",elf_libH_xremove},
	{"xdelete",elf_libH_xdelete},
	{"bubblesort",elf_libH_bubble_sort},
	{"fndaliases",elf_libH_find_aliases},
	{"alias",elf_libH_alias},
	{"merge",elf_libH_merge},
	{"xmerge",elf_libH_xmerge},
	{"reverse",elf_libH_reverse},
	{"clone",elf_libH_clone},
	{"xclone",elf_libH_xclone},
	{"slice",elf_libH_slice},
	{"xset",elf_libH_xset},
	{"swap",elf_libH_swap},
	{"diff",elf_libH_diff},
};


elInteger elf_table_tryS(elTable *tab, char *contents, elInteger length, elHashId hash);
void elf_check_table(elTable *table);
void elf_dealloc_table(elTable *);
void elf_table_add(elTable *table, elValue v);
elBool elf_table_set(elTable *table, elValue k, elValue v);
elInteger elf_table_lookup_index(elTable *table, elValue k);
elInteger elf_table_get_value_hash(elValue v);
elHashId elf_table_rehash(elHashId hash);
elHashId elf_tabhashstr(char *junk);
elHashId elf_tabhashptr(elAddr *ptr);
elBool elf_tabvaleq(elValue *x, elValue *y);
void elf_table_field_alias(elState *S, elTable *tab, char *key, elValue alias);
void elf_table_alias(elState *S, elTable *tab, elValue key, elValue alias);







/* This isn't portable to CPP, the idea is to
use C's "cast to union types" for typechecking:
	https://gcc.gnu.org/onlinedocs/gcc/Cast-to-Union.html#Cast-to-a-Union-Type
*/
#define UCAST(D,T) ( ((union { T _; }){D})._ )



#define elGETTOP(S)   ((S)->T)
#define elSETTOP(S,X) (elGETTOP(S) = UCAST(X, elValue *))
#define elPUSH(S,X)   (* elGETTOP(S) ++ = (X))

#define elGETFRAME(S) ((S)->F)
#define elGETTHIS(S) (elGETFRAME(S)->Q)
#define elGETNARGS(S) (elGETFRAME(S)->nx)

#define elNUM(thing) (elLITERAL(elValue){ TAG_NUM, ((union { elNumber _; float __; elInteger I; }){thing}).I })
#define elINT(thing) (elLITERAL(elValue){ TAG_INT, (elInteger) UCAST(thing, elInteger) })
#define elSYS(thing) (elLITERAL(elValue){ TAG_SYS, (elInteger) UCAST(thing, elHandle) })
#define elTAB(thing) (elLITERAL(elValue){ TAG_TAB, (elInteger) UCAST(thing, elTable *) })
#define elOBJ(thing) (elLITERAL(elValue){ TAG_OBJ, (elInteger) UCAST(thing, elObject *) })
#define elSTR(thing) (elLITERAL(elValue){ TAG_STR, (elInteger) UCAST(thing, elString *) })
#define elCLS(thing) (elLITERAL(elValue){ TAG_CLS, (elInteger) UCAST(thing, elClosure *) })
#define elCFN(thing) (elLITERAL(elValue){ TAG_CFN, (elInteger) UCAST(thing, elCFunction) })
#define elNIL() (elLITERAL(elValue){TAG_NIL})


#define elGETL(S,X) (elGETFRAME(S)->L[X])

#define elGETTAG(S,X) elGETL(S,X).tag


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
elAPI int elf_load_code3(elState *, elString *name, elRegId ry, int ny, elString *contents);
elAPI int elf_load_expr3(elState *, elString *name, elRegId ry, int ny, elString *contents);
elAPI int elf_load_file3(elState *, elString *name, elRegId ry, int ny);

elAPI int elf_load_code3_fs(elState *, elFileState *fs, elString *name, elRegId ry, int ny, elString *contents);
elAPI int elf_load_expr3_fs(elState *, elFileState *fs, elString *name, elRegId ry, int ny, elString *contents);
elAPI int elf_load_file3_fs(elState *, elFileState *fs, elString *name, elRegId ry, int ny);

int elf_call_function3(elState *R, elObject *obj, int nx, int ny, elRegId ry);

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


typedef struct elDelaylist elDelaylist;
typedef struct elDelaylist {
	elDelaylist *n;
	elByteId j;
} elDelaylist;


typedef struct elStackFrame elStackFrame;
typedef struct elStackFrame {
	elStackFrame *caller;
	/* the closure for this frame, if applicable */
	elClosure *C;
	/* context object, 'this' */
	elObject  *Q;
	/* the 'locals' or register where the locals
	for this frame start... */
	elValue   *L;

	/* this is the instruction (in the module)
	that initiated the call, mainly used for debugging. */
	elByteId   call_instr;
	elRegId    ry;

	/* the number of inputs (nx) and
	the number of expected outputs (ny).
	Output registers are allocated by the
	caller and runtime writes to them when
	the callee \yields.
	A function or binding can yield many more
	or less values, it does not matter.
	For bindings you must return the number
	of actual values yielded so that runtime
	can hoist the return values. */
	union { int nx,x; };
	union { int ny; };
	/* list of delayed jumps to be executed
	on return, 'finally' statements produce
	these. */
	elDelaylist *delay_list;
	elBool logging;
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
	/* This changes dynamically based on usage patterns,
	the idea is to collect more when it is worth collecting,
	and to refrain from collecting when previous culls
	weren't fortuitous. This is based on the assumption that
	it is better to make small and quick collections. However,
	there's very high likelihood that we won't be successful,
	so we remember that through this threshold */
	elInteger  object_trigger_threshold;
} elCollector;


#define FLAG_DEBUGGER 			(1 << 0)
#define FLAG_DEBUGGER_ONCALL 	(1 << 1)
#define FLAG_BYTETRACKING 		(1 << 2)
#define FLAG_BYTELOGGING 		(1 << 3)


typedef struct elState {
	/* The module, must be first member field */
	elModule *M;
	/* The stack */
	elValue  *K;
	/* The stack size */
	elRegId   Z;
	elValue  *T;
	/*
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

	union { elStackFrame *F; };
	int call_level;
	int flags;
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


/* ---------------------------------------------------------------------
** Parsing And Code Generation...
** ---------------------------------------------------------------------
*/


typedef struct elToken {
	unsigned char 	type;
	elFileLine 		line;
	/* whether this token was terminated by an
	end of line character */
	unsigned int 	 eol: 1;
	union {
		char 			*s;
		elInteger 	 i;
		elNumber 	 n;
	};
} elToken;


#define KWLIST(_) \
_(ELF,"elf") /* <- most likely to be removed */ \
_(ENUM,"enum") \
_(LOAD,"load") \
_(TRY,"try") _(CATCH,"catch") _(FINALLY,"finally") \
_(NEW,"new") _(THIS,"this") \
_(LET,"let") _(FOR,"for") _(FUN,"fun") \
_(DO,"do") _(WHILE,"while") \
_(BREAK,"break") _(CONTINUE,"continue") \
_(LASTLY,"lastly") _(LEAVE,"leave") \
_(IF,"if") _(IFF,"iff") _(ELSE,"else") _(ELIF,"elif") _(THEN,"then") \
_(NIL,"nil") _(TRUE,"true") _(FALSE,"false")


#define MCLIST(_) \
_(LINE_NUMBER,"line_number") _(LINE_CHAR,"line_char") _(FILE_NAME,"file_name") \
_(INT,"int") _(NUM,"num") \
_(LEVEL,"level") _(REGISTER,"register") \
_(INDEX,"index") _(VALUE,"value") _(ARRAY,"array") _(FIELD,"field") _(ENDOFFILE,"eof")


/* todo: why would !! and ?? have lower precedence
than relational operators, when !! and ?? work on
values */
#define OPLIST(_) \
_(POW,"**",12) \
_(MUL,"*",11) _(DIV,"/",11) _(MOD,"%",11) \
_(ADD,"+",10) _(SUB,"-",10) \
_(SHR,">>",9) _(SHL,"<<",9) \
_(LT,"<", 8) _(LTEQ,"<=", 8) \
_(GT,">", 8) _(GTEQ,">=", 8) \
_(EQ, "==", 7) _(NEQ , "!=", 7) \
_(BIT_AND,"&",6) _(BIT_OR,"|",5) _(BIT_XOR,"^",4) \
_(LOG_AND,"&&",3) _(LOG_OR,"||",2) \
_(NIL_AND,"!!",3) _(NIL_OR,"??",2) \
_(ELLIPSIS,"...",1) _(DOT_DOT,"..",1)


#define TKLIST(_) \
_(INTEGER,"integer") _(NUMBER,"number") _(STRING,"string") _(LETTER,"letter") _(WORD,"word") \
_(QUESTION_MARK,"?") _(EXCLAMATION_MARK,"!") \
_(ASSIGN,"=") _(NIL_ASSIGN,"?=") \
_(COLON,":") _(SEMI_COLON,";") \
_(COMMA,",") _(DOT,".") \
_(SQUARE_LEFT,"[") _(SQUARE_RIGHT,"]") _(CURLY_LEFT,"{") _(CURLY_RIGHT,"}") \
_(PAREN_LEFT,"(") _(PAREN_RIGHT,")") \


typedef enum elTokenType {
	TK_NONE = 0,

#define TKITEM(NAME,_) elFUSE(TK_,NAME),
#define OPITEM(NAME,_,__) elFUSE(TK_,NAME),
#define MCITEM(NAME,_) elFUSE(TK_M_,NAME),

	KWLIST(TKITEM)
	MCLIST(MCITEM)
	TKLIST(TKITEM)
	OPLIST(OPITEM)

#undef TKITEM
#undef MCITEM
#undef OPITEM
} elTokenType;


/*
** Nodes, a minimal intermediate language between
** source code and bytecode, makes it a bit easier
** to generate bytecode...
** Nodes can also represent pseudo instructions, which
** are "desugarized" in the parsing stage before being
** evaluated...
*/


#define SPECIAL_REGISTER_INDEX 0 // #index
#define SPECIAL_REGISTER_VALUE 1 // #value
#define SPECIAL_REGISTER_ARRAY 2 // #array

#define NO_NODE (-1)

typedef int elNodeId;

typedef struct {
	elNodeId id;
} elNodeIdTypeGuard;

#define MAKE_NODE_ID(id) (elNodeIdTypeGuard){id}

typedef enum elNodeTy {
	NT_NON = 0,
	NT_ANY, NT_SYS,
	NT_NIL, NT_BOL, NT_INT, NT_NUM,
	NT_OBJ, NT_TAB, NT_FUN, NT_STR
} elNodeTy;

// THIS: this
// INDEX: {x}[{x}]
// FIELD: {x}.{x}
// METAFIELD: {x}:{x}
// CALL: {x}({x})
// RANGE_INDEX: [{x}..{x}]
// RANGE: {x}..{x}
// GROUP: ({x})
#define NODE_DEF(_) \
_(NOP)\
	/* these come in pairs, use ^ to get the counter */\
_(AND) _(OR)\
_(NIL_AND) _(NIL_OR)\
_(EQ) _(NEQ)\
_(BIT_SHL) _(BIT_SHR)\
_(ADD) _(SUB) _(MUL) _(DIV)\
_(LT) _(GT) _(LTEQ) _(GTEQ)\
_(INDEX) _(FIELD)\
	/* end */\
_(BIT_AND) _(BIT_OR) _(BIT_XOR)\
_(MOD) _(POW)\
_(TYPEGUARD)\
_(LOAD)\
_(CLOSURE) _(STRING) _(TABLE) \
_(INTEGER) _(NUMBER) _(NIL) \
_(SPECIAL_REGISTER) \
_(GLOBAL) _(LOCAL) _(CLOSURE_VALUE) _(FILE_VALUE) \
_(THIS)\
_(MULTI)\
_(METAFIELD)\
_(CALL)\
_(RANGE_INDEX)\
_(RANGE)\
_(GROUP)\
_(REGION)


#define NODE_ENUM(NAME) NODE_##NAME,
typedef enum elNodeKi {
	NODE_NONE = 0,
	NODE_DEF(NODE_ENUM)
} elNodeKi;
#undef NODE_ENUM


char *elNodeToStr[] = {
	"NONE",
#define NODE_ENUM(NAME) #NAME,
	NODE_DEF(NODE_ENUM)
#undef NODE_ENUM
};


/* todo: make this more compact! */
typedef struct elNode {
	union { elNodeKi kind, ki, k; };
	union { elNodeTy type, ty, t; };
	elFileLine line;
	/* todo: eventually remove this */
	int level;

	union {
		struct { elNodeId x,y,*z; };
		union {
			char   *s;
			elInteger i;
			elNumber n;
		} lit;
	};
} elNode;


elNodeId elf_make_node_xyz(elFileState *fs, elFileLine, elNodeKi k, elNodeTy ty, elNodeId x, elNodeId y, elNodeId *z);
elNodeId elf_make_binary_node(elFileState *fs, elFileLine, elNodeKi k, elNodeTy ty, elNodeId x, elNodeId y);
elNodeId elf_make_node_unary(elFileState *fs, elFileLine, elNodeKi k, elNodeTy ty, elNodeId x);
elNodeId elf_make_node_nullary(elFileState *fs, elFileLine, elNodeKi k, elNodeTy t);
elNodeId elf_make_group_node(elFileState *fs, elFileLine, elNodeId x);
elNodeId elf_make_region_node(elFileState *fs, elFileLine, elNodeId x, elNodeId *z);
elNodeId elf_make_nil_node(elFileState *fs, elFileLine);
elNodeId elf_make_integer_node(elFileState *fs, elFileLine, elInteger i);
elNodeId elf_make_number_node(elFileState *fs, elFileLine, elNumber n);
elNodeId elf_make_string_node(elFileState *fs, elFileLine, char *);
elNodeId elf_make_table_node(elFileState *fs, elFileLine, elNodeId *z);
elNodeId elf_make_closure_node(elFileState *fs, elFileLine, elNodeId x, elNodeId *z);
elNodeId elf_make_load_node(elFileState *fs, elFileLine line, elNodeId x, elNodeId y);
elNodeId elf_make_register_node(elFileState *fs, elFileLine line, elNodeId i);
elNodeId elf_make_special_register_node(elFileState *fs, elFileLine line, elNodeId i);
elNodeId elf_make_global_value_node(elFileState *fs, elFileLine line, elNodeId i);
elNodeId elf_make_closure_value_node(elFileState *fs, elFileLine line, elNodeId i);
elNodeId elf_make_type_guard_node(elFileState *fs, elFileLine line, elNodeId x, elNodeTy y);
elNodeId elf_make_metafield_node(elFileState *fs, elFileLine line, elNodeId x, elNodeId y);
elNodeId elf_make_field_node(elFileState *fs, elFileLine line, elNodeId x, elNodeId y);
elNodeId elf_make_index_node(elFileState *fs, elFileLine line, elNodeId x, elNodeId y);
elNodeId elf_nodeloadfile(elFileState *fs, elFileLine line, elNodeId x);
elNodeId elf_make_ranged_index_node(elFileState *fs, elFileLine line, elNodeId x, elNodeId y);
elNodeId elf_make_call_node(elFileState *fs, elFileLine line, elNodeId x, elNodeId *z);


elValueTag elf_nodettotag(elNodeTy ty) {
	switch (ty) {
		case NT_SYS: return TAG_SYS;
		case NT_NUM: return TAG_NUM;
		case NT_INT: return TAG_INT;
		default: elNOCODE;
	}
	return TAG_NIL;
}



#define FILE_LHS     0x01
#define FILE_DESUGAR 0x02



/* Entity: high-level data structure used for
lexical scoping, binds a name to some
value or compile time thing. */

#define NO_ENTITY -1

typedef int elEntityId;
typedef int elBlockId;

typedef struct {
	elEntityId id;
} elEntityIdTypeGuard;

#define ENTITY_REFERENCED (1 << 0)
#define ENTITY_CONSTANT   (1 << 1)
#define ENTITY_ASSIGNED   (1 << 2)
#define ENTITY_PARAMETER  (1 << 3)
#define ENTITY_FORLOOP    (1 << 4)


typedef enum elEntityKind {
	ENTITY_INVALID = 0,
	ENTITY_DIRECTORY,
	ENTITY_LOCAL,
	ENTITY_GLOBAL,
} elEntityKind;

typedef struct elFileEntity {
	elEntityKind kind;
	elBool      flags;
	char        *name;
	elFileLine   line;
	// elNodeId     node;
	elRegId      slot;
	int         level;
} elFileEntity;


/* todo: remove this */
#define LOAD_RELOAD 		0x1
#define LOAD_ALLOCATE 	0x2

typedef struct elFileLoopState {
	elByteId entry;
	elByteId *false_jumps,*true_jumps;
	/* these can be directly accessed using #array, #index,
	and #value.
	#array and #index are guaranteed to be register nodes,
	and #value is an (index node), which translates to
	#array[#index] */
	elNodeId array_register; // _register;
	elNodeId index_register; // _register;
	elNodeId value_register;
	/* todo: why do we need this, please
	remove? */
	union { elNodeId x; };
} elFileLoopState;


#define BLOCK_LOOP 		0x01
#define BLOCK_ENDED  	0x02
#define BLOCK_DELAYED 	0x04


typedef struct elFileBlock elFileBlock;
typedef struct elFileBlock {
	elBool flags;
	int level;
	int xmemory;
	int xentity;
	int xnode;
	elByteId entry;
	elByteId jumpover;
	elByteId *leavejumps;
	elFileLoopState loop;
} elFileBlock;


/* Contains list of false and true jumps
generated by some boolean expression. */
typedef struct elFileBoolExpr {
	elByteId *t,*f;
} elFileBoolExpr;


/* Contains list of jumps generated
by a select statement, such as if else */
typedef struct elSelectState {
	/* Conditional false jump instructions
	to be patched so that they jump to
	the next block or instruction */
	elByteId *jz;
	/* list of exit jump instructions from
	each consecutive block to be patched */
	elByteId *j;
} elSelectState;


typedef struct elFileFnState elFileFnState;
typedef struct elFileFnState {
	elFileFnState *enclosing;
	elFileLine line;
	/* maximum number of local register used concurrently
	at any point for this function */
	elRegId nlocals;
	elRegId xmemory;
	/* array of entities from enclosing function,
	use for closure values... */
	elEntityId *enclosure;
	/* this is needed to emit instructions
	relative to the current function we're
	loading, there's always an active function,
	even at file level */
	/* index to first entity within entity list in file. */
	elEntityId entities;
	elBlockId entry_block;
	// elBlockId block;
	elByteId bytes;
	int nloops;
	int nyield;
	/* todo: deprecated */
	/* list of yield jumps to be patched */
	elByteId *yj;
} elFileFnState;


#define NO_SLOT (-1)
#define NO_BYTE (-1)
#define NO_JUMP (-0)
#define NO_LINE (-0)


typedef struct elFileState {
	elModule *M;
	elState  *R;
	/*
	these probably came from GCStrings,
	they should remain alive... I think. */
	char     *filename;
	char     *contents;
	char     *linechar;
	char     *thischar;
	int     linenumber;
	union {
		struct {
			elToken lasttk,tk,thentk;
		};
		struct {
			elToken last_token,this_token,then_token;
		};
	};
	elNode *nodes;
	elNodeId nnodes;
	elFileEntity *entities;
	elEntityId nentities;
	elFileBlock *blocks;
	union { elBlockId nblocks, level; };
	/* the current function */
	elFileFnState *fn;
	elByteId bytes;
	int flags;
	elBool debuggerflag;
} elFileState;

char *elf_get_file_name(elFileState *fs);

elNodeId elf_load_file_expr(elFileState *fs, elBool flags);
elNodeId elf_load_unary_expr(elFileState *fs, elBool allow_postfix, elBool flags);
void elf_load_file_stat(elFileState *fs);

void elf_emitter_emit_store(elFileState *fs, elFileLine line, elNodeId x, elNodeId y);
elFileBlock *elf_emitter_get_loop_block(elFileState *fs, elRegId with_value_register);
elRegId elf_emitter_local_load(elFileState *fs, elFileLine line, elBool reload, elRegId x, elRegId y, elNodeId id);
elRegId elf_emitter_localize(elFileState *fs, elFileLine line, elNodeId id);
elRegId elf_emitter_relocalize(elFileState *fs, elFileLine line, elRegId target_register, elNodeIdTypeGuard id);
void elf_emitter_enter_delayed_block(elFileState *fs, elFileLine line);
void elf_emitter_leave_delayed_block(elFileState *fs, elFileLine line);
elByteId elf_branch_if_false(elFileState *fs, elFileBoolExpr *js, elRegId x, elNodeId id);
elByteId elf_branch_if_true(elFileState *fs, elFileBoolExpr *js, elRegId x, elNodeId id);
elByteId *elf_emit_jump_if_true(elFileState *fs, elFileBoolExpr *js, elRegId x, elNodeId id);
elByteId *elf_emit_jump_if_false(elFileState *fs, elFileBoolExpr *js, elRegId x, elNodeId id);

// JZ
// JNZ
enum { L_IF  = 0, L_IFF = 1, };

void elf_emitter_begin_if(elFileState *fs, elFileLine line, elSelectState *s, elNodeId x, int z);
void elf_emitter_add_elif_clause(elFileState *fs, elFileLine line, elSelectState *s, elNodeId x);
void elf_emitter_add_else_clause(elFileState *fs, elFileLine line, elSelectState *s);
void elf_emitter_add_then_clause(elFileState *fs, elFileLine line, elSelectState *s);
void elf_emitter_close_if(elFileState *fs, elFileLine line, elSelectState *s);
void elf_emitter_begin_ranged_loop(elFileState *fs, elFileLine line, elNodeId x, elNodeId lo, elNodeId hi);
void elf_emitter_close_ranged_loop(elFileState *fs, elFileLine line);
void elf_emitter_begin_do_while_loop(elFileState *fs, elFileLine line);
void elf_emitter_close_do_while_loop(elFileState *fs, elFileLine line, elNodeId x);
void elf_emitter_begin_while_loop(elFileState *fs, elFileLine line, elNodeId x);
void elf_emitter_close_while_loop(elFileState *fs, elFileLine line);
elBlockId elf_emitter_begin_block(elFileState *fs, elBool flags);
void elf_emitter_close_block(elFileState *fs);


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




// #include "src/elf-tkn.h"
// #include "src/elf-run.h"
#include "src/elf-var.h"
#include "src/elf-str.h"
#include "src/elf-tab.h"
// #include "src/elf-node.h"
// #include "src/elf-file.h"


#include "src/elf-sys.c"
#include "src/elf-mem.c"
#include "src/ldebug.c"
#include "src/llog.c"
#include "src/elf-obj.c"
#include "src/elf-mod.c"
#include "src/elf-chr.c"
#include "src/elf-aux.c"
#include "src/elf-str.c"
#include "src/elf-var.c"
#include "src/elf-tab.c"
#include "src/elf-lex.c"
#include "src/elf-node.c"
#include "src/elf-emit.c"
#include "src/elf-file.c"
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

