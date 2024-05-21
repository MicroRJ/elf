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


typedef struct elf_CallFrame elf_CallFrame;


typedef struct elf_CallFrame {
	/* todo: this is only here so that we can write
	to caller->locals[rx/ry] directly, rx and ry
	could be relative to this.locals */
	elf_CallFrame *caller;
	/* the closure this call frame belongs to */
	elf_Closure *cl;
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
	/* next instruction index */
	elInteger j;
	elf_localid rx,ry;
	/* x and y names are deprecated */
	/* -- The number of inputs (nx) and
	- the number of expected outputs (ny).
	- Output registers are allocated by the
	- caller and runtime writes to them when
	- the callee \yields.
	- A function or binding can yield many more
	- or less values, it does not matter.
	- For bindings you must return the number
	- of actual values yielded so that runtime
	- can hoist the return values. */
	union { int nx,x; };
	union { int ny,y; };
	/* list of delayed jumps to be executed
	on return, 'finally' statements produce
	these. */
	elf_delaylist *dl;
	elBool logging;
} elf_CallFrame;


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
	union { elf_CallFrame *call; };
	union { elValue *stk;      };
	elf_localid stklen;
	elInteger threadid;
	elf_byteid  curbyte;
} lThread;


typedef struct elState {
	union { elModule *M, *md; };
	union { elValue *stk,*s; };
	elf_localid stklen;
	union { elValue *top,*v; };
	union { elf_CallFrame *call,*frame,*f; };
	elBool debuggerflag;
	elTable *metatab_str;
	elTable *metatab_tab;
	struct {
		elString *x,*y,*z,*w;
		elString *width,*height;
		elString *__add,*__sub,*__mul,*__div;
		elString *__add1,*__sub1,*__mul1,*__div1;
		elString *__getfield,*__setfield;
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


