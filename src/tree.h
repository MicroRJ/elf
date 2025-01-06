
// #define TREE_THIS       ((treeID)(1))
// #define TREE_LOOP_VALUE ((treeID)(2))
// #define TREE_LOOP_INDEX ((treeID)(3))
// #define TREE_LOOP_ARRAY ((treeID)(4))


typedef struct Parser Parser;

typedef struct treeT treeT;
typedef treeT *treeID;
typedef struct { treeID id; } treeID2;

#define TREEID(id) (treeID2){id}


#define NO_TREE 0

//block:
//z: are the statements
//expr-builder:
//x: is the resulting value
//z: are the statements to be evaluated
//note:mind order
#define TREEDEF(_) \
_(TREE_NONE) \
_(TREE_NOP) \
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
_(EXPR_THIS_REF) \
_(EXPR_LOCAL_REF) \
_(EXPR_GLOBAL_REF) \
_(EXPR_UPVALUE_REF) \
_(EXPR_NUM)_(EXPR_INT)_(EXPR_STR) \
_(EXPR_FUN)_(EXPR_TAB) \
_(EXPR_CLOSURE) \
_(EXPR_INDEX) \
_(EXPR_FIELD) \
_(EXPR_METAFIELD) \
_(EXPR_CALL) \
_(EXPR_NIL) \
_(EXPR_RANGE) \
_(EXPR_RANGE_INDEX) \
_(EXPR_MULTI) \
_(EXPR_COMPOSITE) \
_(STAT_ASSIGN_MEM) \
_(STAT_YIELD) \
_(STAT_STORE) \
_(STAT_BLOCK) \
_(STAT_IF) \
_(STAT_WHILE)_(STAT_DO_WHILE) \
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
	union {
		struct {
			treeID x,y,*z;
		};
		struct {
			char *name;
			union{
				treeID local_ref;
				int global_ref;
				int upvalue_ref;
			};
		}expr_ref;
		char *expr_str;
		elf_Int expr_int;
		elf_Num expr_num;
		struct {
			int scope;
			treeID enclosing;
			treeID *params;
			treeID body;
		} expr_fun;
		treeID stat_expr;
		struct {
			treeID pred;
			treeID true_clause;
			treeID else_clause;
		} stat_if;
		struct {
			treeID pred;
			treeID body;
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
static treeID tree_xy(Parser *, Source, treeKi k, treeTy ty, treeID x, treeID y);
static treeID tree_x(Parser *, Source, treeKi k, treeTy ty, treeID x);
static treeID tree_nil(Parser *, Source);
static treeID tree_int(Parser *, Source, elf_Int i);
static treeID tree_num(Parser *, Source, elf_Num n);
static treeID tree_str(Parser *, Source, Source);
static treeID tree_nullary(Parser *, Source, treeKi k, treeTy t);
static treeID tree_group(Parser *, Source, treeID x);
static treeID tree_table(Parser *, Source, treeID *z);
static treeID tree_closure(Parser *, Source, treeID x, treeID *z);
static treeID tree_store(Parser *, Source line, treeID x, treeID y);

static treeID tree_yield(Parser *, Source, treeID i);

static treeID tree_this_ref(Parser *, Source line);
static treeID tree_global_ref(Parser *, Source line, char *name, int x);
static treeID tree_local_ref(Parser *, Source line, char *name, treeID x);

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