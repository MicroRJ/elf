//
// See Copyright Notice In elf.h
//

typedef struct Ast Ast;
typedef Ast *AstRef;

#define AST_XDEF(_)                                       \
_(AST_NONE                , "none")                       \
\
_(AST_ERROR               , "error")                      \
\
_(AST_FILE                , "file")                       \
\
_(AST_IDENT               , "identifier")                 \
\
_(AST_AND                 , "and")                        \
_(AST_OR                  , "or")                         \
_(AST_ADD                 , "add")                        \
_(AST_SUB                 , "sub")                        \
_(AST_MUL                 , "mul")                        \
_(AST_DIV                 , "div")                        \
_(AST_POW                 , "pow")                        \
_(AST_MOD                 , "mod")                        \
_(AST_EQ                  , "eq")                         \
_(AST_NOT_EQ              , "not_eq")                     \
_(AST_LESS_THAN           , "less_than")                  \
_(AST_LESS_THAN_EQ        , "less_than_eq")               \
_(AST_GREATER_THAN        , "greater_than")               \
_(AST_GREATER_THAN_EQ     , "greater_than_eq")            \
_(AST_NIL_AND             , "nil_and")                    \
_(AST_NIL_OR              , "nil_or")                     \
_(AST_SHIFT_LEFT          , "shift_left")                 \
_(AST_SHIFT_RIGHT         , "shift_right")                \
_(AST_BITWISE_AND         , "bitwise_and")                \
_(AST_BITWISE_OR          , "bitwise_or")                 \
_(AST_BITWISE_XOR         , "bitwise_xor")                \
_(AST_BITWISE_NOT         , "bitwise_not")                \
\
_(AST_ADD_ASSIGN          , "add_assign")                 \
_(AST_SUB_ASSIGN          , "sub_assign")                 \
_(AST_MUL_ASSIGN          , "mul_assign")                 \
_(AST_DIV_ASSIGN          , "div_assign")                 \
_(AST_MOD_ASSIGN          , "mod_assign")                 \
_(AST_XOR_ASSIGN          , "xor_assign")                 \
_(AST_SHL_ASSIGN          , "shl_assign")                 \
_(AST_SHR_ASSIGN          , "shr_assign")                 \
_(AST_NIL_ASSIGN          , "nil_assign")                 \
_(AST_ASSIGN              , "assign")                     \
\
_(AST_BLOCK_STAT          , "block_stat")                 \
_(AST_DEFER_STAT          , "defer_stat")                 \
_(AST_DECL_STAT           , "stat_decl")                  \
\
_(AST_NUMBER_LITERAL      , "number_literal")             \
_(AST_INTEGER_LITERAL     , "integer_literal")            \
_(AST_STRING_LITERAL      , "atom_literal")             \
\
_(AST_CALL                , "call")                       \
_(AST_META_CALL           , "meta_call")                  \
\
_(AST_TUPLE               , "tuple")                      \
\
_(AST_FUNCTION            , "function")                   \
_(AST_FUNCTION_PARAM      , "function_param")             \
_(AST_RECURSE             , "recurse")                    \
\
_(AST_TABLE               , "table")                      \
_(AST_TABLE_ENTRY         , "table_entry")                \
\
_(AST_INDEX               , "index")                      \
_(AST_FIELD               , "field")                      \
_(AST_META_FIELD          , "meta_field")                 \
_(AST_NIL_LITERAL         , "nil_literal")                \
_(AST_IF                  , "if")                         \
_(AST_FOR                 , "for")                        \
\
_(AST_FOR_STEPS           , "for_steps")                  \
_(AST_COMMA_EXPR          , "comma_expr")                 \
\
_(AST_WHILE               , "while")                      \
_(AST_BREAK               , "break")                      \
_(AST_CONTINUE            , "continue")                   \
_(AST_RANGE               , "range")                      \
_(AST_RANGE_INDEX         , "range_index")                \
_(AST_ELLIPSIS            , "ellipsis")                   \
_(AST_RETURN              , "return")                     \
\
_(AST_LENGTH_INTRINSIC    , "length_intrinsic")           \
_(AST_GET_MEM             , "get_mem")                    \
/* end */

typedef enum
{
#define AST_XPAND(ENUM, NAME) ENUM,

	AST_XDEF(AST_XPAND)

#undef AST_XPAND
	AST_COUNT_,
}
AstType;

static const char *ast_type_name(AstType type);

// Todo, we could use an AstArray structure since we have that pattern all over the place,
// something like:
typedef struct
{
	u32    nargs;
	AstRef *args;
}
AstArray;

typedef struct
{
	Arena *arena;
	u32        stack_size;
	u32        stack_index;
	AstRef    *stack;
}
AstContext;

struct Ast
{
	AstType  kind;
	SourceSite site;

	union
	{
		elf_Atom *atom;
		i64      integer_value;
		f64      number_value;

		struct
		{
			AstRef body;
		}
		file;

		struct
		{
			AstRef *params;
			u32    nparams;
			AstRef variadic;
			AstRef    body;
		}
		function;

		struct
		{
			AstRef name;
			AstRef type;
			AstRef expr;
		}
		param;

		struct
		{
			AstRef name;
			AstRef type;
			AstRef expr;
		}
		decl;

		AstRef unary;

		struct
		{
			AstRef x;
			AstRef y;
		}
		binary;

		struct
		{
			AstRef *args;
			u32    nargs;
		}
		tuple;

		struct
		{
			AstRef *args;
			u32    nargs;
		}
		for_steps;

		struct
		{
			AstRef  expr;
			AstRef *args;
			u32    nargs;
		}
		call;

		struct
		{
			AstRef *args;
			u32    nargs;
		}
		table;

		struct
		{
			AstRef key;
			AstRef value;
		}
		table_entry;

		struct
		{
			AstRef        pred;
			AstRef true_clause;
			AstRef else_clause;
			AstRef then_clause;
		}
		if_stat;

		struct
		{
			AstRef expr;
		}
		return_stat;

		struct
		{
			AstRef *stats;
			u32    nstats;
		}
		block;

		struct
		{
			AstRef body;
		}
		defer_stat;

		struct
		{
			AstRef pred;
			AstRef body;
		}
		while_stat;

		struct
		{
			AstRef decl;
			AstRef body;
		}
		for_stat;

		struct
		{
			AstRef expr;
		}
		continue_stat;

		struct
		{
			AstRef expr;
		}
		break_stat;
	};
};

static Ast ast_error_sentinel =
{
	.kind = AST_ERROR,
};

#define NULL_AST ((AstRef)0)
#define ERROR_AST (&ast_error_sentinel)

static inline b32 ast_is_error(AstRef ast)
{
	return ast == ERROR_AST;
}

