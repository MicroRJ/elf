//
// See Copyright Notice In elf.h
//

#ifndef ELF_BYTECODE_H
#define ELF_BYTECODE_H

#include "elf.h"
#include "base.h"
#include "source.h"

// Type masks embedded in BC_CHECK_TYPE instructions.
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

/*
Instruction operands
  x, y, and z are signed 16-bit fields; w is an unsigned 8-bit field.
  r[n] denotes a register relative to the executing frame's register base.
  Register indices are nonnegative. The register base may differ from the argument base for variadic functions.
  Operands not listed for an instruction are unused and are initialized to zero by the construction macros.
  Execution normally continues at the next instruction. Jump offsets are measured in instructions from the current instruction.

Control flow
  BC_JUMP: x = signed relative offset. Continue at pc + x.
  BC_JUMP_IF_ZERO: x = signed relative offset, y = predicate register. Jump when the integer in r[y] is zero.
  BC_JUMP_IF_NONZERO: x = signed relative offset, y = predicate register. Jump when the integer in r[y] is nonzero.

Loads and register movement
  BC_LOAD_CAPTURE: x = destination register, y = capture index in the current closure. Copy capture[y] to r[x].
  BC_LOAD_NATIVE_FUNCTION: x = destination register, y = index in the state's native-function registry.
  BC_LOAD_CONSTANT_STRING: x = destination register, y = index in the prototype's runtime-string references.
  BC_LOAD_CONSTANT_NUMBER: x = destination register, y = index in the prototype's number constants.
  BC_LOAD_CONSTANT_INTEGER: x = destination register, y = index in the prototype's integer constants.
  BC_LOAD_NIL: x = destination register. Write nil to r[x].
  BC_MOVE: x = destination register, y = source register. Copy r[y] to r[x].
  BC_LOAD_CURRENT_CLOSURE: x = destination register. Load the executing closure; requires a closure-backed frame.

Allocation
  BC_NEW_TABLE: x = destination register. Allocate an empty table in r[x].
  BC_NEW_ARRAY: x = destination register. Allocate an empty array in r[x].
  BC_NEW_CLOSURE: x = destination and first capture-input register, y = prototype index in the state.
  Let n be the target prototype's capture count. Copy n captures beginning at r[x], then replace r[x] with the new closure.

Access and mutation
  BC_GET_FIELD: x = destination register, y = object register, z = key register. Read a table field into r[x].
  BC_GET_META_FIELD: x = destination register, y = receiver register, z = key register. Read the receiver's metatable entry into r[x].
  BC_GET_INDEX: x = destination register, y = object register, z = index/key register. Read an array, table, string, or buffer element.
  String and buffer indexing produces an integer byte. Array, string, and buffer indices are integers; negative indices count from the end.
  BC_SET_FIELD: x = object register, y = key register, z = value register. Assign r[z] to the table field named by r[y].
  BC_SET_INDEX: x = object register, y = index/key register, z = value register. Assign an array, table, or writable buffer element.
  Sequence indices follow BC_GET_INDEX. Buffer assignments take an integer byte in the range 0..255.
  BC_ARRAY_APPEND: x = array register, y = value register. Append r[y] to r[x].
  BC_MEASURE: x = destination register, y = input register. Produce an integer count in r[x].
  The count is the table entry count, array length, string byte length, buffer byte size, or zero for nil.

Calls and returns
  BC_CALL_DIRECT: x = result base register, y = prototype index, z = argument count, w = requested result count.
  Call a prototype without captures directly, without materializing a closure.
  BC_CALL_DYNAMIC: x = result base register, y = callee register, z = argument count, w = requested result count.
  Call the closure or native function in r[y]. The callee register need not equal the result base.
  Both calls read z args beginning at r[x + 1], including the implicit receiver in r[x + 1].
  Explicit args begin at r[x + 2]. Results begin at r[x]; r[x] also reserves the callee/result slot in the call layout.
  BC_RETURN: x = first result register, y = produced result count. Finish the current frame.
  Copy the lesser of y and the requested result count to the caller's result base; fill missing requested results with nil.
  Excess produced results are discarded. x is ignored when no results are copied.

Type checks
  BC_CHECK_TYPE: x = value register, y = allowed-type bitmask formed from TRULE_* flags.
  Check that the type of r[x] belongs to the mask; the instruction does not convert or replace the value.

Arithmetic
  All arithmetic instructions use x = destination register, y = left input register, z = right input register.
  Numeric operands produce an integer when both inputs are integers, otherwise a number.
  BC_ADD adds numeric inputs. When the left input is a string, concatenate it with the formatted right input instead.
  BC_SUB subtracts the right input from the left input.
  BC_MUL multiplies the inputs.
  BC_DIV divides the left input by the right input. Integer division truncates toward zero.
  BC_MOD computes the remainder. Number operands use a quotient truncated toward zero.
  BC_POW raises the left input to the right input's power using pow; integer inputs cast the result to an integer.

Comparisons
  All comparisons use x = destination register, y/z = input regs, and produce integer zero or one.
  BC_LESS_THAN performs numeric less-than comparison.
  BC_LESS_THAN_EQ performs numeric less-than-or-equal comparison.
  BC_EQ compares numeric inputs numerically, strings by contents, and other matching types by their stored payload.
  Different nonnumeric types compare unequal.
  BC_NOT_EQ produces the inverse of BC_EQ.

Bitwise operations
  Binary bitwise instructions use x = destination register, y = left integer register, z = right integer register.
  BC_SHIFT_LEFT shifts the left input left by the right input's count.
  BC_SHIFT_RIGHT shifts the signed left input right by the right input's count.
  BC_BITWISE_XOR computes exclusive-or.
  BC_BITWISE_OR computes bitwise or.
  BC_BITWISE_AND computes bitwise and.
  BC_BITWISE_NOT: x = destination register, y = integer input register. Complement every bit of r[y].

*/
#define BC_XDEF(X)                                               \
	X(JUMP,                     "jump")                            \
	X(JUMP_IF_ZERO,             "jump_if_zero")                    \
	X(JUMP_IF_NONZERO,          "jump_if_nonzero")                 \
	X(RETURN,                   "return")                          \
	X(LOAD_CAPTURE,             "load_capture")                    \
	X(LOAD_NATIVE_FUNCTION,     "load_native_function")            \
	X(LOAD_CONSTANT_STRING,     "load_constant_string")            \
	X(LOAD_CONSTANT_NUMBER,     "load_constant_number")            \
	X(LOAD_CONSTANT_INTEGER,    "load_constant_integer")           \
	X(LOAD_NIL,                 "load_nil")                        \
	X(MOVE,                     "move")                            \
	X(LOAD_CURRENT_CLOSURE,     "load_current_closure")            \
	X(GET_INDEX,                "get_index")                       \
	X(GET_META_FIELD,           "get_meta_field")                  \
	X(MEASURE,                  "measure")                         \
	X(GET_FIELD,                "get_field")                       \
	X(SET_FIELD,                "set_field")                       \
	X(SET_INDEX,                "set_index")                       \
	X(ARRAY_APPEND,             "array_append")                    \
	X(CALL_DYNAMIC,             "call_dynamic")                    \
	X(CALL_DIRECT,              "call_direct")                     \
	X(NEW_TABLE,                "new_table")                       \
	X(NEW_ARRAY,                "new_array")                       \
	X(NEW_CLOSURE,              "new_closure")                     \
	X(CHECK_TYPE,               "check_type")                      \
	X(LESS_THAN,                "less_than")                       \
	X(LESS_THAN_EQ,             "less_than_eq")                    \
	X(EQ,                       "eq")                              \
	X(NOT_EQ,                   "not_eq")                          \
	X(MUL,                      "mul")                             \
	X(DIV,                      "div")                             \
	X(ADD,                      "add")                             \
	X(SUB,                      "sub")                             \
	X(MOD,                      "mod")                             \
	X(POW,                      "pow")                             \
	X(SHIFT_LEFT,               "shift_left")                      \
	X(SHIFT_RIGHT,              "shift_right")                     \
	X(BITWISE_XOR,              "bitwise_xor")                     \
	X(BITWISE_OR,               "bitwise_or")                      \
	X(BITWISE_AND,              "bitwise_and")                     \
	X(BITWISE_NOT,              "bitwise_not")                     \
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
char *format_bytecode_function(elf_State *state, Arena *arena, u32 prototype_index);
void print_bytecode_function(elf_State *state, u32 prototype_index);

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

#define BC_XYZW(K,X,Y,Z,W) (Bytecode){.b_type = (K), .b_w = (u8)(W), .b_x = (X), .b_y = (Y), .b_z = (Z)}
#define BC_XYZ(K,X,Y,Z)     BC_XYZW(K,X,Y,Z,0)
#define BC_XY(K,X,Y)        BC_XYZ(K,X,Y,0)
#define BC_X(K,X)           BC_XYZ(K,X,0,0)

#define BC_TYPE(B)      (B).b_type
#define BC_ARGX(B)      (B).b_x
#define BC_ARGY(B)      (B).b_y
#define BC_ARGZ(B)      (B).b_z
#define BC_ARGW(B)      (B).b_w



#endif
