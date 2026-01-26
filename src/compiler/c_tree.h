//
// See Copyright Notice In elf.h
//


#define Y_NULL     (0)
// some internal error
#define Y_ERROR   ((TreeId) (-1))
// undeclared identifier
#define Y_UNIDENT ((TreeId) (-2))

#define istree(t) ((t) > 0)
#define notree(t) ((t) <= 0)
#define iserror(t) ((t) < 0)


typedef struct Tree Tree;
typedef Tree *TreeId;

/* MIND ORDER */
#define TREEDEF(_)                \
_(TREE_NONE)                      \
_(TREE_NOP)                       \
_(EXPR_AND)                       \
_(EXPR_OR)                        \
_(EXPR_ADD)                       \
_(EXPR_SUB)                       \
_(EXPR_MUL)                       \
_(EXPR_DIV)                       \
_(EXPR_POW)                       \
_(EXPR_MOD)                       \
_(EXPR_EQ)                        \
_(EXPR_NEQ)                       \
_(EXPR_LT)                        \
_(EXPR_GT)                        \
_(EXPR_LTEQ)                      \
_(EXPR_GTEQ)                      \
_(EXPR_BIT_SHL)                   \
_(EXPR_BIT_SHR)                   \
_(EXPR_BIT_AND)                   \
_(EXPR_BIT_OR)                    \
_(EXPR_BIT_XOR)                   \
_(EXPR_NIL_AND)                   \
_(EXPR_NIL_OR)                    \
_(EXPR_BIT_NOT)                   \
_(TREE_GLOBAL)                    \
_(TREE_UPVALUE)                   \
_(TREE_ENFORCE)                   \
_(TREE_DEBUG_GET_EXPRESSION_NAME) \
_(TREE_DEBUG_GET_MEMORY)          \
_(TREE_MEMORY)                    \
_(TREE_BLOCK)                     \
_(TREE_PROXY)                \
_(EXPR_NUM)                       \
_(EXPR_INT)                       \
_(EXPR_STR)                       \
_(TREE_CALL)                      \
_(TREE_META_CALL)                 \
_(TREE_FUNCTION)                  \
_(TREE_NEW_TABLE)                 \
_(EXPR_CLOSURE)                   \
_(TREE_LENGTH)                    \
_(TREE_INDEX)                     \
_(TREE_TABLE_FIELD)               \
_(EXPR_METAFIELD)                 \
_(EXPR_NIL)                       \
_(TREE_STORE)                     \
_(TREE_IF)                        \
_(TREE_GOTO)                      \
_(TREE_WHILE_LOOP)                \
_(TREE_RANGE)                     \
_(TREE_RANGE_INDEX)               \
_(TREE_TUPLE)                     \
_(TREE_RET)                       \
/* end */

typedef enum
{
#define TREE(NAME) NAME,
	TREEDEF(TREE)
#undef TREE
}
TreeKind;

STATIC_ASSERT(TREE_NONE == 0);
STATIC_ASSERT(TREE_NOP  == 1);
STATIC_ASSERT((EXPR_LT   ^ 1) == EXPR_GT  );
STATIC_ASSERT((EXPR_LTEQ ^ 1) == EXPR_GTEQ);

typedef enum DataType
{
	NT_NON = 0,
	NT_ANY, NT_SYS,
	NT_NIL, NT_BOL, NT_INT, NT_NUM,
	NT_OBJ, NT_TAB, NT_FUN, NT_STR
}
DataType;

#define TREECHAIN_EMPTY ((TreeChain){ 0 })

typedef struct TreeChain TreeChain;
struct TreeChain
{
	Tree *head;
	Tree *tail;
	int   tally;
};


#define TREE_FOR(N, S) for (Tree *N = S; N; N = (N)->next)

// Todo, make the tree minimal!
struct Tree
{
	TreeKind   kind;
	DataType   type;
	Tree      *next;
	union
	{
		Source  line;
		Source  cursor;
	};
	struct
	{
		TreeId x,y;
		u32    n,r;
	};
	union
	{
		int      expr_global;
		int      expr_cvalue;
		char    *expr_str;
		i64      expr_int;
		f64      expr_num;
		int      jump;
		TypeRule rule;
		struct
		{
			int   rem;
			int   mem;
		}
		tree_memory;
		// Todo, remove unnecessary stuff from here!
		struct
		{
			// scope is used during parsing to detect captures,
			// could be placed somewhere else, some sort of function stack,
			// but here is convenient.
			i16       scope;
			// the arity for the code generator to create the prototype
			// includes 'this', so it is always at least 1.
			u8        arity;
			u8     variadic;
			// this is the current parent of this function, only used
			// during parsing to traverse the function hierarchy.
			TreeId      enc;
			// list of captures for the parser to remember which ones
			// it has already, also tells code gen the number of captures
			// for the proto and emits the instructions for creating new
			// closure
			TreeId   *capts;
			// body of instructions for the code generator to make
			TreeId     body;
			// at make time, associate a proto id with each function
			// so that when the closure instruction is generated, there's
			// a proto ready for it.
			int       proto;
		}
		tree_funexpr;
		struct
		{
			TreeId        pred;
			TreeId true_clause;
			TreeId else_clause;
			// TreeId then_clause;
		}
		tree_ifstat;
		// Todo, remove this mess!
		struct
		{
			TreeId pred;
			TreeId prebody;
			TreeId probody;
			TreeId body;
			TreeId prepred;
			TreeId *b,*c;
		}
		loop;
	};
};

