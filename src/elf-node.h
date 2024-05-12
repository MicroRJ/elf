/*
** See Copyright Notice In elf.h
** elf-node.h
** IR?...
*/


#define NO_NODE (-1)


typedef int elf_nodeid;


typedef enum elf_nodety {
	NT_ANY, NT_SYS,
	NT_NIL, NT_BOL, NT_INT, NT_NUM,
	NT_OBJ, NT_TAB, NT_FUN, NT_STR
} elf_nodety;


typedef enum elf_nodeop {
	NODE_NONE = 0,
	NODE_NOP  = 1,
	/* -- 4.9.24 ------------------------------------
	the following are set up in counterpart pairs,
	this enables us to easily derive their opposite
	operation by simply applying the (^1) operation.
	must be even odd pairs and must start on an even
	value - learned this trick from Mike Pall @ LuaJIT */
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
	/* l-values, represent some address,
	source is x */
	NODE_GLOBAL, NODE_LOCAL, NODE_CACHE,
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
} elf_nodeop;


/* -- todo: make this more compact */
typedef struct elf_Node {
	elf_nodeop k;
	elf_nodety t;
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
	struct { elf_nodeid x,y,*z; };
	elf_localid r;
	/* todo?: don't quite union these two for debugging? */
	union {
		char   *s;
		elf_int i;
		elf_num n;
	} lit;
} elf_Node;


elf_nodeid elf_nodexyz(elf_FileState *fs, elf_lineid, elf_nodeop k, elf_nodety t, elf_nodeid x, elf_nodeid y, elf_nodeid *z);
elf_nodeid elf_nodebinary(elf_FileState *fs, elf_lineid, elf_nodeop k, elf_nodety t, elf_nodeid x, elf_nodeid y);
elf_nodeid elf_nodeunary(elf_FileState *fs, elf_lineid, elf_nodeop k, elf_nodety t, elf_nodeid x);
elf_nodeid elf_nodenullary(elf_FileState *fs, elf_lineid, elf_nodeop k, elf_nodety t);

elf_nodeid elf_nodegroup(elf_FileState *fs, elf_lineid, elf_nodeid x);

elf_nodeid elf_nodenil(elf_FileState *fs, elf_lineid);
elf_nodeid elf_nodeint(elf_FileState *fs, elf_lineid, elf_int i);
elf_nodeid elf_nodenum(elf_FileState *fs, elf_lineid, elf_num n);
elf_nodeid elf_nodestr(elf_FileState *fs, elf_lineid, char *);
elf_nodeid elf_nodetab(elf_FileState *fs, elf_lineid, elf_nodeid *z);
elf_nodeid elf_nodecls(elf_FileState *fs, elf_lineid, elf_nodeid x, elf_nodeid *z);

elf_nodeid elf_nodeload(elf_FileState *fs, elf_lineid line, elf_nodeid x, elf_nodeid y);

elf_nodeid elf_nodelocal(elf_FileState *fs, elf_lineid line, elf_nodeid i);
elf_nodeid elf_nodecache(elf_FileState *fs, elf_lineid line, elf_nodeid i);
elf_nodeid elf_nodeglobal(elf_FileState *fs, elf_lineid line, elf_nodeid i);

elf_nodeid elf_nodetypeguard(elf_FileState *fs, elf_lineid line, elf_nodeid x, elf_nodety y);
elf_nodeid elf_nodemetafield(elf_FileState *fs, elf_lineid line, elf_nodeid x, elf_nodeid y);
elf_nodeid elf_nodefield(elf_FileState *fs, elf_lineid line, elf_nodeid x, elf_nodeid y);
elf_nodeid elf_nodeindex(elf_FileState *fs, elf_lineid line, elf_nodeid x, elf_nodeid y);

elf_nodeid elf_nodeloadfile(elf_FileState *fs, elf_lineid line, elf_nodeid x);

elf_nodeid elf_noderangedindex(elf_FileState *fs, elf_lineid line, elf_nodeid x, elf_nodeid y);

elf_nodeid elf_nodebuiltincall(elf_FileState *fs, elf_lineid line, ltokentype k, elf_nodeid *z);
elf_nodeid elf_nodecall(elf_FileState *fs, elf_lineid line, elf_nodeid x, elf_nodeid *z);


elf_tag elf_nodettotag(elf_nodety ty) {
	switch (ty) {
		case NT_SYS: return TAG_SYS;
		case NT_NUM: return TAG_NUM;
		case NT_INT: return TAG_INT;
		default: elf_unreachable;
	}
	return TAG_NIL;
}
