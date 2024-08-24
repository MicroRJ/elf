/*
** See Copyright Notice In elf.h
** node.c
*/



Node get_node(FileState *fs, NodeId id) {
	ASSERT(id != NO_SLOT);
	return fs->nodes[id];
}


NodeKi get_node_kind(FileState *fs, NodeId id) {
	return get_node(fs,id).kind;
}


NodeTy get_node_type(FileState *fs, NodeId id) {
	return get_node(fs,id).type;
}


char *get_node_line(FileState *fs, NodeId id) {
	return get_node(fs,id).line;
}


static ByteOP node2byte(NodeKi tt);


static Node get_target_node(FileState *fs, NodeIdGuard id) {
	Node node;

	node=get_node(fs,id.id);
	switch (node.kind) {
		case NODE_TYPEGUARD:
		case NODE_GROUP:
		case NODE_REGION: {
			return get_target_node(fs,NODE(node.x));
		}
		default: {
			return node;
		}
	}
}


static elBool node_is_lvalue(NodeKi kind) {
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


ValueTag node2tag(NodeTy ty) {
	switch (ty) {
		case NT_SYS: return TAG_SYS;
		case NT_NUM: return TAG_NUM;
		case NT_INT: return TAG_INT;
		default: NO_CODE;
	}
	return TAG_NIL;
}


ByteOP node2byte(NodeKi tt) {
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


NodeId node_xyz(FileState *fs, Source line, NodeKi kind, NodeTy type, NodeId x, NodeId y, NodeId *z) {
	NodeId id=fs->nnodes ++;
	ARRAY_GROW(fs->nodes,fs->nnodes-ARRAY_MIN(fs->nodes));
	Node *node=fs->nodes+id;
	node->level=fs->nblocks-1;
	node->line=line;
	node->type=type;
	node->kind=kind;
	node->x=x;
	node->y=y;
	node->z=z;
	return id;
}


NodeId node_xy(FileState *fs, Source line, NodeKi k, NodeTy t, NodeId x, NodeId y) {
	return node_xyz(fs,line,k,t,x,y,0);
}


NodeId node_x(FileState *fs, Source line, NodeKi k, NodeTy t, NodeId x) {
	return node_xy(fs,line,k,t,x,NO_NODE);
}


NodeId node_nullary(FileState *fs, Source line, NodeKi k, NodeTy t) {
	return node_x(fs,line,k,t,NO_NODE);
}


NodeId node_store(FileState *fs, Source line, NodeId x, NodeId y) {
	return node_xy(fs,line,NODE_STORE,NT_ANY,x,y);
}


NodeId node_type_guard(FileState *fs, Source line, NodeId x, NodeTy y) {
	return node_xy(fs,line,NODE_TYPEGUARD,y,x,y);
}


NodeId node_group(FileState *fs, Source line, NodeId x) {
	NodeId id = node_x(fs,line,NODE_GROUP,get_node_type(fs,x),x);
	return id;
}


NodeId node_integer(FileState *fs, Source line, elInteger i) {
	NodeId v = node_nullary(fs,line,NODE_INTEGER,NT_INT);
	fs->nodes[v].lit.i = i;
	return v;
}


NodeId node_number(FileState *fs, Source line, elNumber n) {
	NodeId v = node_nullary(fs,line,NODE_NUMBER,NT_NUM);
	fs->nodes[v].lit.n = n;
	return v;
}


NodeId node_string(FileState *fs, Source line, char *s) {
	NodeId v = node_nullary(fs,line,NODE_STRING,NT_STR);
	fs->nodes[v].lit.s = s;
	return v;
}


NodeId node_new_table(FileState *fs, Source line, NodeId *z) {
	return node_xyz(fs,line,NODE_TABLE,NT_TAB,NO_NODE,NO_NODE,z);
}


NodeId node_new_closure(FileState *fs, Source line, NodeId x, NodeId *z) {
	return node_xyz(fs,line,NODE_CLOSURE,NT_FUN,x,NO_NODE,z);
}


NodeId node_nil(FileState *fs, Source line) {
	return node_nullary(fs,line,NODE_NIL,NT_NIL);
}


NodeId node_closure_value(FileState *fs, Source line, elRegId x) {
	return node_x(fs,line,NODE_CLSVAL,NT_ANY,x);
}


NodeId node_local(FileState *fs, Source line, elRegId x) {
	return node_x(fs,line,NODE_LOCAL,NT_ANY,x);
}


NodeId node_this(FileState *fs, Source line) {
	return node_local(fs,line,0);
}


NodeId node_global(FileState *fs, Source line, elSymbolId x) {
	return node_x(fs,line,NODE_GLOBAL,NT_ANY,x);
}


NodeId node_field(FileState *fs, Source line, NodeId x, NodeId y) {
	return node_xy(fs,line,NODE_FIELD,NT_ANY,x,y);
}


NodeId node_index(FileState *fs, Source line, NodeId x, NodeId y) {
	return node_xy(fs,line,NODE_INDEX,NT_ANY,x,y);
}


NodeId node_ranged_index(FileState *fs, Source line, NodeId x, NodeId y) {
	return node_xy(fs,line,NODE_RANGE_INDEX,NT_ANY,x,y);
}


NodeId node_metafield(FileState *fs, Source line, NodeId x, NodeId y) {
	return node_xy(fs,line,NODE_METAFIELD,NT_ANY,x,y);
}


NodeId node_call(FileState *fs, Source line, NodeId x, NodeId *z) {
	return node_xyz(fs,line,NODE_CALL,NT_ANY,x,NO_NODE,z);
}


NodeId node_multi(FileState *fs, Source line, NodeId *z) {
	return node_xyz(fs,line,NODE_MULTI,NT_ANY,NO_NODE,NO_NODE,z);
}


NodeId node_less_than(FileState *fs, Source line, NodeId x, NodeId y) {
	return node_xy(fs,line,NODE_LT,NT_BOL,x,y);
}


NodeId node_eq_nil(FileState *fs, Source line, NodeId x) {
	return node_xy(fs,line,NODE_EQ,NT_BOL,x,node_nil(fs,line));
}


NodeId node_call_metafield(FileState *fs, Source line, NodeId x, NodeId *z, char *name) {
	NodeId field = node_metafield(fs,line,x,node_string(fs,line,name));
	return node_call(fs,line,field,z);
}


NodeId node_global_name(FileState *fs, Source line, char *name) {
	elSymbolId x = elf_get_global_symbol(fs->M,elf_new_string(fs->R,name));
	ASSERT(x != -1);
	return node_global(fs,line,x);
}


NodeId node_call_pf(FileState *fs, Source line, NodeId *args) {
	NodeId fn = node_global_name(fs,line,"elf.pf");
	return node_call(fs,line,fn,args);
}


NodeId node_call_set_metatable(FileState *fs, Source line, NodeId object, NodeId metatable) {
	NodeId fn = node_global_name(fs,line,"elf.set_object_metatable");
	NodeId *z = 0;
	ARRAY_ADD(z,object);
	ARRAY_ADD(z,metatable);
	return node_call(fs,line,fn,z);
}


static elBool is_binary_node(NodeKi kind) {
	return kind >= NODE_AND && kind <= NODE_BIT_OR;
}


static void fpf_node(FileState *fs, FILE *io, NodeId id) {
	Node node = get_node(fs,id);
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
		case NODE_INTEGER: fprintf(io,"int(%lli)",node.lit.i); break;
		case NODE_NUMBER: fprintf(io,"num(%f)",node.lit.n); break;
		case NODE_NIL: fprintf(io,"nil"); break;
		case NODE_GROUP: {
			fprintf(io,"(");
			fpf_node(fs,io,node.x);
			fprintf(io,")");
		} break;
		default: fprintf(io,"%s",node2s[node.kind]);
	}
	if (get_node_register(fs,NODE(id)) != NO_SLOT) {
		fprintf(io,"@r%i",get_node_register(fs,NODE(id)));
	}
}


