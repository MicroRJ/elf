//
// See Copyright Notice In elf.h
//

typedef struct
{
	i32 slot;
}
BcSlot;

#define NO_MEMORY ((BcSlot) { -1 })

#define IR_DEFINITIONS(X)                                   \
	X(IR_NONE                , "none")                       \
	X(IR_ERROR               , "error")                      \
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
	X(IR_STORE               , "store")                      \
	X(IR_ARRAY_ADD           , "array_add")                  \
	X(IR_IF                  , "if")                         \
	X(IR_BLOCK               , "block_stat")                 \
	X(IR_EXPR_BLOCK          , "expr_block")                 \
	X(IR_NUMBER              , "number")                     \
	X(IR_INTEGER             , "integer")                    \
	X(IR_ATOM                , "atom")                       \
	X(IR_NIL                 , "nil")                        \
	X(IR_CALL                , "call")                       \
	X(IR_META_CALL           , "meta_call")                  \
	X(IR_TABLE               , "table")                      \
	X(IR_INDEX               , "index")                      \
	X(IR_FIELD               , "field")                      \
	X(IR_META_FIELD          , "meta_field")                 \
	X(IR_LABEL               , "label")                      \
	X(IR_JUMP                , "jump")                       \
	X(IR_JUMP_IF_FALSE       , "jump_if_false")              \
	X(IR_RETURN              , "return")                     \
	X(IR_LENGTH_INTRINSIC    , "length_intrinsic")           \
	X(IR_GET_MEM             , "get_mem")                    \
	X(IR_FUNCTION            , "function")                   \
	X(IR_RECURSE             , "recurse")                    \
	X(IR_LOCAL               , "local")                      \
	X(IR_LOAD_LOCAL          , "load_local")                 \
	X(IR_LOAD_GLOBAL         , "load_global")                \
	X(IR_CAPTURE             , "capture")                    \
/* end */

typedef enum
{

#define EXPAND(ENUM, NAME) ENUM,
	IR_DEFINITIONS(EXPAND)
#undef EXPAND

	IR_COUNT_,
}
IrKind;

static const char *ir_kind_name(IrKind kind);

typedef struct IrNode IrNode;
typedef IrNode *Ir;

typedef struct
{
	Ir *items;
	u32 count;
}
IrArray;

struct IrNode
{
	IrKind kind;
	SourceSite site;
	union
	{
		u32      ir_global;
		u32      ir_capture;

		struct
		{
			u32 bytecode_label;
			b32 has_bytecode_label;
		}
		ir_label;

		Atom *atom;
		i64      ir_int;
		f64      ir_num;

		Ir ir_unary;

		struct
		{
			Ir x;
			Ir y;
		}
		ir_binary;

		struct
		{
			Ir   expr;
			IrArray args;
		}
		ir_call;

		struct
		{
			u32      index;
			IrArray captures;
		}
		ir_function;

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
			BcSlot slot;
		}
		ir_local;

		struct
		{
			Ir local;
		}
		ir_load_local;
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

