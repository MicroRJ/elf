/*
** See Copyright Notice In elf.h
** node.h
*/


typedef struct FileState FileState;


#define NO_NODE (-1)


#define SPECIAL_REGISTER_THIS   (     0) // #this
#define SPECIAL_REGISTER_INDEX  (0xff+1) // #index
#define SPECIAL_REGISTER_VALUE  (0xff+2) // #value
#define SPECIAL_REGISTER_ARRAY  (0xff+3) // #array


typedef int NodeId;


typedef struct { NodeId id; } NodeIdGuard;


#define NODE(id) (NodeIdGuard){id}


typedef enum NodeTy {
	NT_NON = 0,
	NT_ANY, NT_SYS,
	NT_NIL, NT_BOL, NT_INT, NT_NUM,
	NT_OBJ, NT_TAB, NT_FUN, NT_STR
} NodeTy;


/* Nodes AND through GTEQ come in pairs,
use ^ to get the counter instruction.
AND should be an even number so that this
works... */
#define NODE_LIST(_) \
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
typedef enum NodeKi {
	NODE_NONE = 0,
	NODE_LIST(NODE_ENUM)
} NodeKi;
#undef NODE_ENUM


/* todo: make this more compact! */
typedef struct Node {
	union { NodeKi kind, ki, k; };
	union { NodeTy type, ty, t; };
	Source line;
	/* todo: eventually remove this */
	int level;

	union {
		struct { NodeId x,y,*z; };
		union {
			char      *s;
			elf_Int  i;
			elf_Num   n;
		} lit;
	};
} Node;


static Node get_target_node(FileState *fs, NodeIdGuard id);
static Node get_node(FileState *fs, NodeId id);
static NodeKi get_node_kind(FileState *fs, NodeId id);
static NodeTy get_node_type(FileState *fs, NodeId id);
static Source get_node_line(FileState *fs, NodeId id);
static NodeId node_xyz(FileState *fs, Source, NodeKi k, NodeTy ty, NodeId x, NodeId y, NodeId *z);
static NodeId node_xy(FileState *fs, Source, NodeKi k, NodeTy ty, NodeId x, NodeId y);
static NodeId node_x(FileState *fs, Source, NodeKi k, NodeTy ty, NodeId x);
static NodeId node_nil(FileState *fs, Source);
static NodeId node_integer(FileState *fs, Source, elf_Int i);
static NodeId node_number(FileState *fs, Source, elf_Num n);
static NodeId node_string(FileState *fs, Source, Source);
static NodeId node_nullary(FileState *fs, Source, NodeKi k, NodeTy t);
static NodeId node_group(FileState *fs, Source, NodeId x);
static NodeId node_new_table(FileState *fs, Source, NodeId *z);
static NodeId node_new_closure(FileState *fs, Source, NodeId x, NodeId *z);
static NodeId node_store(FileState *fs, Source line, NodeId x, NodeId y);
static NodeId node_local(FileState *fs, Source line, NodeId i);
static NodeId node_this(FileState *fs, Source line);
static NodeId node_global(FileState *fs, Source line, NodeId i);
static NodeId node_closure_value(FileState *fs, Source line, NodeId i);
static NodeId node_type_guard(FileState *fs, Source line, NodeId x, NodeTy y);
static NodeId node_metafield(FileState *fs, Source line, NodeId x, NodeId y);
static NodeId node_field(FileState *fs, Source line, NodeId x, NodeId y);
static NodeId node_index(FileState *fs, Source line, NodeId x, NodeId y);
static NodeId node_ranged_index(FileState *fs, Source line, NodeId x, NodeId y);
static NodeId node_call(FileState *fs, Source line, NodeId x, NodeId *z);
static NodeId node_less_than(FileState *fs, Source line, NodeId x, NodeId y);
static NodeId node_call_metafield(FileState *fs, Source line, NodeId x, NodeId *z, char *name);
static NodeId node_multi(FileState *fs, Source line, NodeId *z);
static NodeId node_global_name(FileState *fs, Source line, char *name);
static NodeId node_call_pf(FileState *fs, Source line, NodeId *args);
static NodeId node_call_set_metatable(FileState *fs, Source line, NodeId object, NodeId metatable);

static elf_ValueTag node2tag(NodeTy ty);
static ByteOP node2byte(NodeKi tt);
static elf_Bool node_is_lvalue(NodeKi kind);