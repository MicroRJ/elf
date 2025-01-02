/*
** See Copyright Notice In elf.h
** tree.h
*/

typedef struct Parser Parser;

#define NO_NODE (-1)


#define SPECIAL_REGISTER_THIS   (     0) // #this
#define SPECIAL_REGISTER_INDEX  (0xff+1) // #index
#define SPECIAL_REGISTER_VALUE  (0xff+2) // #value
#define SPECIAL_REGISTER_ARRAY  (0xff+3) // #array


typedef int TreeId;


typedef struct { TreeId id; } NodeIdGuard;


#define NODE(id) (NodeIdGuard){id}


typedef enum TreeTy {
	NT_NON = 0,
	NT_ANY, NT_SYS,
	NT_NIL, NT_BOL, NT_INT, NT_NUM,
	NT_OBJ, NT_TAB, NT_FUN, NT_STR
} TreeTy;


/* Note: preserve order - rj */
#define TREEDEF(_) \
_(NOP)\
_(AND)_(OR)_(NIL_AND)_(NIL_OR)\
_(EQ)_(NEQ)_(LT)_(GT)_(LTEQ)_(GTEQ)\
_(ADD)_(SUB)_(MUL)_(DIV)\
_(BIT_SHL)_(BIT_SHR)\
_(INDEX)_(FIELD)\
_(BIT_AND)_(BIT_OR)_(BIT_XOR)\
_(MOD)_(POW)\
_(TYPEGUARD)\
_(STORE)\
_(CLOSURE) _(STRING) _(TABLE) \
_(INTEGER) _(NUMBER) _(NIL) \
_(GLOBAL) _(LOCAL) _(CLSVAL) _(FILE_VALUE) \
_(MULTI)\
_(METAFIELD)\
_(CALL)\
_(RANGE_INDEX)\
_(RANGE)\
_(GROUP)


#define NODE_ENUM(NAME) NODE_##NAME,
typedef enum TreeKi {
	NODE_NONE = 0,
	TREEDEF(NODE_ENUM)
} TreeKi;
#undef NODE_ENUM

/* todo: make this more compact! */
typedef struct Tree {
	union { TreeKi kind, ki, k; };
	union { TreeTy type, ty, t; };
	Source line;
	/* todo: eventually remove this */
	int level;

	union {
		struct { TreeId x,y,*z; };
		// union {
		// 	char     *s;
		// 	elf_Int   i;
		// 	elf_Num   n;
		// } lit;
		union {
			char     *s;
			elf_Int   i;
			elf_Num   n;
		};
	};
} Tree;


static Tree get_target_node(Parser *fs, NodeIdGuard id);
static Tree get_node(Parser *fs, TreeId id);
static TreeKi get_node_kind(Parser *fs, TreeId id);
static TreeTy get_node_type(Parser *fs, TreeId id);
static Source get_node_line(Parser *fs, TreeId id);
static TreeId node_xyz(Parser *fs, Source, TreeKi k, TreeTy ty, TreeId x, TreeId y, TreeId *z);
static TreeId node_xy(Parser *fs, Source, TreeKi k, TreeTy ty, TreeId x, TreeId y);
static TreeId node_x(Parser *fs, Source, TreeKi k, TreeTy ty, TreeId x);
static TreeId node_nil(Parser *fs, Source);
static TreeId node_integer(Parser *fs, Source, elf_Int i);
static TreeId node_number(Parser *fs, Source, elf_Num n);
static TreeId node_string(Parser *fs, Source, Source);
static TreeId node_nullary(Parser *fs, Source, TreeKi k, TreeTy t);
static TreeId node_group(Parser *fs, Source, TreeId x);
static TreeId node_new_table(Parser *fs, Source, TreeId *z);
static TreeId node_new_closure(Parser *fs, Source, TreeId x, TreeId *z);
static TreeId node_store(Parser *fs, Source line, TreeId x, TreeId y);
static TreeId node_local(Parser *fs, Source line, TreeId i);
static TreeId node_this(Parser *fs, Source line);
static TreeId node_global(Parser *fs, Source line, TreeId i);
static TreeId node_closure_value(Parser *fs, Source line, TreeId i);
static TreeId node_type_guard(Parser *fs, Source line, TreeId x, TreeTy y);
static TreeId node_metafield(Parser *fs, Source line, TreeId x, TreeId y);
static TreeId node_field(Parser *fs, Source line, TreeId x, TreeId y);
static TreeId node_index(Parser *fs, Source line, TreeId x, TreeId y);
static TreeId node_ranged_index(Parser *fs, Source line, TreeId x, TreeId y);
static TreeId node_call(Parser *fs, Source line, TreeId x, TreeId *z);
static TreeId node_less_than(Parser *fs, Source line, TreeId x, TreeId y);
static TreeId node_call_metafield(Parser *fs, Source line, TreeId x, TreeId *z, char *name);
static TreeId node_multi(Parser *fs, Source line, TreeId *z);
static TreeId node_global_name(Parser *fs, Source line, char *name);
static TreeId node_call_pf(Parser *fs, Source line, TreeId *args);
static TreeId node_call_set_metatable(Parser *fs, Source line, TreeId object, TreeId metatable);

static elf_ValueTag node2tag(TreeTy ty);
static ByteOP node2byte(TreeKi tt);
static elf_Bool node_is_lvalue(TreeKi kind);