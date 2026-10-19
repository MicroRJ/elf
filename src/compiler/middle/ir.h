//
// See Copyright Notice In elf.h
//

#include "bytecode.h"

typedef struct Atom Atom;
typedef struct Ast_T Ast_T;
typedef struct Com_Source Com_Source;
typedef Ast_T *Ast;

#define NO_SLOT (-1)

/*
IR kinds are grouped by behavior:

Sentinels
  IR_NONE is an invalid or uninitialized kind and is never emitted.
  IR_ERROR preserves an already reported lowering failure.

Literal values
  IR_NIL, IR_INTEGER, and IR_NUMBER produce their corresponding literal values.
  IR_ATOM produces a runtime string from an interned compiler atom.

Named, local, and callable values
  IR_GLOBAL reads a symbolic global and may also be an IR_STORE destination.
  IR_LOAD_CAPTURE reads an IrCapture from the current closure.
  IR_RECURSE produces the current closure for a recursive call.
  IR_FUNCTION refers to an IrFunction and materializes a closure when used as a value.
  IR_LOAD reads an IR_LOCAL whose statement has already assigned its slot; it never allocates the slot itself.
  IR_GET_MEM produces that assigned stack-slot index and has the same allocation precondition as IR_LOAD.

Aggregate values and access
  IR_TABLE and IR_ARRAY allocate empty collections.
  IR_INDEX reads an indexed value and may also be an IR_STORE destination.
  IR_FIELD reads a field and preserves its receiver as the implicit argument when called.
  IR_META_FIELD does the same for a meta field.
  IR_MEASURE produces the measured length of its operand.

Short-circuit expressions
  IR_AND and IR_OR evaluate lazily and produce normalized boolean zero or one.
  IR_NIL_OR produces its left operand unless it is nil, then evaluates its right operand.

Arithmetic, comparison, and bitwise expressions
  IR_ADD, IR_SUB, IR_MUL, IR_DIV, IR_POW, and IR_MOD perform their named arithmetic operations.
  IR_EQ, IR_NOT_EQ, IR_LESS_THAN, and IR_LESS_THAN_EQ compare two operands.
  IR_SHIFT_LEFT, IR_SHIFT_RIGHT, IR_BITWISE_AND, IR_BITWISE_OR, and IR_BITWISE_XOR operate on two integers.
  IR_BITWISE_NOT computes the complement of one integer.

Calls and structured expressions
  IR_CALL calls an expression with arguments; the backend selects direct or dynamic dispatch.
  IR_EXPR_BLOCK executes a statement sequence and then produces its final value.

Statements and storage
  IR_LOCAL represents stable local storage. Parameters receive incoming slots during function setup; a standalone IR_LOCAL
  statement adopts its initializer's result slot or allocates a fresh slot when emitted.
  IR_MULTI_LOCAL is a statement that evaluates one expression for N results and assigns those consecutive slots to N locals.
  IR_STORE writes to an allocated local, global, field, or index; a local destination must already have an assigned slot.
  IR_ARRAY_ADD appends one value to an array.
  IR_BLOCK executes statements with a scoped local-slot lifetime.
  IR_IF executes one of two statement branches according to a predicate.
  IR_RETURN returns zero or one value from the current function.

Low-level control flow
  IR_LABEL defines a symbolic bytecode position.
  IR_JUMP transfers control unconditionally to an IR_LABEL.
  IR_JUMP_IF_FALSE transfers control to an IR_LABEL when its predicate is false.
*/
#define IR_DEFINITIONS(X)                                   \
	X(IR_NONE                , "none")                       \
	X(IR_ERROR               , "error")                      \
	X(IR_NIL                 , "nil")                        \
	X(IR_INTEGER             , "integer")                    \
	X(IR_NUMBER              , "number")                     \
	X(IR_ATOM                , "atom")                       \
	X(IR_GLOBAL              , "global")                     \
	X(IR_LOAD_CAPTURE        , "load_capture")               \
	X(IR_RECURSE             , "recurse")                    \
	X(IR_FUNCTION            , "function")                   \
	X(IR_LOAD                , "load")                       \
	X(IR_GET_MEM             , "get_mem")                    \
	X(IR_TABLE               , "table")                      \
	X(IR_ARRAY               , "array")                      \
	X(IR_INDEX               , "index")                      \
	X(IR_FIELD               , "field")                      \
	X(IR_META_FIELD          , "meta_field")                 \
	X(IR_MEASURE             , "measure")                    \
	X(IR_AND                 , "and")                        \
	X(IR_OR                  , "or")                         \
	X(IR_NIL_OR              , "nil_or")                     \
	X(IR_ADD                 , "add")                        \
	X(IR_SUB                 , "sub")                        \
	X(IR_MUL                 , "mul")                        \
	X(IR_DIV                 , "div")                        \
	X(IR_POW                 , "pow")                        \
	X(IR_MOD                 , "mod")                        \
	X(IR_EQ                  , "eq")                         \
	X(IR_NOT_EQ              , "not_eq")                     \
	X(IR_LESS_THAN           , "less_than")                  \
	X(IR_LESS_THAN_EQ        , "less_than_eq")               \
	X(IR_SHIFT_LEFT          , "shift_left")                 \
	X(IR_SHIFT_RIGHT         , "shift_right")                \
	X(IR_BITWISE_AND         , "bitwise_and")                \
	X(IR_BITWISE_OR          , "bitwise_or")                 \
	X(IR_BITWISE_XOR         , "bitwise_xor")                \
	X(IR_BITWISE_NOT         , "bitwise_not")                \
	X(IR_CALL                , "call")                       \
	X(IR_EXPR_BLOCK          , "expr_block")                 \
	X(IR_LOCAL               , "local")                      \
	X(IR_MULTI_LOCAL         , "multi_local")                \
	X(IR_STORE               , "store")                      \
	X(IR_ARRAY_ADD           , "array_add")                  \
	X(IR_BLOCK               , "block_stat")                 \
	X(IR_IF                  , "if")                         \
	X(IR_RETURN              , "return")                     \
	X(IR_LABEL               , "label")                      \
	X(IR_JUMP                , "jump")                       \
	X(IR_JUMP_IF_FALSE       , "jump_if_false")              \
/* end */

typedef enum
{

#define EXPAND(ENUM, NAME) ENUM,
	IR_DEFINITIONS(EXPAND)
#undef EXPAND

	IR_COUNT_,
}
IrKind;

static inline const char *ir_kind_name(IrKind kind)
{
	static const char *names[] =
	{
#define EXPAND(ENUM, NAME) #ENUM,
		IR_DEFINITIONS(EXPAND)
#undef EXPAND
	};

	if ((u32)kind >= IR_COUNT_) {
		return "IR_UNKNOWN";
	}
	return names[kind];
}

typedef struct IrNode IrNode;
typedef IrNode *Ir;

typedef struct
{
	Ir *items;
	u32 count;
}
IrArray;

typedef struct IrCapture IrCapture;
struct IrCapture
{
	IrCapture     *next;
	struct Entity *entity;
	Ir             target;
	u32            slot;
};

// Note, IrFunctions are allocated separately from regular IR nodes, an actual IR_FUNCTION refers
// to this structure directly ...
typedef struct IrFunction IrFunction;
struct IrFunction
{
	Ast         ast;
	IrFunction *next;
	Com_Source *source;
	SourceSite  site;
	b32         variadic;
	Ir          body;
	Ir         *params;
	u32         param_count;
	IrCapture  *first_capture;
	IrCapture  *last_capture;
	u32         capture_count;
	u32         index;
};

typedef struct IrModule IrModule;
struct IrModule
{
	Arena       *arena;
	IrFunction *first_function;
	IrFunction  *last_function;
	u32         function_count;
};

struct IrNode
{
	IrKind     kind;
	SourceSite site;
	TypeRule   type_rule;

	union
	{
		Atom    *ir_global;
		IrCapture *ir_load_capture;

		struct
		{
			b32 defined;
			u32 position;
		}
		ir_label;

		Atom *atom;
		i64   ir_int;
		f64   ir_num;

		Ir ir_unary;

		struct
		{
			Ir x;
			Ir y;
		}
		ir_binary;

		struct
		{
			Ir      expr;
			IrArray args;
		}
		ir_call;

		IrFunction *ir_function;

		struct
		{
			Ir        pred;
			Ir true_clause;
			Ir else_clause;
		}
		ir_if;

		struct
		{
			Ir expr;
		}
		ir_return;

		struct
		{
			IrArray stats;
		}
		ir_block;

		struct
		{
			IrArray stats;
			Ir       value;
		}
		ir_expr_block;

		struct
		{
			Ir pred;
			Ir label;
		}
		ir_jump_if_false;

		struct
		{
			Ir label;
		}
		ir_jump;

		struct
		{
			Ir expr;
		}
		ir_continue;

		struct
		{
			Ir expr;
		}
		ir_break;

		struct
		{
			Ir     expr;
			i32 slot;
		}
		ir_local;

		struct
		{
			Ir        expr;
			IrArray locals;
		}
		ir_multi_local;

		struct
		{
			Ir local;
		}
		ir_load;
	};
};


static IrNode ir_error_sentinel =
{
	.kind = IR_ERROR,
};

#define ERROR_IR (&ir_error_sentinel)

static inline b32 ir_is_error(Ir ir)
{
	return ir == ERROR_IR;
}

#define IR_FUNCTION_INDEX_INVALID ((u32)-1)


