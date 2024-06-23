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
b.y = number of inputs
b.z = number of outputs
-- function is located at b.x and arguments are located below
b.x, result is to be placed at b.x through b.y-1, so the user
should have allocated sufficient space for both the arguments
and the returns.

 counter pairs, must start at even value, ^1 to get opposite
BC_JZ, BC_JNZ,
BC_JE, BC_JNE,
 delays the execution of \i instructions until
procedure exits by jumping to the specified byte.
the byte address of the following instruction
gets saved into the delay list.
BC_DELAY (i),
 checks delay list, pops the last delay from it
if any and jumps to it, otherwise returns control
flow to the calling procedure
BC_LEAVE,
 copies z values starting at y to corresponding return
registers, jumps to x
BC_YIELD (x,y,z),
*/
#define BCLIST(_) \
	_(HALT, I) \
	_(J, XY) _(JZ, XY) _(JNZ, XY) _(JE, XY) _(JNE, XY) \
	_(DELAY, I) _(LEAVE, I) _(YIELD, I) \
	_(TYPEGUARD, XY) \
	_(LOADGLOBAL, XY) \
	_(LOADNUM, XY) _(LOADINT, XY) _(LOADNIL, I) \
	_(LOADTHIS, I) \
	_(RELOAD, XY) \
	_(CALL, XYZ) \
	_(METACALL, XYZ) \
	_(LOAD_CLOSURE_VALUE, XY) \
	_(INDEX, XYZ) \
	_(FIELD, XYZ) \
	_(METAFIELD, XYZ) \
	_(SETGLOBAL, XY) \
	_(SETINDEX, XYZ) \
	_(SETFIELD, XYZ) \
	_(TABLE, I) _(CLOSURE, XY) \
	_(ISNIL, XY) \
	_(EQ, XYZ) _(NEQ, XYZ) \
	_(LT, XYZ) _(LTEQ, XYZ) \
	_(MUL, XYZ) _(DIV, XYZ) _(MOD, XYZ) \
	_(ADD, XYZ) _(SUB, XYZ) \
	_(SHL, XYZ) _(SHR, XYZ) _(XOR, XYZ) _(BITOR, XYZ)

typedef enum elByteOP {

#define BCITEM(NAME,MODE) XFUSE(BC_,NAME),
	BCLIST(BCITEM)
#undef BCITEM

} elByteOP;


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
	switch (k) {
		case BC_JZ:
		case BC_JNZ:
		case BC_TYPEGUARD:
		case BC_SETGLOBAL:
		case BC_LOADNUM:
		case BC_LOADINT:
		case BC_LOADNIL:
		case BC_LOADGLOBAL:
		case BC_LOAD_CLOSURE_VALUE:
		case BC_RELOAD: {
			return BC_CLASS_XY;
		}
		case BC_CLOSURE:
		case BC_FIELD:
		// case BC_SETMETAFIELD:
		case BC_SETFIELD: case BC_SETINDEX:
		case BC_INDEX:
		case BC_ISNIL:
		case BC_NEQ: case BC_EQ:
		case BC_LT: case BC_LTEQ:
		case BC_ADD: case BC_SUB:
		case BC_DIV: case BC_MUL: case BC_MOD:
		case BC_SHL: case BC_SHR:
		case BC_XOR:
		case BC_CALL:
		case BC_YIELD:
		case BC_METAFIELD:
		case BC_METACALL: {
			return BC_CLASS_XYZ;
		}
		default: return BC_CLASS_I;
	}
}


char const *elf_get_byte_label(elByteOP k) {
	switch (k) {
		case BC_LOADNUM: return "loadnum";
		case BC_LOADINT: return "loadint";
		case BC_LOADNIL: return "loadnil";
		case BC_LEAVE: return "leave";
		case BC_YIELD: return "yield";
		case BC_J: return "j";
		case BC_JZ: return "jz";
		case BC_JNZ: return "jnz";
		case BC_ISNIL: return "isnil";
		case BC_CALL: return "call";
		case BC_LOADTHIS: return "this";
		case BC_METAFIELD: return "metafield";
		// case BC_SETMETAFIELD: return "setmetafield";
		case BC_METACALL: return "metacall";
		case BC_LOAD_CLOSURE_VALUE: return "loadcached";
		case BC_LOADGLOBAL: return "loadglobal";
		case BC_RELOAD: return "reload";
		case BC_SETGLOBAL: return "setglobal";
		case BC_CLOSURE: return "newclosure";
		case BC_TABLE: return "newtable";
		case BC_INDEX: return "getindex";
		case BC_FIELD: return "getfield";
		case BC_SETFIELD: return "setfield";
		case BC_SETINDEX: return "setindex";
		case BC_TYPEGUARD: return "typeguard";
		case BC_ADD: return "add";
		case BC_SUB: return "sub";
		case BC_DIV: return "dib";
		case BC_MUL: return "mul";
		case BC_MOD: return "mod";
		case BC_LT: return "lt";
		case BC_LTEQ: return "lteq";
		case BC_NEQ: return "neq";
		case BC_EQ: return "eq";
		case BC_DELAY: return "delay";
		case BC_SHL: return "shl";
		case BC_SHR: return "shr";
		case BC_XOR: return "xor";

		default: return "???";
	}
}

