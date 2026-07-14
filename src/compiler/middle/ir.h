//
// See Copyright Notice In elf.h
//


typedef struct
{
	i32 slot;
}
GenMemory;

#define NO_MEMORY ((GenMemory) { -1 })

static inline b32 gen_memory_is_valid(GenMemory memory)
{
	return memory.slot >= 0;
}

static inline i32 gen_memory_index(GenMemory memory)
{
	ASSERT(gen_memory_is_valid(memory));
	return memory.slot;
}


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
IRKind;

static const char *ir_kind_name(IRKind kind);

typedef struct IR_Node IR_Node;
typedef IR_Node *IR;


typedef struct
{
	IR *items;
	u32    count;
}
IR_Array;


struct IR_Node
{
	IRKind kind;
	SourceSite site;

	union
	{
		u32      ir_global;
		u32      ir_capture;
		u32      ir_function;
		u32      ir_label;

		elf_Atom *atom;
		i64      ir_int;
		f64      ir_num;

		IR ir_unary;

		struct
		{
			IR x;
			IR y;
		}
		ir_binary;

		struct
		{
			IR   expr;
			IR_Array args;
		}
		ir_call;

		struct
		{
			IR        pred;
			IR true_clause;
			IR else_clause;
		}
		ir_if;

		struct
		{
			// Todo, replace with *args, nargs
			IR expr;
		}
		ir_return;

		struct
		{
			IR_Array stats;
		}
		ir_block;

		struct
		{
			IR_Array stats;
			IR       value;
		}
		ir_expr_block;

		struct
		{
			IR pred;
			u32   label;
		}
		ir_jump_if_false;

		struct
		{
			IR expr;
		}
		ir_continue;

		struct
		{
			IR expr;
		}
		ir_break;

		struct
		{
			IR   expr;
			GenMemory slot;
		}
		ir_local;

		struct
		{
			IR local;
		}
		ir_load_local;
	};
};

static IR_Node ir_error_sentinel =
{
	.kind = IR_ERROR,
};

#define ERROR_IR (&ir_error_sentinel)

static inline b32 ir_is_error(IR ir)
{
	return ir == ERROR_IR;
}

