//
// See Copyright Notice In elf.h
//

#define AST_DEFINITIONS(X)                                   \
	X(AST_NONE                , "none")                       \
	X(AST_ERROR               , "error")                      \
	X(AST_IDENT               , "identifier")                 \
	X(AST_AND                 , "and")                        \
	X(AST_OR                  , "or")                         \
	X(AST_ADD                 , "add")                        \
	X(AST_SUB                 , "sub")                        \
	X(AST_MUL                 , "mul")                        \
	X(AST_DIV                 , "div")                        \
	X(AST_POW                 , "pow")                        \
	X(AST_MOD                 , "mod")                        \
	X(AST_EQ                  , "eq")                         \
	X(AST_NOT_EQ              , "not_eq")                     \
	X(AST_LESS_THAN           , "less_than")                  \
	X(AST_LESS_THAN_EQ        , "less_than_eq")               \
	X(AST_GREATER_THAN        , "greater_than")               \
	X(AST_GREATER_THAN_EQ     , "greater_than_eq")            \
	X(AST_NIL_AND             , "nil_and")                    \
	X(AST_NIL_OR              , "nil_or")                     \
	X(AST_SHIFT_LEFT          , "shift_left")                 \
	X(AST_SHIFT_RIGHT         , "shift_right")                \
	X(AST_BITWISE_AND         , "bitwise_and")                \
	X(AST_BITWISE_OR          , "bitwise_or")                 \
	X(AST_BITWISE_XOR         , "bitwise_xor")                \
	X(AST_BITWISE_NOT         , "bitwise_not")                \
	X(AST_ADD_ASSIGN          , "add_assign")                 \
	X(AST_SUB_ASSIGN          , "sub_assign")                 \
	X(AST_MUL_ASSIGN          , "mul_assign")                 \
	X(AST_DIV_ASSIGN          , "div_assign")                 \
	X(AST_MOD_ASSIGN          , "mod_assign")                 \
	X(AST_XOR_ASSIGN          , "xor_assign")                 \
	X(AST_SHL_ASSIGN          , "shl_assign")                 \
	X(AST_SHR_ASSIGN          , "shr_assign")                 \
	X(AST_NIL_ASSIGN          , "nil_assign")                 \
	X(AST_ASSIGN              , "assign")                     \
	X(AST_BLOCK_STAT          , "block_stat")                 \
	X(AST_DEFER_STAT          , "defer_stat")                 \
	X(AST_DECL_STAT           , "stat_decl")                  \
	X(AST_STATIC_DECL         , "static_decl")                \
	X(AST_NUMBER_LITERAL      , "number_literal")             \
	X(AST_INTEGER_LITERAL     , "integer_literal")            \
	X(AST_STRING_LITERAL      , "string_literal")             \
	X(AST_INTERPOLATED_STRING , "interpolated_string")        \
	X(AST_IMPORT              , "import")                     \
	X(AST_CALL                , "call")                       \
	X(AST_NAMED_ARGUMENT      , "named_argument")             \
	X(AST_TUPLE               , "tuple")                      \
	X(AST_FUNCTION            , "function")                   \
	X(AST_RECURSE             , "recurse")                    \
	X(AST_TABLE               , "table")                      \
	X(AST_ARRAY               , "array")                      \
	X(AST_TABLE_ENTRY         , "table_entry")                \
	X(AST_INDEX               , "index")                      \
	X(AST_FIELD               , "field")                      \
	X(AST_META_FIELD          , "meta_field")                 \
	X(AST_NIL_LITERAL         , "nil_literal")                \
	X(AST_IF                  , "if")                         \
	X(AST_FOR                 , "for")                        \
	X(AST_FOR_RANGE           , "for_range")                  \
	X(AST_COMMA_EXPR          , "comma_expr")                 \
	X(AST_WHILE               , "while")                      \
	X(AST_BREAK               , "break")                      \
	X(AST_CONTINUE            , "continue")                   \
	X(AST_RANGE               , "range")                      \
	X(AST_RANGE_INDEX         , "range_index")                \
	X(AST_ELLIPSIS            , "ellipsis")                   \
	X(AST_RETURN              , "return")                     \
	X(AST_MEASURE             , "measure")                    \
	X(AST_GET_MEM             , "get_mem")                    \
// END

typedef struct Ast_T Ast_T;
typedef Ast_T *Ast;
struct IrFunction;

typedef enum
{
#define EXPAND(ENUM, NAME) ENUM,
	AST_DEFINITIONS(EXPAND)
#undef EXPAND
	AST_COUNT_,
}
Ast_Kind;

typedef struct
{
	Ast *items;
	u32  count;
}
Ast_Array;

typedef struct Ast_Node Ast_Node;
struct Ast_Node
{
	Ast_Node *next;
	Ast       tree;
};

typedef struct
{
	Ast_Node *first;
	Ast_Node *last;
	u32       count;
}
Ast_List;

// IMMUTABLE once constructed.
struct Ast_T
{
	Ast_Kind             kind;
	SourceSite           site;

	// metadata
	struct Entity     *entity;
	u32            param_slot;

	// if this is an expression, whether it appears on the right hand side
	// of a static declaration
	b32             is_static;

	union
	{
		Com_Status               status;
		Atom                      *atom;
		Atom                *identifier;
		Ast_Array   interpolated_string;

		// TODO(RJ): have these store a value instead, that way we can go from:
		// token -> ast -> ir -> checking -> bytecode more easily ...
		Atom              *string_value;
		i64               integer_value;
		f64                number_value;

		struct
		{
			// Todo, path should be a tree as well ...
			Atom         *path;

			Com_Source *target;
		}
		import;

		struct
		{
			Ast                parent;
			Ast_Array          params;
			b32                variadic;
			Ast                body;
			struct IrFunction *ir;
		}
		function;

		struct
		{
			b32 constant;
			Ast name;
			Ast type;
			Ast expr;
		}
		decl;

		struct
		{
			b32 exported;
			Ast name;
			Ast expr;
		}
		static_decl;

		Ast unary;

		struct
		{
			Ast x;
			Ast y;
		}
		binary;

		Ast_Array tuple;

		struct
		{
			Ast       expr;
			Ast_Array args;
		}
		call;

		struct
		{
			Ast name;
			Ast expr;
		}
		named_argument;

		Ast_Array table;

		Ast_Array array;

		struct
		{
			Ast key;
			Ast value;
		}
		table_entry;

		struct
		{
			Ast        pred;
			Ast true_clause;
			Ast else_clause;
			Ast then_clause;
		}
		if_stat;

		struct
		{
			Ast expr;
		}
		return_stat;

		Ast_Array block;

		struct
		{
			Ast body;
		}
		defer_stat;

		struct
		{
			Ast pred;
			Ast body;
		}
		while_stat;

		struct
		{
			Ast init;
			Ast pred;
			Ast step;
			Ast body;
		}
		for_stat;

		struct
		{
			Ast decl;
			Ast body;
		}
		for_range_stat;

		struct
		{
			Ast expr;
		}
		continue_stat;

		struct
		{
			Ast expr;
		}
		break_stat;

	};
};

static Ast_T ast_error_sentinel;
static Ast_T ast_error_oom_sentinel;

#define NULL_AST      ((Ast)0)
#define ERROR_AST     (&ast_error_sentinel)
#define ERROR_OOM_AST (&ast_error_oom_sentinel)

static const struct Entity *ast_entity(Ast ast)
{
	return ast->entity;
}

static Ast ast_from_status(Com_Status status)
{
	switch (status)
	{
		case COM_STATUS_REPORTED: return ERROR_AST;
		case COM_STATUS_OOM:      return ERROR_OOM_AST;
		default: ASSERT(0);       return ERROR_AST;
	}
}

static inline b32 ast_is_error(Ast ast)
{
	return ast && ast->kind == AST_ERROR;
}

static Ast ast_ensure_kind(Ast ast, Ast_Kind type)
{
	if (!ast || ast_is_error(ast) || ast->kind != type) {
		return ERROR_AST;
	}

	return ast;
}

static inline const char *ast_type_name(Ast_Kind type)
{
	static const char *names[] =
	{
#define EXPAND(ENUM, NAME) #ENUM,
		AST_DEFINITIONS(EXPAND)
#undef EXPAND
	};

	if ((u32)type >= AST_COUNT_) {
		return "AST_UNKNOWN";
	}
	return names[type];
}
