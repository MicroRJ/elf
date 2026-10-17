//
// See Copyright Notice In elf.h
//

#ifndef ELF_BYTECODE_H
#define ELF_BYTECODE_H

// Every bytecode function receives its implicit receiver in parameter slot zero.
#define IMPLICIT_PARAM_INDEX 0
#define IMPLICIT_PARAM_COUNT 1

#include "elf.h"
#include "base.h"
#include "source.h"

typedef struct BcFunctionRef BcFunctionRef;

// Type masks embedded in BC_ENFORCE instructions.
typedef enum
{
	TRULE_NONE     = 0,
	TRULE_NIL      = 1 << ELF_VALUE_TYPE_NIL,
	TRULE_NUMBER   = 1 << ELF_VALUE_TYPE_NUMBER,
	TRULE_INTEGER  = 1 << ELF_VALUE_TYPE_INTEGER,
	TRULE_FUNCTION = 1 << ELF_VALUE_TYPE_CFUNCTION,
	TRULE_USER     = 1 << ELF_VALUE_TYPE_USER_OBJECT,
	TRULE_CLOSURE  = 1 << ELF_VALUE_TYPE_CLOSURE,
	TRULE_TABLE    = 1 << ELF_VALUE_TYPE_TABLE,
	TRULE_STRING   = 1 << ELF_VALUE_TYPE_STRING,
	TRULE_ERROR    = 1 << ELF_VALUE_TYPE_ERROR,
	TRULE_BUFFER   = 1 << ELF_VALUE_TYPE_BUFFER,
	TRULE_ARRAY    = 1 << ELF_VALUE_TYPE_ARRAY,
	TRULE_ALL      = (1 << ELF_VALUE_TYPE_COUNT_) - 1,
	TRULE_NUMERIC  = TRULE_NUMBER | TRULE_INTEGER,
	TRULE_NONNIL   = TRULE_ALL & ~TRULE_NIL,
	TRULE_OBJECT   = (1 << ELF_VALUE_TYPE_USER_OBJECT) | (1 << ELF_VALUE_TYPE_CLOSURE) | (1 << ELF_VALUE_TYPE_TABLE) | (1 << ELF_VALUE_TYPE_ERROR) | (1 << ELF_VALUE_TYPE_BUFFER) | (1 << ELF_VALUE_TYPE_ARRAY),
	TRULE_CALLABLE = (1 << ELF_VALUE_TYPE_CFUNCTION) | (1 << ELF_VALUE_TYPE_CLOSURE),
}
TypeRule;

STATIC_ASSERT(TRULE_ALL <= 0x7fff);

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
	X(LOADKSTRING,     "getstring")        \
	X(LOADKNUM,        "getnum")           \
	X(LOADKINT,        "getint")           \
	X(LOADNIL,         "getnil")           \
	X(RELOAD,          "reload")           \
	X(CURRENT_CLOSURE, "current_closure")  \
	X(GETINDEX,        "getindex")         \
	X(GETMETAFIELD,    "getmetafield")     \
	X(MEASURE,         "measure")          \
	X(GETFIELD,        "getfield")         \
	X(SETGLOBAL,       "setglobal")        \
	X(SETFIELD,        "setfield")         \
	X(SETINDEX,        "setindex")         \
	X(ARRAYADD,        "arrayadd")         \
	X(DYNAMIC_CALL,    "dynamic_call")     \
	X(CALL,            "call")             \
	X(TABLE,           "new_table")        \
	X(ARRAY,           "new_array")        \
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
BcType;

const char *bytecode_type_name(BcType type);
char *format_bytecode_function(elf_State *state, elf_Arena *arena, BcFunctionRef function);
void print_bytecode_function(elf_State *state, BcFunctionRef function);

// One fixed-width VM instruction. The fourth operand uses padding that already
// existed after z, so adding it does not make an instruction any larger.
typedef struct
{
	BcType b_type;
	i16 b_x, b_y, b_z;
	u8 b_w;
}
Bytecode;

STATIC_ASSERT(sizeof(Bytecode) == 12);

typedef struct BcFunction
{
	b32       variadic;
	u8           arity;
	// TODO(RJ): we could literally instead just have i16 *captures, u8 ncaptures, here and the vm would
	// just load the values directly into the closure ...
	// we can put a limit on the number of captures, and if negative load from the parent's captures otherwise
	// from the stack
	u8        captures;
	u16     stack_size;
	u16         length;
	u32         offset;
}
BcFunction;

#define BC_XYZW(K,X,Y,Z,W) (Bytecode){.b_type = (K), .b_w = (u8)(W), .b_x = (X), .b_y = (Y), .b_z = (Z)}
#define BC_XYZ(K,X,Y,Z)     BC_XYZW(K,X,Y,Z,0)
#define BC_XYY(K,X,Y)       BC_XYZ(K,X,Y,0)
#define BC_XXX(K,X)         BC_XYZ(K,X,0,0)

#define BC_TYPE(B)      (B).b_type
#define BC_ARGX(B)      (B).b_x
#define BC_ARGY(B)      (B).b_y
#define BC_ARGZ(B)      (B).b_z
#define BC_ARGW(B)      (B).b_w



#endif
