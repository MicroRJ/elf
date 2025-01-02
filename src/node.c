/*
** See Copyright Notice In elf.h
** node.c
*/



#define NODE_ENUM(NAME) #NAME,
INTERNAL char *node2s[] = {
	"NONE",
	TREEDEF(NODE_ENUM)
};
#undef NODE_ENUM




Tree get_node(Parser *fs, TreeId id) {
	ASSERT(id != NO_SLOT);
	return fs->nodes[id];
}


TreeKi get_node_kind(Parser *fs, TreeId id) {
	return get_node(fs,id).kind;
}


TreeTy get_node_type(Parser *fs, TreeId id) {
	return get_node(fs,id).type;
}


char *get_node_line(Parser *fs, TreeId id) {
	return get_node(fs,id).line;
}


static ByteOP node2byte(TreeKi tt);


static Tree get_target_node(Parser *fs, NodeIdGuard id) {
	Tree node;

	node=get_node(fs,id.id);
	switch (node.kind) {
		case NODE_TYPEGUARD:
		case NODE_GROUP: {
			return get_target_node(fs,NODE(node.x));
		}
		default: {
			return node;
		}
	}
}


static elf_Bool node_is_lvalue(TreeKi kind) {
	switch (kind) {
		case NODE_RANGE_INDEX:
		case NODE_GLOBAL:
		case NODE_LOCAL:
		case NODE_INDEX:
		case NODE_FIELD: {
			return 1;
		}
		default: {
			return 0;
		}
	}
}


elf_ValueTag node2tag(TreeTy ty) {
	switch (ty) {
		case NT_SYS: return elf_TAG_SYS;
		case NT_NUM: return elf_TAG_NUM;
		case NT_INT: return elf_TAG_INT;
		default: NO_CODE;
	}
	return elf_TAG_NIL;
}


ByteOP node2byte(TreeKi tt) {
	switch (tt) {
		case NODE_FIELD: 		return BC_GETFIELD;
		case NODE_INDEX: 		return BC_GETINDEX;
		case NODE_METAFIELD: return BC_GETMETAFIELD;
		case NODE_CALL: return BC_CALL;
		case NODE_ADD: return BC_ADD;
		case NODE_SUB: return BC_SUB;
		case NODE_DIV: return BC_DIV;
		case NODE_MUL: return BC_MUL;
		case NODE_POW: return BC_POW;
		case NODE_MOD: return BC_MOD;
		case NODE_NEQ: return BC_NEQ;
		case NODE_EQ: return BC_EQ;
		case NODE_LT: return BC_LT;
		case NODE_LTEQ: return BC_LTEQ;
		case NODE_BIT_OR: return BC_BIT_OR;
		case NODE_BIT_AND: return BC_BIT_AND;
		case NODE_BIT_SHL: return BC_SHL;
		case NODE_BIT_SHR: return BC_SHR;
		case NODE_BIT_XOR: return BC_BIT_XOR;
		/* given the intended use cases, this is an error */
		default: NO_CODE;
	}
	return BC_HALT;
}


TreeId node_xyz(Parser *fs, Source line, TreeKi kind, TreeTy type, TreeId x, TreeId y, TreeId *z) {
	TreeId id=fs->nnodes ++;
	ARRAY_GROW(fs->nodes,fs->nnodes-ARRAY_GET_MIN(fs->nodes));
	Tree *node=fs->nodes+id;
	node->level=fs->nblocks-1;
	node->line=line;
	node->type=type;
	node->kind=kind;
	node->x=x;
	node->y=y;
	node->z=z;
	return id;
}


TreeId node_xy(Parser *fs, Source line, TreeKi k, TreeTy t, TreeId x, TreeId y) {
	return node_xyz(fs,line,k,t,x,y,0);
}


TreeId node_x(Parser *fs, Source line, TreeKi k, TreeTy t, TreeId x) {
	return node_xy(fs,line,k,t,x,NO_NODE);
}


TreeId node_nullary(Parser *fs, Source line, TreeKi k, TreeTy t) {
	return node_x(fs,line,k,t,NO_NODE);
}


TreeId node_store(Parser *fs, Source line, TreeId x, TreeId y) {
	return node_xy(fs,line,NODE_STORE,NT_ANY,x,y);
}


TreeId node_type_guard(Parser *fs, Source line, TreeId x, TreeTy y) {
	return node_xy(fs,line,NODE_TYPEGUARD,y,x,y);
}


TreeId node_group(Parser *fs, Source line, TreeId x) {
	TreeId id = node_x(fs,line,NODE_GROUP,get_node_type(fs,x),x);
	return id;
}


TreeId node_integer(Parser *fs, Source line, elf_Int i) {
	TreeId v = node_nullary(fs,line,NODE_INTEGER,NT_INT);
	fs->nodes[v].i = i;
	return v;
}


TreeId node_number(Parser *fs, Source line, elf_Num n) {
	TreeId v = node_nullary(fs,line,NODE_NUMBER,NT_NUM);
	fs->nodes[v].n = n;
	return v;
}


TreeId node_string(Parser *fs, Source line, char *s) {
	TreeId v = node_nullary(fs,line,NODE_STRING,NT_STR);
	fs->nodes[v].s = s;
	return v;
}


TreeId node_new_table(Parser *fs, Source line, TreeId *z) {
	return node_xyz(fs,line,NODE_TABLE,NT_TAB,NO_NODE,NO_NODE,z);
}


TreeId node_new_closure(Parser *fs, Source line, TreeId x, TreeId *z) {
	return node_xyz(fs,line,NODE_CLOSURE,NT_FUN,x,NO_NODE,z);
}


TreeId node_nil(Parser *fs, Source line) {
	return node_nullary(fs,line,NODE_NIL,NT_NIL);
}


TreeId node_closure_value(Parser *fs, Source line, elf_StackId x) {
	return node_x(fs,line,NODE_CLSVAL,NT_ANY,x);
}


TreeId node_local(Parser *fs, Source line, elf_StackId x) {
	return node_x(fs,line,NODE_LOCAL,NT_ANY,x);
}


TreeId node_this(Parser *fs, Source line) {
	return node_local(fs,line,0);
}


TreeId node_global(Parser *fs, Source line, elf_SymbolId x) {
	return node_x(fs,line,NODE_GLOBAL,NT_ANY,x);
}


TreeId node_field(Parser *fs, Source line, TreeId x, TreeId y) {
	return node_xy(fs,line,NODE_FIELD,NT_ANY,x,y);
}


TreeId node_index(Parser *fs, Source line, TreeId x, TreeId y) {
	return node_xy(fs,line,NODE_INDEX,NT_ANY,x,y);
}


TreeId node_ranged_index(Parser *fs, Source line, TreeId x, TreeId y) {
	return node_xy(fs,line,NODE_RANGE_INDEX,NT_ANY,x,y);
}


TreeId node_metafield(Parser *fs, Source line, TreeId x, TreeId y) {
	return node_xy(fs,line,NODE_METAFIELD,NT_ANY,x,y);
}


TreeId node_call(Parser *fs, Source line, TreeId x, TreeId *z) {
	return node_xyz(fs,line,NODE_CALL,NT_ANY,x,NO_NODE,z);
}


TreeId node_multi(Parser *fs, Source line, TreeId *z) {
	return node_xyz(fs,line,NODE_MULTI,NT_ANY,NO_NODE,NO_NODE,z);
}


TreeId node_less_than(Parser *fs, Source line, TreeId x, TreeId y) {
	return node_xy(fs,line,NODE_LT,NT_BOL,x,y);
}


TreeId node_eq_nil(Parser *fs, Source line, TreeId x) {
	return node_xy(fs,line,NODE_EQ,NT_BOL,x,node_nil(fs,line));
}


TreeId node_call_metafield(Parser *fs, Source line, TreeId x, TreeId *z, char *name) {
	TreeId field = node_metafield(fs,line,x,node_string(fs,line,name));
	return node_call(fs,line,field,z);
}


TreeId node_global_name(Parser *fs, Source line, char *name) {
	elf_SymbolId x = elf_get_global(fs->M,elf_alloc_string(fs->R,name));
	ASSERT(x != -1);
	return node_global(fs,line,x);
}


TreeId node_call_pf(Parser *fs, Source line, TreeId *args) {
	TreeId fn = node_global_name(fs,line,"elf.pf");
	return node_call(fs,line,fn,args);
}


TreeId node_call_set_metatable(Parser *fs, Source line, TreeId object, TreeId metatable) {
	TreeId fn = node_global_name(fs,line,"elf.set_object_metatable");
	TreeId *z = 0;
	ARRAY_ADD(z,object);
	ARRAY_ADD(z,metatable);
	return node_call(fs,line,fn,z);
}


static elf_Bool is_binary_node(TreeKi kind) {
	return kind >= NODE_AND && kind <= NODE_BIT_OR;
}


static void fpf_node(Parser *fs, FILE *io, TreeId id) {
	Tree node = get_node(fs,id);
	if (is_binary_node(node.kind)) {
		fprintf(io, "(%s ", node2s[node.kind]);
		fpf_node(fs,io,node.x);
		fprintf(io, ", ");
		fpf_node(fs,io,node.y);
		fprintf(io, ")");
	} else switch (node.kind) {
		case NODE_INDEX: {
			fpf_node(fs,io,node.x);
			fprintf(io, "[");
			fpf_node(fs,io,node.y);
			fprintf(io, "]");
		} break;
		case NODE_INTEGER: fprintf(io,"int(%lli)",node.i); break;
		case NODE_NUMBER: fprintf(io,"num(%f)",node.n); break;
		case NODE_NIL: fprintf(io,"nil"); break;
		case NODE_GROUP: {
			fprintf(io,"(");
			fpf_node(fs,io,node.x);
			fprintf(io,")");
		} break;
		default: fprintf(io,"%s",node2s[node.kind]);
	}
}


