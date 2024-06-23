/*
** See Copyright Notice In elf.h
** elf-node.h
** IR?...
*/


#define NO_NODE (-1)


typedef int elNodeId;

typedef struct {
	elNodeId id;
} elNodeIdTypeGuard;

#define MAKE_NODE_ID(id) (elNodeIdTypeGuard){id}


typedef enum elNodeTy {
	NT_NON = 0,
	NT_ANY, NT_SYS,
	NT_NIL, NT_BOL, NT_INT, NT_NUM,
	NT_OBJ, NT_TAB, NT_FUN, NT_STR
} elNodeTy;


// THIS: this
// INDEX: {x}[{x}]
// FIELD: {x}.{x}
// METAFIELD: {x}:{x}
// CALL: {x}({x})
// RANGE_INDEX: [{x}..{x}]
// RANGE: {x}..{x}
// GROUP: ({x})
#define NODE_DEF(_) \
	_(NONE)\
	_(NOP)\
	_(AND) _(OR)\
	_(EQ) _(NEQ)\
	_(BITSHL) _(BITSHR)\
	_(ADD) _(SUB) _(MUL) _(DIV)\
	_(LT) _(GT) _(LTEQ) _(GTEQ)\
	_(BITXOR) _(MOD) _(BITOR)\
	_(TYPEGUARD)\
	_(LOAD)\
	_(CLOSURE) _(STRING) _(TABLE)\
	_(INTEGER) _(NUMBER) _(NIL)\
	_(GLOBAL) _(LOCAL) _(CLOSURE_VALUE) _(FILE_VALUE)\
	_(THIS)\
	_(INDEX)\
	_(FIELD) _(METAFIELD)\
	_(CALL)\
	_(RANGE_INDEX)\
	_(RANGE)\
	_(GROUP)\
	_(REGION)


typedef enum elNodeKi {
#define NODE_ENUM(NAME) NODE_##NAME,

	NODE_DEF(NODE_ENUM)

#undef NODE_ENUM
} elNodeKi;


char *elNodeToStr[] = {
#define NODE_ENUM(NAME) #NAME,

	NODE_DEF(NODE_ENUM)

#undef NODE_ENUM
};


/* todo */
typedef struct elNode {
	union { elNodeKi kind, k; };
	union { elNodeTy type, ty, t; };
	elf_lineid line;
	/* todo: eventually remove this */
	int level;
	struct { elNodeId x,y,*z; };
	// elRegId x_r;
	/* todo?: don't quite union these two for debugging? */
	union {
		char   *s;
		elInteger i;
		elNumber n;
	} lit;
} elNode;




elNodeId elf_make_node_xyz(elFileState *fs, elf_lineid, elNodeKi k, elNodeTy ty, elNodeId x, elNodeId y, elNodeId *z);
elNodeId elf_make_binary_node(elFileState *fs, elf_lineid, elNodeKi k, elNodeTy ty, elNodeId x, elNodeId y);
elNodeId elf_nodeunary(elFileState *fs, elf_lineid, elNodeKi k, elNodeTy ty, elNodeId x);
elNodeId elf_make_nullary_node(elFileState *fs, elf_lineid, elNodeKi k, elNodeTy t);

elNodeId elf_make_group_node(elFileState *fs, elf_lineid, elNodeId x);
elNodeId elf_make_region_node(elFileState *fs, elf_lineid, elNodeId x, elNodeId *z);

elNodeId elf_make_nil_node(elFileState *fs, elf_lineid);
elNodeId elf_make_integer_node(elFileState *fs, elf_lineid, elInteger i);
elNodeId elf_make_number_node(elFileState *fs, elf_lineid, elNumber n);
elNodeId elf_make_string_node(elFileState *fs, elf_lineid, char *);
elNodeId elf_make_table_node(elFileState *fs, elf_lineid, elNodeId *z);
elNodeId elf_make_closure_node(elFileState *fs, elf_lineid, elNodeId x, elNodeId *z);

elNodeId elf_make_load_node(elFileState *fs, elf_lineid line, elNodeId x, elNodeId y);

elNodeId elf_make_local_value_node(elFileState *fs, elf_lineid line, elNodeId i);
elNodeId elf_make_global_value_node(elFileState *fs, elf_lineid line, elNodeId i);
elNodeId elf_make_closure_value_node(elFileState *fs, elf_lineid line, elNodeId i);

elNodeId elf_make_type_guard_node(elFileState *fs, elf_lineid line, elNodeId x, elNodeTy y);
elNodeId elf_make_meta_field_node(elFileState *fs, elf_lineid line, elNodeId x, elNodeId y);
elNodeId elf_make_field_node(elFileState *fs, elf_lineid line, elNodeId x, elNodeId y);
elNodeId elf_make_index_node(elFileState *fs, elf_lineid line, elNodeId x, elNodeId y);

elNodeId elf_nodeloadfile(elFileState *fs, elf_lineid line, elNodeId x);

elNodeId elf_make_ranged_index_node(elFileState *fs, elf_lineid line, elNodeId x, elNodeId y);

elNodeId elf_make_call_node(elFileState *fs, elf_lineid line, elNodeId x, elNodeId *z);


elf_tag elf_nodettotag(elNodeTy ty) {
	switch (ty) {
		case NT_SYS: return TAG_SYS;
		case NT_NUM: return TAG_NUM;
		case NT_INT: return TAG_INT;
		default: elf_unreachable;
	}
	return TAG_NIL;
}
