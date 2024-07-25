/*
** See Copyright Notice In elf.h
** elf-run.h
** Runtime
*/


typedef struct elf_delaylist elf_delaylist;
typedef struct elf_delaylist {
	elf_delaylist *n;
	elByteId j;
} elf_delaylist;


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
	elf_delaylist *delay_list;
	elBool logging;
} elStackFrame;


/* todo: implement */
typedef struct elThreadState {
	union { elState *R, *rt; };
	union { elModule  *M, *md; };
	union { elStackFrame *call; };
	union { elValue *stk;      };
	elRegId stklen;
	elInteger threadid;
	elByteId  curbyte;
} elThreadState;


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
	struct {
		elBool paused;
		union { elObject **objects, **articles; };
		elInteger allocated;
		elInteger threshold;
	} memory;
} elState;


