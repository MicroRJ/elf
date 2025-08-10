//
// See Copyright Notice In elf.h
//









// todo: prob stop using dynamic arrays and use linked lists instead
// todo: this name can be confusing
#define NO_TREE 0
#define INVALID_TREE -1

typedef struct elf_Parser elf_Parser;

typedef struct treeT treeT;
typedef treeT *treeID;

/* MIND ORDER */
#define TREEDEF(_)   \
_(TREE_NONE)         \
_(TREE_NOP)          \
_(EXPR_AND)          \
_(EXPR_OR)           \
_(EXPR_ADD)          \
_(EXPR_SUB)          \
_(EXPR_MUL)          \
_(EXPR_DIV)          \
_(EXPR_POW)          \
_(EXPR_MOD)          \
_(EXPR_EQ)           \
_(EXPR_NEQ)          \
_(EXPR_LT)           \
_(EXPR_GT)           \
_(EXPR_LTEQ)         \
_(EXPR_GTEQ)         \
_(EXPR_BIT_SHL)      \
_(EXPR_BIT_SHR)      \
_(EXPR_BIT_AND)      \
_(EXPR_BIT_OR)       \
_(EXPR_BIT_XOR)      \
_(EXPR_NIL_AND)      \
_(EXPR_NIL_OR)       \
_(EXPR_BIT_NOT)      \
_(TREE_GLOBAL)       \
_(TREE_UPVALUE)      \
_(TREE_GETEXPR)      \
_(TREE_GETMEM)       \
_(TREE_SETMEM)       \
_(EXPR_NUM)          \
_(EXPR_INT)          \
_(EXPR_STR)          \
_(TREE_CALL)         \
_(TREE_FUNCTION)     \
_(TREE_NEW_TABLE)    \
_(EXPR_CLOSURE)      \
_(EXPR_INDEX)        \
_(EXPR_FIELD)        \
_(EXPR_METAFIELD)    \
_(EXPR_NIL)          \
_(TREE_STORE)        \
_(STAT_BLOCK)        \
_(TREE_IF)           \
_(TREE_GOTO)         \
_(TREE_WHILE_LOOP)   \
_(STAT_DO_WHILE)     \
_(TREE_RANGE)        \
_(EXPR_RANGE_INDEX)  \
_(TREE_TUPLE)        \
_(TREE_LABEL)        \
_(TREE_RET)          \
_(TREE_PROXY)        \
_(TREE_RELOAD)       \
/* end */

typedef enum {
#define TREE(NAME) NAME,
	TREEDEF(TREE)
#undef TREE
} treeKi;

STATIC_ASSERT(TREE_NONE == 0);
STATIC_ASSERT(TREE_NOP == 1);
STATIC_ASSERT((EXPR_LT^1) == EXPR_GT);
STATIC_ASSERT((EXPR_LTEQ^1) == EXPR_GTEQ);


typedef enum treeTy {
	NT_NON = 0,
	NT_ANY, NT_SYS,
	NT_NIL, NT_BOL, NT_INT, NT_NUM,
	NT_OBJ, NT_TAB, NT_FUN, NT_STR
} treeTy;


struct treeT {
	treeKi   kind;
	treeTy   type;
	Source   line;
	treeID   prox;
	union {
		struct {
			treeID x,y,*z;
		};
		// intenal to the generator, the generator fills this in
		// with a real address, when the tree is referenced again
		// the address is used
		int         jump;

		int       expr_global;
		int       expr_upvalue;
		char     *expr_str;
		elf_i64   expr_int;
		elf_f64   expr_num;
		treeID    proxyfor;
		struct {
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
			treeID      enc;
			// list of captures for the parser to remember which ones
			// it has already, also tells code gen the number of captures
			// for the proto
			treeID   *capts;
			// body of instructions for the code generator to make
			treeID     body;
			// at make time, associate a proto id with each function
			// so that when the closure instruction is generated, there's
			// a proto ready for it.
			int       proto;
		} tree_funexpr;
		struct {
			treeID *defers;
			treeID *breaks;
			treeID *stats; // match z
		} tree_blockstat;
		struct {
			treeID        pred;
			treeID true_clause;
			treeID else_clause;
			treeID then_clause;
		} tree_ifstat;
		struct {
			treeID pred;
			treeID prebody;
			treeID probody;
			treeID body;
			// todo: remove
			treeID *b,*c;
		} loop;
	};
};

// @global
static char *tree2s[]={
#define TREE(NAME) #NAME,
	TREEDEF(TREE)
#undef TREE
};

