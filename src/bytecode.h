//
// See Copyright Notice In elf.h
//


#define BYTECODE_XDEF(_)               \
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
#define XPAND(ENUM, NAME) BYTECODE_##ENUM,
	BYTECODE_XDEF(XPAND)
#undef XPAND
	BYTECODE_COUNT_,
}
BytecodeType;

// Todo,
typedef struct
{
	BytecodeType b_type;
	i16 b_x, b_y, b_z;
}
Bytecode;



#define BYTECODE_XYZ(K,X,Y,Z) (Bytecode){K, X, Y, Z}
#define BYTECODE_XYY(K,X,Y)    BYTECODE_XYZ(K,X,Y,0)
#define BYTECODE_XXX(K,X)      BYTECODE_XYZ(K,X,0,0)

#define BYTECODE_TYPE(B)      (B).b_type
#define BYTECODE_ARGX(B)      (B).b_x
#define BYTECODE_ARGY(B)      (B).b_y
#define BYTECODE_ARGZ(B)      (B).b_z
