//
// See Copyright Notice In elf.h
//


#define Y_NULL     (0)
// some internal error
#define Y_ERROR   ((AstRef) (-1))
// undeclared identifier
#define Y_UNIDENT ((AstRef) (-2))

#define istree(t) ((t) > 0)
#define notree(t) ((t) <= 0)
#define iserror(t) ((t) < 0)


typedef struct Ast Ast;
typedef Ast *AstRef;

#define AST_XDEF(_)                                       \
_(AST_NONE                , "none")                       \
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
_(AST_ERROR               , "error")                      \
\
_(AST_COMMA_EXPR          , "comma_expr")                 \
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
_(AST_ASSIGN         , "assign_stat")                \
\
_(AST_FILE                , "file")                       \
\
_(AST_BLOCK_STAT          , "block_stat")                 \
_(AST_DEFER_STAT          , "defer_stat")                 \
_(AST_DECL_STAT           , "stat_decl")                  \
\
_(AST_NUMBER_LITERAL      , "number_literal")             \
_(AST_INTEGER_LITERAL     , "integer_literal")            \
_(AST_STRING_LITERAL      , "string_literal")             \
\
_(AST_CALL                , "call")                       \
_(AST_META_CALL           , "meta_call")                  \
\
_(AST_TUPLE               , "tuple")                      \
\
_(AST_FUNCTION            , "function")                   \
_(AST_FUNCTION_PARAM      , "function_param")             \
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
_(AST_SEMI_COLON_EXPR     , "semi_colon_expr")            \
_(AST_WHILE               , "while")                      \
_(AST_BREAK               , "break")                      \
_(AST_CONTINUE            , "continue")                   \
_(AST_RANGE               , "range")                      \
_(AST_RANGE_INDEX         , "range_index")                \
_(AST_IDENT          , "identifier")                 \
_(AST_ELLIPSIS            , "ellipsis")                   \
_(AST_RETURN              , "return")                     \
\
_(AST_LENGTH_INTRINSIC    , "length_intrinsic")           \
\
\
\
\
_(EXPR_CLOSURE, "???")                   \
_(TREE_STORE, "???")                     \
_(TREE_GOTO, "???")                      \
\
_(TREE_GLOBAL, "???")                    \
_(TREE_UPVALUE, "???")                   \
_(TREE_ENFORCE, "???")                   \
_(TREE_DEBUG_GET_EXPRESSION_NAME, "???") \
_(TREE_DEBUG_GET_MEMORY, "???")          \
_(TREE_PROXY, "???")                     \
_(TREE_MEMORY, "???")                    \
/* end */

typedef enum
{
#define AST_XPAND(ENUM, NAME) ENUM,
	AST_XDEF(AST_XPAND)
#undef AST_XPAND
}
AstType;




struct Ast
{
	AstType  kind;
	Source   site;

	union
	{
		int      expr_global;
		int      expr_cvalue;
		char    *expr_str;
		i64      expr_int;
		f64      expr_num;
		// Todo, remove this!
		int      jump;
		TypeRule rule;

		char   *ast_ident_expr;

		struct
		{
			AstRef body;
		}
		ast_file;

		struct
		{
			AstRef *params;
			u32    nparams;
			AstRef    body;
		}
		ast_function;

		struct
		{
			AstRef name;
			AstRef type;
			AstRef expr;
		}
		ast_function_param;

		struct
		{
			AstRef name;
			AstRef type;
			AstRef expr;
		}
		ast_decl_stat;

		AstRef tree_unary_expr;

		struct
		{
			AstRef x;
			AstRef y;
		}
		ast_binary_expr;

		struct
		{
			AstRef *args;
			u32    nargs;
		}
		ast_tuple_expr;

		struct
		{
			AstRef  expr;
			AstRef *args;
			u32    nargs;
		}
		ast_call_expr;

		struct
		{
			AstRef *args;
			u32    nargs;
		}
		tree_table_expr;

		struct
		{
			AstRef key;
			AstRef value;
		}
		tree_table_entry;

		struct
		{
			AstRef        pred;
			AstRef true_clause;
			AstRef else_clause;
			AstRef then_clause;
		}
		tree_if_stat;

		struct
		{
			AstRef expr;
		}
		tree_return_stat;

		struct
		{
			AstRef *stats;
			u32    nstats;
		}
		ast_block_stat;

		struct
		{
			AstRef body;
		}
		tree_defer_stat;

		struct
		{
			AstRef pred;
			AstRef body;
		}
		tree_while_stat;

		struct
		{
			AstRef decl;
			AstRef body;
		}
		tree_for_stat;

		struct
		{
			AstRef expr;
		}
		tree_continue_stat;

		struct
		{
			AstRef expr;
		}
		tree_break_stat;
	};
};

