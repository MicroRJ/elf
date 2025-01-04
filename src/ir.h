/*
** See Copyright Notice In elf.h
** ir.h
*/

typedef struct Parser Parser;


typedef int IR_Id;
#define NO_IR (-1)

// #define NO_IR 0
// typedef IR_Node *IR_Id;

typedef struct { IR_Id id; } IR_Id2;


#define SPECIAL_REGISTER_THIS   (  0) // #this
#define SPECIAL_REGISTER_INDEX  (256) // #index
#define SPECIAL_REGISTER_VALUE  (257) // #value
#define SPECIAL_REGISTER_ARRAY  (258) // #array


#define NODE(id) (IR_Id2){id}


typedef enum IR_DataTy {
	NT_NON = 0,
	NT_ANY, NT_SYS,
	NT_NIL, NT_BOL, NT_INT, NT_NUM,
	NT_OBJ, NT_TAB, NT_FUN, NT_STR
} IR_DataTy;


/* Note: preserve order - rj */
#define IRDEF(_) \
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
_(PUSH_MEMORY_STATE)\
_(POP_MEMORY_STATE)\
_(BASIC_BLOCK)\
_(GOTO)\
_(IF)\
_(PARAM)\
_(YIELD)\
_(LOAD)\
_(LOAD_DIRECT)\
/* end */



typedef enum IR_Kind {
	IR_NONE = 0,
#define TREE(NAME) IR_##NAME,
	IRDEF(TREE)
#undef TREE
} IR_Kind;

typedef struct IR_Node IR_Node;
struct IR_Node {
	union { IR_Kind kind, ki, k; };
	union { IR_DataTy type, ty; };

	IR_Id prox;
	union {
		struct { IR_Id x,y,*z; };
		union {
			char     *s;
			elf_Int   i;
			elf_Num   n;
		};
	};

	Source line;
};

static IR_Node get_ir(Parser *fs, IR_Id id);
static IR_Kind get_ir_kind(Parser *fs, IR_Id id);
static IR_DataTy get_ir_type(Parser *fs, IR_Id id);
static Source get_ir_line(Parser *fs, IR_Id id);
static IR_Id node_xyz(Parser *fs, Source, IR_Kind k, IR_DataTy ty, IR_Id x, IR_Id y, IR_Id *z);
static IR_Id node_xy(Parser *fs, Source, IR_Kind k, IR_DataTy ty, IR_Id x, IR_Id y);
static IR_Id node_x(Parser *fs, Source, IR_Kind k, IR_DataTy ty, IR_Id x);
static IR_Id node_nil(Parser *fs, Source);
static IR_Id node_int(Parser *fs, Source, elf_Int i);
static IR_Id ir_number(Parser *fs, Source, elf_Num n);
static IR_Id node_str(Parser *fs, Source, Source);
static IR_Id node_nullary(Parser *fs, Source, IR_Kind k, IR_DataTy t);
static IR_Id node_group(Parser *fs, Source, IR_Id x);
static IR_Id node_new_table(Parser *fs, Source, IR_Id *z);
static IR_Id ir_new_closure(Parser *fs, Source, IR_Id x, IR_Id *z);
static IR_Id node_store(Parser *fs, Source line, IR_Id x, IR_Id y);

static IR_Id ir_yield(Parser *fs, Source, IR_Id i);
static IR_Id ir_param(Parser *fs, Source, IR_Id i);
static IR_Id node_global(Parser *fs, Source line, IR_Id i);

/* Todo: deprecate */
static IR_Id node_local(Parser *fs, Source line, IR_Id i);

static IR_Id ir_this(Parser *fs, Source line);
static IR_Id node_closure_value(Parser *fs, Source line, IR_Id i);
static IR_Id node_type_guard(Parser *fs, Source line, IR_Id x, IR_DataTy y);
static IR_Id node_metafield(Parser *fs, Source line, IR_Id x, IR_Id y);
static IR_Id node_field(Parser *fs, Source line, IR_Id x, IR_Id y);
static IR_Id node_index(Parser *fs, Source line, IR_Id x, IR_Id y);
static IR_Id node_ranged_index(Parser *fs, Source line, IR_Id x, IR_Id y);
static IR_Id node_call(Parser *fs, Source line, IR_Id x, IR_Id *z);
static IR_Id node_less_than(Parser *fs, Source line, IR_Id x, IR_Id y);
static IR_Id node_call_metafield(Parser *fs, Source line, IR_Id x, IR_Id *z, char *name);
static IR_Id node_multi(Parser *fs, Source line, IR_Id *z);
static IR_Id node_global_name(Parser *fs, Source line, char *name);
static IR_Id node_call_pf(Parser *fs, Source line, IR_Id *args);
static IR_Id node_call_set_metatable(Parser *fs, Source line, IR_Id object, IR_Id metatable);

static IR_Id ir_block(Parser *fs, Source line, IR_Id src, IR_Id end);


static elf_ValueTag node2tag(IR_DataTy ty);
static ByteOP ir2b(IR_Kind tt);
static elf_Bool node_is_lvalue(IR_Kind kind);