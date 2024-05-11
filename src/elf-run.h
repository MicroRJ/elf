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
	elf_Object *obj;
	/* pointer to base stack address, the callee should
	yield starting at base[-1], should have base[-1..y)
	registers to write to. */
	union {
		elf_Value *base,*l,*locals;
	};
	/* todo: rename top to regress */
	elf_Value *top;
	/* next instruction index */
	elf_int j;
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
	elf_bool logging;
} elf_CallFrame;


typedef struct elf_Runtime {
	union { elf_Module *M, *md; };
	/* these should be safe to access
	multi-threaded */
	elf_Table *metatab_str;
	elf_Table *metatab_tab;
	struct {
		elf_String *__add,*__sub,*__mul,*__div;
		elf_String *__add1,*__sub1,*__mul1,*__div1;
		elf_String *__getfield,*__setfield;
	} cache;
} elf_Runtime;


/* todo: implement */
typedef struct lThread {
	union { elf_State *R, *rt; };
	union { elf_Module  *M, *md; };
	union { elf_CallFrame *call; };
	union { elf_Value *stk;      };
	elf_localid stklen;
	elf_int threadid;
	elf_byteid  curbyte;
} lThread;


typedef struct elf_State {
	union { elf_Module *M, *md; };
	union { elf_Value *stk,*s; };
	elf_localid stklen;
	union { elf_Value *top,*v; };
	union { elf_CallFrame *call,*frame,*f; };
	elf_bool debuggerflag;
	elf_Table *metatab_str;
	elf_Table *metatab_tab;
	struct {
		elf_String *x,*y,*z,*w;
		elf_String *width,*height;
		elf_String *__add,*__sub,*__mul,*__div;
		elf_String *__add1,*__sub1,*__mul1,*__div1;
		elf_String *__getfield,*__setfield;
	} cache;
	elf_Bytecode *bytetrace;
	elf_bool bytetracing;
	elf_bool bytetracking;
	/* current byte and whether bytelogging is on */
	elf_byteid byte;
	elf_bool bytelogging;
	elf_Object **gc;
	elf_bool     gcflags;
	elf_int      gcmemory;
	elf_int      gcthreshold;
} elf_State;


