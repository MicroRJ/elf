/*
** See Copyright Notice In elf.h
** elf-run.h
** Runtime
*/


typedef struct elf_delaylist elf_delaylist;
typedef struct elf_delaylist {
	elf_delaylist *n;
	elf_byteid j;
} elf_delaylist;


typedef struct elCallFrame elCallFrame;


typedef struct elCallFrame {
	/* todo: this is only here so that we can write
	to caller->locals[rx/ry] directly, rx and ry
	could be relative to this.locals */
	elCallFrame *caller;
	/* 'head' is the first instruction (in the module)
	that initiated the call.
	This isn't limited to call instructions as the
	runtime may issue calls synthetically for
	things like operator overloading.
	'head' is mainly used for debugging, we store the
	byte directly as supposed to the line because this
	way we can find both the file and the line. */
	elf_byteid head;
	/* note that tail is local so it's head + tail to
	get the byte relative to the module */
	elf_byteid tail;
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

	elf_localid rx,ry;
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
	elf_delaylist *dl;
	elBool logging;
} elCallFrame;


typedef struct elf_Runtime {
	union { elModule *M, *md; };
	/* these should be safe to access
	multi-threaded */
	elTable *metatab_str;
	elTable *metatab_tab;
	struct {
		elString *__add,*__sub,*__mul,*__div;
		elString *__add1,*__sub1,*__mul1,*__div1;
		elString *__getfield,*__setfield;
	} cache;
} elf_Runtime;


/* todo: implement */
typedef struct lThread {
	union { elState *R, *rt; };
	union { elModule  *M, *md; };
	union { elCallFrame *call; };
	union { elValue *stk;      };
	elf_localid stklen;
	elInteger threadid;
	elf_byteid  curbyte;
} lThread;


typedef struct elState {
	union { elModule *M, *md; };
	union { elValue *stk,*s; };
	elf_localid stklen;
	elCallFrame root_call;
	union { elValue *top,*v; };
	union { elCallFrame *call,*frame,*f; };
	int call_level;
	elBool debuggerflag;
	elTable *metatab_str;
	elTable *metatab_tab;
	struct {
		elString *x,*y,*z,*w;
		elString *width,*height;
		elString *__add,*__sub,*__mul,*__div;
		elString *__add1,*__sub1,*__mul1,*__div1;
		elString *__getfield,*__setfield;
		elString *__hash;
	} cache;
	elf_Bytecode *bytetrace;
	elBool bytetracing;
	elBool bytetracking;
	/* current byte and whether bytelogging is on */
	elf_byteid byte;
	elBool bytelogging;
	elObject **gc;
	elBool     gcflags;
	elInteger      gcmemory;
	elInteger      gcthreshold;
} elState;


