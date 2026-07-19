//
// See Copyright Notice In elf.h
//


typedef struct
{
	i32 slot;
}
BcSlot;

#define NO_MEMORY ((BcSlot) { -1 })


#define IR_XDEF(_)                                       \
_(IR_NONE                , "none")                       \
\
_(IR_ERROR               , "error")                      \
\
_(IR_AND                 , "and")                        \
_(IR_OR                  , "or")                         \
_(IR_NIL_OR              , "nil_or")                     \
_(IR_ADD                 , "add")                        \
_(IR_SUB                 , "sub")                        \
_(IR_MUL                 , "mul")                        \
_(IR_DIV                 , "div")                        \
_(IR_POW                 , "pow")                        \
_(IR_MOD                 , "mod")                        \
_(IR_EQ                  , "eq")                         \
_(IR_NOT_EQ              , "not_eq")                     \
_(IR_LESS_THAN           , "less_than")                  \
_(IR_LESS_THAN_EQ        , "less_than_eq")               \
_(IR_SHIFT_LEFT          , "shift_left")                 \
_(IR_SHIFT_RIGHT         , "shift_right")                \
_(IR_BITWISE_AND         , "bitwise_and")                \
_(IR_BITWISE_OR          , "bitwise_or")                 \
_(IR_BITWISE_XOR         , "bitwise_xor")                \
_(IR_BITWISE_NOT         , "bitwise_not")                \
\
_(IR_STORE               , "store")                      \
_(IR_ARRAY_ADD           , "array_add")                  \
\
_(IR_IF                  , "if")                         \
\
_(IR_BLOCK               , "block_stat")                 \
_(IR_EXPR_BLOCK          , "expr_block")                 \
\
_(IR_NUMBER              , "number")                     \
_(IR_INTEGER             , "integer")                    \
_(IR_ATOM                , "atom")                       \
_(IR_NIL                 , "nil")                        \
\
_(IR_CALL                , "call")                       \
_(IR_META_CALL           , "meta_call")                  \
\
_(IR_TABLE               , "table")                      \
\
_(IR_INDEX               , "index")                      \
_(IR_FIELD               , "field")                      \
_(IR_META_FIELD          , "meta_field")                 \
_(IR_LABEL               , "label")                      \
_(IR_JUMP                , "jump")                       \
_(IR_JUMP_IF_FALSE       , "jump_if_false")              \
\
_(IR_RETURN              , "return")                     \
\
_(IR_LENGTH_INTRINSIC    , "length_intrinsic")           \
_(IR_GET_MEM             , "get_mem")                    \
\
_(IR_FUNCTION            , "function")                   \
_(IR_RECURSE             , "recurse")                    \
\
_(IR_LOCAL               , "local")                      \
_(IR_LOAD_LOCAL          , "load_local")                 \
_(IR_LOAD_GLOBAL         , "load_global")                \
_(IR_CAPTURE             , "capture")                    \
/* end */

typedef enum
{
#define IR_XPAND(ENUM, NAME) ENUM,

	IR_XDEF(IR_XPAND)

#undef IR_XPAND
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

		elf_String *atom;
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
			// Todo, replace with *args, nargs
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
			Ir   expr;
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

