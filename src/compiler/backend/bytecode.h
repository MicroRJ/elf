//
// See Copyright Notice In elf.h
//

#include "base.h"


#define BC_XDEF(_)               \
_(HALT,            "halt")             \
_(NOP,             "nop")              \
_(JUMP,            "jump")             \
_(JZ,              "jz")               \
_(JNZ,             "jnz")              \
_(JE,              "je")               \
_(JNE,             "jne")              \
_(RETURN,          "return")           \
_(LOADCVAL,        "getupval")         \
_(GETGLOBAL,       "getglobal")        \
_(LOADKNUM,        "getnum")           \
_(LOADKINT,        "getint")           \
_(LOADNIL,         "getnil")           \
_(RELOAD,          "reload")           \
_(CURRENT_CLOSURE, "current_closure")  \
_(GETINDEX,        "getindex")         \
_(GETMETAFIELD,    "getmetafield")     \
_(GETLENGTH,       "getlength")        \
_(GETFIELD,        "getfield")         \
_(SETGLOBAL,       "setglobal")        \
_(SETFIELD,        "setfield")         \
_(SETINDEX,        "setindex")         \
_(ARRAYADD,        "arrayadd")         \
_(CALL,            "call")             \
_(TABLE,           "new_table")        \
_(CLOSURE,         "new_closure")      \
_(ENFORCE,         "enforce")          \
_(LT,              "lt")               \
_(LTEQ,            "lteq")             \
_(EQ,              "eq")               \
_(NEQ,             "neq")              \
_(MUL,             "mul")              \
_(DIV,             "div")              \
_(ADD,             "add")              \
_(SUB,             "sub")              \
_(MOD,             "mod")              \
_(POW,             "pow")              \
_(BIT_SHL,         "shl")              \
_(BIT_SHR,         "shr")              \
_(BIT_XOR,         "xor")              \
_(BIT_OR,          "or")               \
_(BIT_AND,         "and")              \
_(BIT_NOT,         "not")              \
_(I2N,             "i2n")              \
_(N2I,             "n2i")              \
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

// Todo,
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
