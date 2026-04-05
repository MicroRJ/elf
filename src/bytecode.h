//
// See Copyright Notice In elf.h
//


// todo: make 32 bits
typedef struct
{
	i16 b_z, b_y, b_x, b_k;
}
Bytecode;

#define BYTECODE_XDEF(_)                          \
_(HALT,            XXX      , "halt")             \
_(NOP,             XXX      , "nop")              \
_(J,               XXX      , "jump")             \
_(JZ,              XY       , "jz")               \
_(JNZ,             XY       , "jnz")              \
_(JE,              XY       , "je")               \
_(JNE,             XY       , "jne")              \
_(RET,             XY       , "ret")              \
_(LOADCVAL,        XY       , "getupval")         \
_(GETGLOBAL,       XY       , "getglobal")        \
_(LOADKNUM,        XY       , "getnum")           \
_(LOADKINT,        XY       , "getint")           \
_(LOADNIL,         XXX      , "getnil")           \
_(RELOAD,          XY       , "reload")           \
_(GETINDEX,        XYZ      , "getindex")         \
_(GETMETAFIELD,    XYZ      , "getmetafield")     \
_(GETLENGTH,       XY       , "getlength")        \
_(GETFIELD,        XYZ      , "getfield")         \
_(SETGLOBAL,       XY       , "setglobal")        \
_(SETFIELD,        XYZ      , "setfield")         \
_(SETINDEX,        XYZ      , "setindex")         \
_(ARRAYADD,        XYZ      , "arrayadd")         \
_(CALL,            XYZ      , "call")             \
_(TABLE,           XXX      , "new_table")        \
_(CLOSURE,         XY       , "new_closure")      \
_(ENFORCE,         XY       , "enforce")          \
_(LT,              XYZ      , "lt")               \
_(LTEQ,            XYZ      , "lteq")             \
_(EQ,              XYZ      , "eq")               \
_(NEQ,             XYZ      , "neq")              \
_(MUL,             XYZ      , "mul")              \
_(DIV,             XYZ      , "div")              \
_(ADD,             XYZ      , "add")              \
_(SUB,             XYZ      , "sub")              \
_(MOD,             XYZ      , "mod")              \
_(POW,             XYZ      , "pow")              \
_(BIT_SHL,         XYZ      , "shl")              \
_(BIT_SHR,         XYZ      , "shr")              \
_(BIT_XOR,         XYZ      , "xor")              \
_(BIT_OR,          XYZ      , "or")               \
_(BIT_AND,         XYZ      , "and")              \
_(BIT_NOT,         XYZ      , "not")              \
_(I2N,             XY       , "i2n")              \
_(N2I,             XY       , "n2i")              \
/* end */

#define BYTECODE_XPAND(NAME,MODE,SYM) XFUSE(BYTECODE_,NAME),

typedef enum
{
	BYTECODE_XDEF(BYTECODE_XPAND)

	BYTECODE_COUNT_,
} ByteOP;

#undef BYTECODE_XPAND


#define BYTECODE_XYZ(K,X,Y,Z) (Bytecode){Z,Y,X,K}
#define BYTECODE_XYY(K,X,Y)    BYTECODE_XYZ(K,X,Y,0)
#define BYTECODE_XXX(K,X)      BYTECODE_XYZ(K,X,0,0)

#define BYTECODE_OP(B)        (B).b_k
#define BYTECODE_ARGX(B)      (B).b_x
#define BYTECODE_ARGY(B)      (B).b_y
#define BYTECODE_ARGZ(B)      (B).b_z
