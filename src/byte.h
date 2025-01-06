/*
** See Copyright Notice In elf.h
** byte.h
*/


/* todo: make use only 32 bits */
#if 1
typedef unsigned long long int Bytecode;

#define BC_XYZ(K,X,Y,Z) (Bytecode){\
((((K) & 0xffffllu) <<(0x30))|\
 (((X) & 0xffffllu) <<(0x20))|\
 (((Y) & 0xffffllu) <<(0x10))|\
 (((Z) & 0xffffllu) <<(0x00)))}


#define BC_OP(B)   (short)(((B)>>(0x30))&0xffff)
#define BC_ARGX(B) (short)(((B)>>(0x20))&0xffff)
#define BC_ARGY(B) (short)(((B)>>(0x10))&0xffff)
#define BC_ARGZ(B) (short)(((B)>>(0x00))&0xffff)
#else

typedef struct Bytecode {
	short z;
	short y;
	short x;
	short k;
} Bytecode;

#define BC_XYZ(K,X,Y,Z) (Bytecode){Z,Y,X,K}

#define BC_OP(B)   (B).k
#define BC_ARGX(B) (B).x
#define BC_ARGY(B) (B).y
#define BC_ARGZ(B) (B).z
#endif

#define BC_XYY(K,X,Y) BC_XYZ(K,X,Y,0)
#define BC_XXX(K,X)   BC_XYZ(K,X,0,0)

typedef enum elByteClass {
	BC_CLASS_XXX,
	BC_CLASS_XY,
	BC_CLASS_XYZ,
} elByteClass;


#define BCDEF(_) \
_(HALT, XXX, "halt") \
_(NOP, XXX, "nop") \
_(J, XXX, "jump") \
_(JZ, XY, "jz") \
_(JNZ, XY, "jnz") \
_(JE, XY, "je") \
_(JNE, XY, "jne") \
_(LOOP, XXX, "loop") \
_(DELAY, XXX, "delay") \
_(LEAVE, XXX, "leave") \
_(YIELD, XYZ, "yield") \
_(TYPEGUARD, XY, "typeguard") \
_(GETCLOSED, XY, "getclosed") \
_(GETGLOBAL, XY, "getglobal")\
_(SETGLOBAL, XY, "setglobal")\
_(GETKNUM, XY, "getnum") \
_(GETKINT, XY, "getint") \
_(LOADNIL, XXX, "getnil") \
_(GETINDEX, XYZ, "getindex") _(SETINDEX, XYZ, "setindex")\
_(GETFIELD, XYZ, "getfield") _(SETFIELD, XYZ, "setfield")\
_(GETMETAFIELD, XYZ, "getmetafield") \
_(RELOAD, XY, "reload") \
_(CALL, XYZ, "call") \
_(TABLE, XXX, "new_table") \
_(CLOSURE, XY, "new_closure") \
_(ISNIL, XY, "is_nil") \
_(EQ, XYZ, "eq") _(NEQ, XYZ, "neq")   \
_(LT, XYZ, "lt") _(LTEQ, XYZ, "lteq")  \
_(MUL, XYZ, "mul") _(DIV, XYZ, "div")  \
_(ADD, XYZ, "add") _(SUB, XYZ, "sub") \
_(SHL, XYZ, "shl") _(SHR, XYZ, "shr") \
_(BIT_XOR, XYZ, "xor") _(BIT_OR, XYZ, "or") \
_(BIT_AND, XYZ, "and") \
_(MOD, XYZ, "mod") _(POW, XYZ, "pow") \
_(I2N,XY,"i2n") _(N2I,XY, "n2i") \
_(FLOAT2,XYZ,"float2")


#define BCITEM(NAME,MODE,SYM) XFUSE(BC_,NAME),
typedef enum ByteOP { BCDEF(BCITEM) } ByteOP;
#undef BCITEM





