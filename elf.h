/*
** See Copyright Notice Below.
** elf.h
** The λ elf language.
*/

#ifndef _elf_lang_
#define _elf_lang_


#if !defined(PLATFORM_DESKTOP) && !defined(PLATFORM_WEB)
	#error No Platform Defined
#endif


#ifndef STB_SPRINTF_IMPLEMENTATION
#define STB_SPRINTF_IMPLEMENTATION
	#include "stb/stb_sprintf.h"
#endif
#ifndef STB_LEAKCHECK_IMPLEMENTATION
#define STB_LEAKCHECK_IMPLEMENTATION
	#include "stb/stb_leakcheck.h"
#endif


#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include <string.h>


/* em knows where this is at */
#if defined(PLATFORM_WEB)
#include <emscripten.h>
#include <unistd.h>
#endif



/*
** Auxiliary Macros
*/

#if defined(__EMSCRIPTEN__)
	#define elAPI EMSCRIPTEN_KEEPALIVE
	#define elLIB EMSCRIPTEN_KEEPALIVE
	#define elf_threaddecl static
#else
	#define elAPI static
	#define elLIB __declspec(dllexport)
	#define elf_threaddecl static __declspec(thread)
#endif


#define elf_globaldecl static


#define LITC(xx) (xx)


#define XSTRINGIFY_(xx) #xx
#define XSTRINGIFY(xx) XSTRINGIFY_(xx)


#define XFUSE_(xx,yy) xx##yy
#define XFUSE(xx,yy) XFUSE_(xx,yy)


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


/* todo: why are these macros */
#define elGC_MEM_THRESHOLD_MIN (elInteger) MEGABYTES(4)
#define elGC_MEM_THRESHOLD_MAX (elInteger) MEGABYTES(512)

#define elGC_OBJ_THRESHOLD_MIN (elInteger) ((2048)*1)
#define elGC_OBJ_THRESHOLD_MAX (elInteger) ((2048)*2048)


/*
** Type Definitions
*/

typedef long long int 	   elInteger;
typedef signed int 		   elBool;
typedef double 			   elNumber;
typedef void 			     *elHandle;
typedef void 			     *elAddr;
typedef unsigned int 	   elHashId;
typedef int 				   elRegId;
typedef int 				   elByteId;
typedef int 				   elSymbolId;
typedef struct elModule 	elModule;
typedef struct elFileState elFileState;
typedef struct elState 		elState;
typedef struct elTable 		elTable;
typedef struct elString 	elString;
typedef struct elObject 	elObject;
typedef struct elClosure 	elClosure;
typedef int (* elBinding)(elState *);
/* todo: eventually convert this to an offset,
but this is great for debugging... */
typedef char 		        *elFileLine;

typedef struct elProto {
	short x,y;
	short zcache;
	short zstack;
	int 	nbytes;
	int 	bytes;
} elProto;


typedef enum elGCColor {
	GC_WHITE = 0, GC_BLACK, GC_PINK, GC_RED,
} elGCColor;


typedef enum elObjType {
	OBJ_NONE = 0, OBJ_CLOSURE, OBJ_STRING, OBJ_ARRAY, OBJ_TAB, OBJ_CUSTOM,
} elObjType;


typedef struct elObject {
	elObjType type;
	elGCColor color;
	elTable  *metatable;
	/* todo: please remove this... */
	short tell;
} elObject;

/* first object tag must be OBJECT, all other
objects come after it */
#define TAGLIST(_) \
_(NIL) _(GCD) _(SYS) \
_(INT) _(NUM) _(BID) \
_(OBJ) _(CLS) _(STR) _(TAB) /* end */


#define TAGENUM(NAME) XFUSE(TAG_,NAME),
typedef enum elValueTag {
	TAGLIST(TAGENUM)
} elValueTag;
#undef TAGENUM


#define TAGENUM(NAME) XSTRINGIFY(NAME),
elf_globaldecl char const *tag2s[] = {
	TAGLIST(TAGENUM)
};
#undef TAGENUM


typedef struct elValue {
	elValueTag tag;
	union {
		elAddr    p,  x_ptr;
		elHandle  h,  x_sys;
		elInteger i,  x_int;
		elNumber  n,  x_num;
		elClosure *f,*x_cls;
		elObject  *j,*x_obj;
		elTable   *t,*x_tab;
		elString  *s,*x_str;
		elBinding c;
	};
} elValue;


typedef struct elClosure {
	elObject obj;
	union { elProto prototype, fn; };

	elByteId  j;
	elValue enclosure[1];
} elClosure;


/*
** Main user API
*/

elAPI int elf_get_num_args(elState *R);
elAPI elObject *elf_get_this(elState *R);
elAPI elValueTag elf_get_tag(elState *R, elRegId x);
elAPI elInteger elf_get_integer(elState *R, elRegId x);
elAPI elNumber elf_get_number(elState *R, elRegId x);
elAPI elString *elf_get_string(elState *R, elRegId x);
elAPI elObject *elf_get_object(elState *R, elRegId x);
elAPI elTable *elf_get_table(elState *R, elRegId x);
elAPI elHandle elf_get_handle(elState *R, elRegId x);
elAPI elClosure *elf_get_closure(elState *R, elRegId x);
elAPI elValue elf_get_value(elState *R, elRegId x);


elAPI elValue elf_tab(elTable *);
elAPI elValue elf_binding_value(elBinding);
elAPI elValue elf_string_value(elString *);
elAPI elValue elf_closure_value(elClosure *);
elAPI elValue elf_integer_value(elInteger i);
elAPI elValue elf_number_value(elNumber n);
elAPI elValue elf_nil_value();


/*
** The following set of functions are very similar
** and have the same semantics, the only difference
** are the parameters.
** - filename: is the name of the file to load from
** disk or the label you wish to attach the code.
** - rxy: is the register from which to read inputs
** and to which to write outputs.
**
**   Loads elf [....] and calls its function.
** loadcodefs: [code]
** loadexprfs: [expr]
** loadfilefs: [file]
*/
elAPI int elf_loadcodefs(elState *, elFileState *fs, elString *filename, elRegId rxy, int ny, char *contents);
elAPI int elf_loadexprfs(elState *, elFileState *fs, elString *filename, elRegId rxy, int ny, char *contents);
elAPI int elf_loadfilefs(elState *, elFileState *fs, elString *filename, elRegId rxy, int ny);

elAPI int elf_loadcode(elState *, elString *filename, elRegId rxy, int ny, char *contents);
elAPI int elf_loadexpr(elState *, elString *filename, elRegId rxy, int ny, char *contents);
elAPI int elf_loadfile(elState *, elString *filename, elRegId rxy, int ny);


/*
** Ultimately, calls a function of any kind.
** Takes an optional object for meta calls,
** nx and ny are the number of in and out
** values respectively.
** nx does not include the function nor the
** optional object.
** rx is the frame register, the function
** should reside in that register at call
** time. arguments should come after that
** register.
** ry is the first yield register, where
** the results are written to.
** ry can be equal to rx.
*/
elAPI int elf_callex(elState *, elObject *obj, elRegId rx, elRegId ry, int nx, int ny);
/*
** Performs a root call, where rx and ry are the same
** and obj is nil.
*/
elAPI int elf_call_function(elState *, elRegId rx, int nx, int ny);
elAPI int elf_run(elState *);
elAPI void elf_checkcl(elState *c, elRegId x);
elAPI elValue *elf_get_stack_top(elState *R);
elAPI void elf_set_stack_top(elState *R, elValue *top);
elAPI elRegId elf_local_alloc(elState *R, int howmany);
elAPI elRegId elf_add_value(elState *, elValue v);
elAPI void eld_add_nil(elState *);
elAPI void elf_add_integer(elState *, elInteger i);
elAPI void elf_add_number(elState *, elNumber n);
elAPI void elf_pushsys(elState *c, elHandle h);
elAPI elString *elf_add_string(elState *, elString *s);
elAPI elString *elf_add_new_string(elState *, char *c);
elAPI elString *elf_pushnewstrlen(elState *, elInteger len);
elAPI elObject *elf_add_object(elState *, elObject *t);
elAPI elObject *elf_pushnewobj(elState *, elInteger tell);
elAPI elTable *elf_add_table(elState *, elTable *t);
elAPI elTable *elf_add_new_table(elState *R);
elAPI elRegId elf_add_closure(elState *, elClosure *f);
elAPI elRegId elf_pushnewcls(elState *, elProto fn);
elAPI elRegId elf_pushbinding(elState *, elBinding c);


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



/*
** Constant Macros
*/

#define false   ((elBool)(0))
#define elFalse ((elBool)(0))
#define elTrue  ((elBool)(1))
#define elNil   ((elAddr)(0))




#include "src/lerror.h"
#include "src/ldebug.h"
#include "src/lmem.h"
#include "src/elf-sys.h"
#include "src/llog.h"
#include "src/elf-obj.h"
#include "src/elf-chr.h"
#include "src/elf-aux.h"
#include "src/elf-byte.h"
#include "src/elf-mod.h"
#include "src/elf-api.h"


/* Core Structures */

typedef struct elDelaylist elDelaylist;
typedef struct elDelaylist {
	elDelaylist *n;
	elByteId j;
} elDelaylist;


typedef struct elStackFrame elStackFrame;
typedef struct elStackFrame {
	/* todo: this is only here so that we can write
	to caller->locals[rx/ry] directly, rx and ry
	could be relative to this.locals */
	elStackFrame *caller;
	/* 'head' is the first instruction (in the module)
	that initiated the call.
	This isn't limited to call instructions as the
	runtime may issue calls synthetically for
	things like operator overloading.
	'head' is mainly used for debugging, we store the
	byte directly as supposed to the line because this
	way we can find both the file and the line. */
	elByteId head;
	/* note that tail is local so it's head + tail to
	get the byte relative to the module */
	elByteId tail;
	/* the closure for this call frame */
	elClosure *cl;
	/* the object for meta fields, meta calls and the likes */
	elObject *obj;
	/* pointer to base stack address, the callee should
	yield starting at base[-1], should have base[-1..y)
	registers to write to. */
	union {
		elValue *base,*l,*locals;
	};
	/* todo: rename top to regress */
	elValue *top;
	elRegId rx,ry;
	/* x and y names are deprecated */
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
	union { int ny,y; };
	/* list of delayed jumps to be executed
	on return, 'finally' statements produce
	these. */
	elDelaylist *delay_list;
	elBool logging;
} elStackFrame;


typedef enum elGCPhase {
	elGC_PHASE_HOLD = GC_WHITE,
	elGC_PHASE_FREE = GC_BLACK,
} elGCPhase;


typedef struct elCollector {
	elBool     paused;
	elGCPhase  phase;
	elObject **new_objects;
	elObject **objects;
	elInteger  allocated;
	elInteger  threshold;
} elCollector;


typedef struct elState {
	union { elModule *M, /* DEPRECATED: */ *md; };
	union { elValue *stack, /* DEPRECATED: */ *stk,*s; };
	union { elRegId stack_length, /* DEPRECATED: */stklen; };
	union { elValue *stack_top,/* DEPRECATED: */*top,*v; };
	union { elStackFrame *stack_frame,/* DEPRECATED: */*call,*frame,*f; };
	elStackFrame root_call;
	int call_level;
	elBool debuggerflag;
	elBool oncalldebuggerflag;
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
		elTable *transient;
		elString *x,*y,*z,*w;
		elString *width,*height;
		elString *__add,*__sub,*__mul,*__div;
		elString *__add1,*__sub1,*__mul1,*__div1;
		elString *__getfield,*__setfield;
		elString *__hash;
	} cache;
	/* the current instruction */
	elByteId byte;
	elBool bytetracing;
	elBool bytetracking;
	elBool bytelogging;
	union { elCollector collector, memory; };
} elState;


/*
** Parsing And Code Generation...
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

#define TKITEM(NAME,_) XFUSE(TK_,NAME),
#define OPITEM(NAME,_,__) XFUSE(TK_,NAME),
#define MCITEM(NAME,_) XFUSE(TK_M_,NAME),

	KWLIST(TKITEM)
	MCLIST(MCITEM)
	TKLIST(TKITEM)
	OPLIST(OPITEM)

#undef TKITEM
#undef MCITEM
#undef OPITEM
} elTokenType;


elTokenType elf_is_word_or_macro(char *name) {
	#define MCITEM(NAME,SYM) if (S_eq(SYM,name)) return XFUSE(TK_M_,NAME);
	MCLIST(MCITEM)
	#undef MCITEM
	return TK_WORD;
}


elTokenType elf_is_word_or_keyword(char *name) {
	#define KWITEM(NAME,SYM) if (S_eq(SYM,name)) return XFUSE(TK_,NAME);
	KWLIST(KWITEM)
	#undef KWITEM
	return TK_WORD;
}


typedef struct ltokenintel {
	char *name;
	char prec;
} ltokenintel;


elf_globaldecl ltokenintel elf_tkintel[] = {
	{"none",-2},
#define TKITEM(_,SYM) {SYM,-2},
#define OPITEM(_,SYM,PRC) {SYM,PRC},
	KWLIST(TKITEM)
	MCLIST(TKITEM)
	TKLIST(TKITEM)
	OPLIST(OPITEM)
};


/*
** Nodes, a minimal intermediate language between
** source code and bytecode, makes it a bit easier
** to generate bytecode...
** Nodes can also represent pseudo instructions, which
** are "desugarized" in the parsing stage before being
** evaluated...
*/

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
	_(CLOSURE) _(STRING) _(TABLE)\
	_(INTEGER) _(NUMBER) _(NIL)\
	_(GLOBAL) _(LOCAL) _(CLOSURE_VALUE) _(FILE_VALUE)\
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
	/* this can be directly accessed
	using #array, #index, and #value */
	elRegId array_register;
	elRegId index_register;
	elRegId value_register;
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
	union { elModule *M,*md; };
	union { elState *R,*rt; };
	int file_id;
	char *filename;
	char *linechar;
	char *contents;
	char *thischar;
	int linenumber;
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


elNodeId elf_load_file_expr(elFileState *fs, elBool flags);
elNodeId elf_load_unary_expr(elFileState *fs, elBool allow_postfix, elBool flags);
void elf_load_file_stat(elFileState *fs);
void elf_emitter_emit_store(elFileState *fs, elFileLine line, elNodeId x, elNodeId y);
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
#include "src/lfunc.c"
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