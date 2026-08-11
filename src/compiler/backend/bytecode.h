//
// See Copyright Notice In elf.h
//

#include "base.h"

#define BC_XDEF(X)                        \
	X(HALT,            "halt")             \
	X(NOP,             "nop")              \
	X(JUMP,            "jump")             \
	X(JZ,              "jz")               \
	X(JNZ,             "jnz")              \
	X(JE,              "je")               \
	X(JNE,             "jne")              \
	X(RETURN,          "return")           \
	X(LOADCVAL,        "getupval")         \
	X(GETGLOBAL,       "getglobal")        \
	X(LOADKATOM,       "getatom")          \
	X(LOADKNUM,        "getnum")           \
	X(LOADKINT,        "getint")           \
	X(LOADNIL,         "getnil")           \
	X(RELOAD,          "reload")           \
	X(CURRENT_CLOSURE, "current_closure")  \
	X(GETINDEX,        "getindex")         \
	X(GETMETAFIELD,    "getmetafield")     \
	X(GETLENGTH,       "getlength")        \
	X(GETFIELD,        "getfield")         \
	X(SETGLOBAL,       "setglobal")        \
	X(SETFIELD,        "setfield")         \
	X(SETINDEX,        "setindex")         \
	X(ARRAYADD,        "arrayadd")         \
	X(CALL,            "call")             \
	X(TABLE,           "new_table")        \
	X(CLOSURE,         "new_closure")      \
	X(ENFORCE,         "enforce")          \
	X(LT,              "lt")               \
	X(LTEQ,            "lteq")             \
	X(EQ,              "eq")               \
	X(NEQ,             "neq")              \
	X(MUL,             "mul")              \
	X(DIV,             "div")              \
	X(ADD,             "add")              \
	X(SUB,             "sub")              \
	X(MOD,             "mod")              \
	X(POW,             "pow")              \
	X(BIT_SHL,         "shl")              \
	X(BIT_SHR,         "shr")              \
	X(BIT_XOR,         "xor")              \
	X(BIT_OR,          "or")               \
	X(BIT_AND,         "and")              \
	X(BIT_NOT,         "not")              \
	X(I2N,             "i2n")              \
	X(N2I,             "n2i")              \
/* end */


typedef enum
{
#define XPAND(ENUM, NAME) BC_##ENUM,
	BC_XDEF(XPAND)
#undef XPAND
	BC_COUNT_,
}
BytecodeType;

const char *bytecode_type_name(BytecodeType type);

// TODO(RJ) improve the size of it!
typedef struct
{
	BytecodeType b_type;
	i16 b_x, b_y, b_z;
}
Bytecode;

#define BC_XYZ(K,X,Y,Z) (Bytecode){K, X, Y, Z}
#define BC_XYY(K,X,Y)    BC_XYZ(K,X,Y,0)
#define BC_XXX(K,X)      BC_XYZ(K,X,0,0)

#define BC_TYPE(B)      (B).b_type
#define BC_ARGX(B)      (B).b_x
#define BC_ARGY(B)      (B).b_y
#define BC_ARGZ(B)      (B).b_z
