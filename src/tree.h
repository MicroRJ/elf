
typedef struct Tree Tree;
typedef Tree *treeID;
#define NO_TREE 0


#define TREEDEF(_) \
_(TREE_NONE) \
_(EXPR_IDENT) \
_(EXPR_FUN) \
_(EXPR_NUM) \
_(EXPR_INT) \
_(EXPR_STR) \
_(EXPR_NIL) \
_(EXPR_UNARY) \
_(EXPR_RANGE) \
_(EXPR_AND) \
_(EXPR_OR) \
_(EXPR_NIL_OR) \
_(EXPR_NIL_AND) \
_(EXPR_ADD) \
_(EXPR_SUB) \
_(EXPR_DIV) \
_(EXPR_MUL) \
_(EXPR_POW) \
_(EXPR_MOD) \
_(EXPR_NEQ) \
_(EXPR_EQ) \
_(EXPR_GT) \
_(EXPR_GTEQ) \
_(EXPR_LT) \
_(EXPR_LTEQ) \
_(EXPR_BIT_SHL) \
_(EXPR_BIT_SHR) \
_(EXPR_BIT_XOR) \
_(EXPR_BIT_OR) \
_(EXPR_BIT_AND) \
_(EXPR_NONE) \
_(EXPR_STAT) \
_(STAT_DECL) \
_(STAT_ASSIGN) \
_(STAT_BLOCK) \
_(STAT_IF) \
_(STAT_FOR) \
_(STAT_WHILE) \
_(STAT_DO_WHILE) \
/* end */

enum {
#define TREE(NAME) NAME,
	TREEDEF(TREE)
#undef TREE
};


struct Tree {
	elf_i32 kind;
	Source line;
	union {
		char *expr_ident;
		char *expr_str;
		elf_Int expr_int;
		elf_Num expr_num;
		treeID expr_unary;
		struct {
			treeID x;
			treeID y;
		} expr_binary;
		struct {
			treeID enclosing;
			treeID *params;
			treeID body;
		} expr_fun;
		treeID stat_expr;
		treeID *stat_block;
		struct {
			treeID name;
			treeID value;
		} stat_decl;
		struct {
			treeID x;
			treeID y;
		} stat_assign;
		struct {
			treeID predicate;
			treeID true_clause;
			treeID else_clause;
		} stat_if;
		struct {
			treeID predicate;
			treeID body;
		} stat_while;
		struct {
			treeID name;
			treeID predicate;
			treeID body;
		} stat_for;
	};
};