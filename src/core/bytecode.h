//
// See Copyright Notice In elf.h
//


// todo: make 32 bits
typedef struct {
	short b_z,b_y,b_x,b_k;
} Bytec;


// _(EXIT_LOOP_JUMP,  XYZ      , "exit_loop_jump")
// _(LOOP_JUMP,       XYZ      , "loop_jump")
// _(TRACE,           XYZ      , "trace")
// _(TYPEGUARD,       XY       , "typeguard")

#define BCDEF(_)                                  \
_(HALT,            XXX      , "halt")             \
_(NOP,             XXX      , "nop")              \
_(J,               XXX      , "jump")             \
_(JZ,              XY       , "jz")               \
_(JNZ,             XY       , "jnz")              \
_(JE,              XY       , "je")               \
_(JNE,             XY       , "jne")              \
_(RET,             XY       , "ret")              \
_(LOADCVAL,        XY       , "getupval")         \
_(LOADGLOBAL,      XY       , "getglobal")        \
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
_(I2N,             XY       , "i2n")              \
_(N2I,             XY       , "n2i")              \
_(BIT_SHL,         XYZ      , "shl")              \
_(BIT_SHR,         XYZ      , "shr")              \
_(BIT_XOR,         XYZ      , "xor")              \
_(BIT_OR,          XYZ      , "or")               \
_(BIT_AND,         XYZ      , "and")              \
_(BIT_NOT,         XYZ      , "not")              \
/* end */

#define BCITEM(NAME,MODE,SYM) XFUSE(BC_,NAME),
typedef enum ByteOP {
	BCDEF(BCITEM)

	BC_COUNT_,
} ByteOP;
#undef BCITEM


#define BC_XYZ(K,X,Y,Z) (Bytec){Z,Y,X,K}
#define BC_XYY(K,X,Y)    BC_XYZ(K,X,Y,0)
#define BC_XXX(K,X)      BC_XYZ(K,X,0,0)

#define BC_OP(B)        (B).b_k
#define BC_ARGX(B)      (B).b_x
#define BC_ARGY(B)      (B).b_y
#define BC_ARGZ(B)      (B).b_z
