//
// See Copyright Notice In elf.h
//


#define BCDEF(_)                                  \
_(HALT,            XXX      , "halt")             \
_(NOP,             XXX      , "nop")              \
_(J,               XXX      , "jump")             \
_(JZ,              XY       , "jz")               \
_(JNZ,             XY       , "jnz")              \
_(JE,              XY       , "je")               \
_(JNE,             XY       , "jne")              \
_(RET,             XY       , "ret")              \
_(TYPEGUARD,       XY       , "typeguard")        \
_(GETUPVAL,        XY       , "getupval")         \
_(GETGLOBAL,       XY       , "getglobal")        \
_(SETGLOBAL,       XY       , "setglobal")        \
_(GETKNUM,         XY       , "getnum")           \
_(GETKINT,         XY       , "getint")           \
_(LOADNIL,         XXX      , "getnil")           \
_(GETINDEX,        XYZ      , "getindex")         \
_(SETINDEX,        XYZ      , "setindex")         \
_(GETFIELD,        XYZ      , "getfield")         \
_(SETFIELD,        XYZ      , "setfield")         \
_(GETMETAFIELD,    XYZ      , "getmetafield")     \
_(RELOAD,          XY       , "reload")           \
_(CALL,            XYZ      , "call")             \
_(TABLE,           XXX      , "new_table")        \
_(CLOSURE,         XY       , "new_closure")      \
_(EQ,              XYZ      , "eq")               \
_(NEQ,             XYZ      , "neq")              \
_(LT,              XYZ      , "lt")               \
_(LTEQ,            XYZ      , "lteq")             \
_(MUL,             XYZ      , "mul")              \
_(DIV,             XYZ      , "div")              \
_(ADD,             XYZ      , "add")              \
_(SUB,             XYZ      , "sub")              \
_(SHL,             XYZ      , "shl")              \
_(SHR,             XYZ      , "shr")              \
_(BIT_XOR,         XYZ      , "xor")              \
_(BIT_OR,          XYZ      , "or")               \
_(BIT_AND,         XYZ      , "and")              \
_(MOD,             XYZ      , "mod")              \
_(POW,             XYZ      , "pow")              \
_(I2N,             XY       , "i2n")              \
_(N2I,             XY       , "n2i")              \
_(FLOAT2,          XYZ      , "float2")           \
/* end */

#define BCITEM(NAME,MODE,SYM) XFUSE(BC_,NAME),
typedef enum ByteOP {
	BCDEF(BCITEM)
} ByteOP;
#undef BCITEM


#define BC_XYZ(K,X,Y,Z) (elf_Bytecode){Z,Y,X,K}
#define BC_OP(B)        (B).k
#define BC_ARGX(B)      (B).x
#define BC_ARGY(B)      (B).y
#define BC_ARGZ(B)      (B).z
#define BC_XYY(K,X,Y)    BC_XYZ(K,X,Y,0)
#define BC_XXX(K,X)      BC_XYZ(K,X,0,0)


//
// todo: these metatables are static per translation unit, which is not very
// optimal
//


typedef enum elf_byteMode {
	BC_CLASS_XXX,
	BC_CLASS_XY,
	BC_CLASS_XYZ,
} elf_byteMode;

// @global
#define BCITEM(_,__,NAME) #NAME,
static char *byte2s[] = { BCDEF(BCITEM) };
#undef BCITEM


// todo: remove this!
static int get_byte_class(int k) {
#define BCITEM(NAME,FMT,__) case XFUSE(BC_,NAME): return XFUSE(BC_CLASS_,FMT);
	switch (k) {
		BCDEF(BCITEM)
	default: NO_CODE;
	}
#undef BCITEM
	return -1;
}


