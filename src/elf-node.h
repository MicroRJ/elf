/*
** See Copyright Notice In elf.h
** elf-node.h
** IR?...
*/


#define NO_NODE (-1)


typedef int elNodeID;


typedef enum elNodeTy {
	NT_ANY, NT_SYS,
	NT_NIL, NT_BOL, NT_INT, NT_NUM,
	NT_OBJ, NT_TAB, NT_FUN, NT_STR
} elNodeTy;


typedef enum elNodeOP {
	NODE_NONE = 0,
	NODE_NOP  = 1,
	/* counterpart pairs, do ^1 to get the opposite op */
	NODE_AND, NODE_OR,
	NODE_EQ, NODE_NEQ,
	NODE_BITSHL, NODE_BITSHR,
	NODE_ADD, NODE_SUB,
	NODE_MUL, NODE_DIV,
	NODE_LT, NODE_GT,
	NODE_LTEQ, NODE_GTEQ,
	/* end */
	NODE_BITXOR, NODE_MOD, NODE_BITOR,

	NODE_TYPEGUARD,

	NODE_LOAD,
	NODE_FILE,
	/* constant values, closure points to prototype
	in proto table in module, all other constants
	are embedded in the node and the generator decides
	how to emit the instruction. */
	NODE_CLOSURE, NODE_STRING, NODE_TABLE,
	NODE_INTEGER, NODE_NUMBER, NODE_NIL,
	// Global, Function and Closure values...
	NODE_GLOBAL, NODE_LOCAL, NODE_CLSVAL,
	NODE_THIS,// this
	NODE_INDEX,// {x}[{x}]
	NODE_FIELD,// {x}.{x}
	NODE_METAFIELD,// {x}:{x}
	NODE_CALL,// {x}({x})
	/* some builtin instruction node, x is the
	builtin id, (the token type) and z the
	inputs */
	NODE_BUILTIN,// ID({x})
	/* psuedo ops */
	NODE_RANGE_INDEX,// [{x}..{x}]
	NODE_RANGE,// {x}..{x}
	NODE_GROUP,// ({x})
} elNodeOP;


/* -- todo: make this more compact */
typedef struct elNode {
	elNodeOP k;
	elNodeTy t;
	elf_lineid line;
	/* todo: eventually remove this */
	int level;
	/* ---------------------------
	x,y are node inputs/operands,
	if more than 2 are required,
	use z*.
	r represents a tangible memory
	location, used for register
	allocation. */
	struct { elNodeID x,y,*z; };
	elf_localid _r;
	/* todo?: don't quite union these two for debugging? */
	union {
		char   *s;
		elInteger i;
		elNumber n;
	} lit;
} elNode;


elNodeID elf_nodexyz(elFileState *fs, elf_lineid, elNodeOP k, elNodeTy t, elNodeID x, elNodeID y, elNodeID *z);
elNodeID elf_nodebinary(elFileState *fs, elf_lineid, elNodeOP k, elNodeTy t, elNodeID x, elNodeID y);
elNodeID elf_nodeunary(elFileState *fs, elf_lineid, elNodeOP k, elNodeTy t, elNodeID x);
elNodeID elf_nodenullary(elFileState *fs, elf_lineid, elNodeOP k, elNodeTy t);

elNodeID elf_nodegroup(elFileState *fs, elf_lineid, elNodeID x);

elNodeID elf_nodenil(elFileState *fs, elf_lineid);
elNodeID elf_make_integer_node(elFileState *fs, elf_lineid, elInteger i);
elNodeID elf_nodenum(elFileState *fs, elf_lineid, elNumber n);
elNodeID elf_make_string_node(elFileState *fs, elf_lineid, char *);
elNodeID elf_make_table_node(elFileState *fs, elf_lineid, elNodeID *z);
elNodeID elf_nodecls(elFileState *fs, elf_lineid, elNodeID x, elNodeID *z);

elNodeID elf_make_load_node(elFileState *fs, elf_lineid line, elNodeID x, elNodeID y);

elNodeID elf_nodelocal(elFileState *fs, elf_lineid line, elNodeID i);
elNodeID elf_nodeglobal(elFileState *fs, elf_lineid line, elNodeID i);
elNodeID elf_nodeclsval(elFileState *fs, elf_lineid line, elNodeID i);

elNodeID elf_nodetypeguard(elFileState *fs, elf_lineid line, elNodeID x, elNodeTy y);
elNodeID elf_nodemetafield(elFileState *fs, elf_lineid line, elNodeID x, elNodeID y);
elNodeID elf_make_field_node(elFileState *fs, elf_lineid line, elNodeID x, elNodeID y);
elNodeID elf_nodeindex(elFileState *fs, elf_lineid line, elNodeID x, elNodeID y);

elNodeID elf_nodeloadfile(elFileState *fs, elf_lineid line, elNodeID x);

elNodeID elf_noderangedindex(elFileState *fs, elf_lineid line, elNodeID x, elNodeID y);

elNodeID elf_nodebuiltincall(elFileState *fs, elf_lineid line, ltokentype k, elNodeID *z);
elNodeID elf_nodecall(elFileState *fs, elf_lineid line, elNodeID x, elNodeID *z);


elf_tag elf_nodettotag(elNodeTy ty) {
	switch (ty) {
		case NT_SYS: return TAG_SYS;
		case NT_NUM: return TAG_NUM;
		case NT_INT: return TAG_INT;
		default: elf_unreachable;
	}
	return TAG_NIL;
}
