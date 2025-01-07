#define TREE_THIS ((treeID)(1))
#define NO_TREE 0

typedef struct Parser Parser;

typedef struct treeT treeT;
typedef treeT *treeID;

/* MIND ORDER */
#define TREEDEF(_) \
_(TREE_NONE) \
_(TREE_NOP)  \
_(EXPR_AND) _(EXPR_OR)   \
_(EXPR_ADD) _(EXPR_SUB)  \
_(EXPR_DIV) _(EXPR_MUL)  \
_(EXPR_POW) _(EXPR_MOD)  \
_(EXPR_NEQ) _(EXPR_EQ)   \
_(EXPR_GT)  _(EXPR_GTEQ) \
_(EXPR_LT)  _(EXPR_LTEQ) \
_(EXPR_BIT_SHL)_(EXPR_BIT_SHR) \
_(EXPR_BIT_XOR)_(EXPR_BIT_OR) \
_(EXPR_BIT_AND) \
_(EXPR_NIL_OR)_(EXPR_NIL_AND) \
_(TREE_DUMMY) \
_(TREE_GLOBAL) \
_(TREE_UPVALUE) \
_(EXPR_NUM)_(EXPR_INT)_(EXPR_STR) \
_(TREE_FUNCTION)_(TREE_NEW_TABLE) \
_(EXPR_CLOSURE) \
_(EXPR_INDEX) \
_(EXPR_FIELD) \
_(EXPR_METAFIELD) \
_(EXPR_CALL) \
_(EXPR_NIL) \
_(TREE_ASSIGN_MEM) \
_(TREE_STORE) \
_(STAT_BLOCK) \
_(TREE_IF) \
_(TREE_WHILE_LOOP)_(STAT_DO_WHILE) \
_(TREE_RANGE) \
_(EXPR_RANGE_INDEX) \
_(EXPR_MULTI) \
_(TREE_RET) \
/* end */

typedef enum {
#define TREE(NAME) NAME,
	TREEDEF(TREE)
#undef TREE
} treeKi;

typedef enum treeTy {
	NT_NON = 0,
	NT_ANY, NT_SYS,
	NT_NIL, NT_BOL, NT_INT, NT_NUM,
	NT_OBJ, NT_TAB, NT_FUN, NT_STR
} treeTy;


struct treeT {
	treeKi kind;
	treeTy type;
	Source line;
	treeID prox;
	union {
		struct {
			treeID x,y,*z;
		};
		int  expr_global;
		int expr_upvalue;
		char   *expr_str;
		elf_i64 expr_int;
		elf_f64 expr_num;
		struct {
			treeID       body;
			//seems we need this...
			int         proto;
			//todo:we don't need these beyond parsing,
			//could do with a stack...
			int         scope;
			treeID  enc;
		} expr_fun;
		struct {
			treeID        pred;
			treeID true_clause;
			treeID else_clause;
			treeID then_clause;
		} stat_if;
		struct {
			treeID pred;
			treeID prev;
			treeID body;
			treeID post;
		} stat_while;
	};
};

char *tree2s[]={
#define TREE(NAME) #NAME,
	TREEDEF(TREE)
#undef TREE
};



static treeT get_tree(Parser *, treeID id);
static treeKi get_tree_kind(Parser *, treeID id);
static treeTy get_tree_type(Parser *, treeID id);
static Source get_tree_line(Parser *, treeID id);
static treeID tree_xyz(Parser *, Source, treeKi k, treeTy ty, treeID x, treeID y, treeID *z);
static treeID tree_binary(Parser *, Source, treeKi k, treeTy ty, treeID x, treeID y);
static treeID tree_x(Parser *, Source, treeKi k, treeTy ty, treeID x);
static treeID tree_nil(Parser *, Source);
static treeID tree_int(Parser *, Source, elf_Int i);
static treeID tree_num(Parser *, Source, elf_Num n);
static treeID tree_str(Parser *, Source, Source);
static treeID tree_nullary(Parser *, Source, treeKi k, treeTy t);
static treeID tree_group(Parser *, Source, treeID x);
static treeID tree_table(Parser *, Source);
static treeID tree_closure(Parser *, Source, treeID x, treeID *z);
static treeID tree_store(Parser *, Source line, treeID x, treeID y);

static treeID tree_ret(Parser *, Source, treeID i);
static treeID tree_dummy(Parser *, Source);

static treeID tree_this_ref(Parser *, Source line);
static treeID tree_global_ref(Parser *, Source line, char *name, int x);

static treeID tree_type_guard(Parser *, Source line, treeID x, treeTy y);
static treeID tree_metafield(Parser *, Source line, treeID x, treeID y);
static treeID tree_field(Parser *, Source line, treeID x, treeID y);
static treeID tree_index(Parser *, Source line, treeID x, treeID y);
static treeID tree_ranged_index(Parser *, Source line, treeID x, treeID y);
static treeID tree_call(Parser *, Source line, treeID x, treeID *z);
static treeID tree_less_than(Parser *, Source line, treeID x, treeID y);
static treeID tree_call_metafield(Parser *, Source line, treeID x, treeID *z, char *name);
static treeID tree_multi(Parser *, Source line, treeID *z);
static treeID tree_block(Parser *, Source line, treeID *z);

static treeID tree_global_ref_by_name(Parser *, Source line, char *name);
static treeID tree_call_pf(Parser *, Source line, treeID *args);
static treeID tree_call_set_metatable(Parser *, Source line, treeID object, treeID metatable);