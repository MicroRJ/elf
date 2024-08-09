/*
** See Copyright Notice In elf.h
** elf-byte.h
** Bytecodes
*/


typedef enum elByteClass {
	BC_CLASS_I,
	BC_CLASS_XY,
	BC_CLASS_XYZ,
} elByteClass;


/* BC_CALL(io x,y,z):
b.x = function address and first return value
b.y = number of arguments
b.z = number of results

-- function is located at b.x and arguments are located below
b.x, result is to be placed at b.x through b.y-1, so the user
should have allocated sufficient space for both the arguments
and the returns.

 counter pairs, must start at even value, ^1 to get opposite
BC_JZ, BC_JNZ,
BC_JE, BC_JNE,

BC_DELAY(i): postpones the execution of a portion of a
function's code for when it "leaves".

When the function attempts to "leave" by emitting a
"leave" instruction, it first checks the delay-queue,
if any, jumps to the specified delayed address.

Eeach delay block must jump back to the final "leave"
or emit a "leave" instruction directly.

This triggers the process once more for how ever many
delays there are queued.

If there are no more delays, the function finally
exits...

It is possible to have a leave within a delay block,
for instance:

	let foo = fun()
		lastly leave true
		if ... ?
			leave false

This makes it so that the default return value is true.

When "leave" is called within the delay block, it first
"yields" a result (true), then it jumps to the end of the
function (there's always a leave instruction emitted).
At which point it could be overwritten, in this case
it is only overwritten if the condition is met...




BC_LEAVE: Checks delay list, pops the last delay from it
if any and jumps to it, otherwise returns control
flow to the calling procedure
 copies z values starting at y to corresponding return
registers, jumps to x
BC_YIELD (x,y,z),
*/
/* todo: add support for different conditional jump
instructions, additionally a dedicated loop
instruction which increments a register would be
real nice */
#define BCLIST(_) \
_(HALT, I, "halt") \
_(J, XY, "jump") \
_(JZ, XY, "jz") \
_(JNZ, XY, "jnz") \
_(JE, XY, "je") \
_(JNE, XY, "jne") \
_(DELAY, I, "delay") \
_(LEAVE, I, "leave") \
_(YIELD, I, "yield") \
_(TYPEGUARD, XY, "typeguard") \
_(LOADGLOBAL, XY, "load_global") \
_(LOADNUM, XY, "load_num") \
_(LOADINT, XY, "load_int") \
_(LOADNIL, I, "load_nil") \
_(LOADCACHE, XY, "load_cache") \
_(INDEX, XYZ, "get_index") \
_(FIELD, XYZ, "get_field") \
_(METAFIELD, XYZ, "get_metafield") \
_(RELOAD, XY, "reload") \
_(SETGLOBAL, XY, "set_global") \
_(SETINDEX, XYZ, "set_index") \
_(SETFIELD, XYZ, "set_field") \
_(CALL, XYZ, "call") \
_(TABLE, I, "new_table") \
_(CLOSURE, XY, "new_closure") \
_(ISNIL, XY, "is_nil") \
_(EQ, XYZ, "eq") _(NEQ, XYZ, "neq")   \
_(LT, XYZ, "lt") _(LTEQ, XYZ, "lteq")  \
_(MUL, XYZ, "mul") _(DIV, XYZ, "div")  \
_(ADD, XYZ, "add") _(SUB, XYZ, "sub") \
_(SHL, XYZ, "shl") _(SHR, XYZ, "shr") \
_(BIT_XOR, XYZ, "xor") _(BIT_OR, XYZ, "or") \
_(BIT_AND, XYZ, "and") \
_(MOD, XYZ, "mod") _(POW, XYZ, "pow")

#define BCITEM(NAME,MODE,SYM) elFUSE(BC_,NAME),

typedef enum elByteOP {
	BCLIST(BCITEM)
} elByteOP;

#undef BCITEM


/* todo: make this much more compact! */
typedef struct elBytecode {
	elByteOP k;
	union {
		elInteger  i;
		struct {
			int x,y,z;
		};
	};
} elBytecode;


elByteClass elf_get_byte_class(elByteOP k) {
#define BCITEM(NAME,FMT,__) case elFUSE(BC_,NAME): return elFUSE(BC_CLASS_,FMT);
	switch (k) {
		BCLIST(BCITEM)
		default: elNOCODE;
	}
#undef BCITEM
	return -1;
}


char const *elf_get_byte_label(elByteOP k) {
#define BCITEM(NAME,_,SYM) case elFUSE(BC_,NAME): return SYM;
	switch (k) {
		BCLIST(BCITEM)
		default: elNOCODE;
	}
#undef BCITEM
	return 0;
}

