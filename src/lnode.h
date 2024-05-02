/*
** See Copyright Notice In elf.h
** (Y) lnode.h
** IR?...
*/


#define NO_NODE (-1)


typedef int lnodeid;


typedef enum lnodety {
	NT_ANY, NT_SYS,
	NT_NIL, NT_BOL, NT_INT, NT_NUM,
	NT_OBJ, NT_TAB, NT_FUN, NT_STR
} lnodety;


typedef enum lnodeop {
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
	NODE_BITXOR, NODE_MOD,

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
} lnodeop;


/* -- todo: make this more compact */
typedef struct lNode {
	lnodeop k;
	lnodety t;
	llineid line;
	/* todo: eventually remove this */
	int level;
	/* ---------------------------
	x,y are node inputs/operands,
	if more than 2 are required,
	use z*.
	r represents a tangible memory
	location, used for register
	allocation. */
	struct { lnodeid x,y,*z; };
	llocalid r;
	/* todo?: don't quite union these two for debugging? */
	union {
		char   *s;
		elf_int i;
		elf_num n;
	} lit;
} lNode;


lnodeid elf_nodexyz(elf_FileState *fs, llineid, lnodeop k, lnodety t, lnodeid x, lnodeid y, lnodeid *z);
lnodeid elf_nodebinary(elf_FileState *fs, llineid, lnodeop k, lnodety t, lnodeid x, lnodeid y);
lnodeid elf_nodeunary(elf_FileState *fs, llineid, lnodeop k, lnodety t, lnodeid x);
lnodeid elf_nodenullary(elf_FileState *fs, llineid, lnodeop k, lnodety t);

lnodeid elf_nodegroup(elf_FileState *fs, llineid, lnodeid x);

lnodeid elf_nodenil(elf_FileState *fs, llineid);
lnodeid elf_nodeint(elf_FileState *fs, llineid, elf_int i);
lnodeid elf_nodenum(elf_FileState *fs, llineid, elf_num n);
lnodeid elf_nodestr(elf_FileState *fs, llineid, char *);
lnodeid elf_nodetab(elf_FileState *fs, llineid, lnodeid *z);
lnodeid elf_nodecls(elf_FileState *fs, llineid, lnodeid x, lnodeid *z);

lnodeid elf_nodeload(elf_FileState *fs, llineid line, lnodeid x, lnodeid y);

lnodeid elf_nodelocal(elf_FileState *fs, llineid line, lnodeid i);
lnodeid elf_nodecache(elf_FileState *fs, llineid line, lnodeid i);
lnodeid elf_nodeglobal(elf_FileState *fs, llineid line, lnodeid i);

lnodeid elf_nodetypeguard(elf_FileState *fs, llineid line, lnodeid x, lnodety y);
lnodeid elf_nodemetafield(elf_FileState *fs, llineid line, lnodeid x, lnodeid y);
lnodeid elf_nodefield(elf_FileState *fs, llineid line, lnodeid x, lnodeid y);
lnodeid elf_nodeindex(elf_FileState *fs, llineid line, lnodeid x, lnodeid y);

lnodeid elf_nodeloadfile(elf_FileState *fs, llineid line, lnodeid x);

lnodeid elf_noderangedindex(elf_FileState *fs, llineid line, lnodeid x, lnodeid y);

lnodeid elf_nodebuiltincall(elf_FileState *fs, llineid line, ltokentype k, lnodeid *z);
lnodeid elf_nodecall(elf_FileState *fs, llineid line, lnodeid x, lnodeid *z);


elf_valtag elf_nodettotag(lnodety ty) {
	switch (ty) {
		case NT_SYS: return TAG_SYS;
		case NT_NUM: return TAG_NUM;
		case NT_INT: return TAG_INT;
		default: LNOBRANCH;
	}
	return TAG_NIL;
}
