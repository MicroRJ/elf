/*
** See Copyright Notice In elf.h
** elf-node.c
** IR?...
*/


elNode elf_get_node(elFileState *fs, elNodeId id) {
	return fs->nodes[id];
}


elNodeKi elf_get_node_kind(elFileState *fs, elNodeId id) {
	elf_ensure(id != NO_SLOT);
	return fs->nodes[id].kind;
}


elNodeTy elf_get_node_type(elFileState *fs, elNodeId id) {
	elf_ensure(id != NO_SLOT);
	return fs->nodes[id].type;
}


elFileLine elf_get_node_line(elFileState *fs, elNodeId id) {
	elf_ensure(id != NO_SLOT);
	return fs->nodes[id].line;
}


elBool elf_is_binary_node(elNodeKi kind) {
	return kind >= NODE_AND && kind <= NODE_BITOR;
}


elRegId elf_get_node_register(elFileState *fs, elNodeIdTypeGuard id_type);

void elf_node_fpf(elFileState *fs, FILE *io, elNodeId id) {
	elNode node = elf_get_node(fs,id);
	if (elf_is_binary_node(node.kind)) {
		fprintf(io, "(%s ", elNodeToStr[node.kind]);
		elf_node_fpf(fs,io,node.x);
		fprintf(io, ", ");
		elf_node_fpf(fs,io,node.y);
		fprintf(io, ")");
	} else switch (node.kind) {
		case NODE_INDEX: {
			elf_node_fpf(fs,io,node.x);
			fprintf(io, "[");
			elf_node_fpf(fs,io,node.y);
			fprintf(io, "]");
		} break;
		case NODE_INTEGER: fprintf(io,"int(%lli)",node.lit.i); break;
		case NODE_NUMBER: fprintf(io,"num(%f)",node.lit.n); break;
		case NODE_NIL: fprintf(io,"nil"); break;
		case NODE_GROUP: {
			fprintf(io,"(");
			elf_node_fpf(fs,io,node.x);
			fprintf(io,")");
		} break;
		default: fprintf(io,"%s",elNodeToStr[node.kind]);
	}
	if (elf_get_node_register(fs,MAKE_NODE_ID(id)) != NO_SLOT) {
		fprintf(io,"@r%i",elf_get_node_register(fs,MAKE_NODE_ID(id)));
	}
}


elNodeId elf_make_node_xyz(elFileState *fs, elFileLine line, elNodeKi k, elNodeTy t, elNodeId x, elNodeId y, elNodeId *z) {
	if (elf_varmin(fs->nodes) <= fs->nnodes) {
		elf_xarray_growby(fs->nodes,1);
	}
	elNode *nd = fs->nodes + fs->nnodes;
	nd->level = fs->level;
	nd->line = line;
	nd->t = t, nd->k = k;
	/* todo: temporary */
	// nd->x_r = NO_SLOT;
	// if (k == NODE_LOCAL) nd->x_r = x;

	nd->x = x, nd->y = y, nd->z = z;
	return fs->nnodes ++;
}


elNodeId elf_make_binary_node(elFileState *fs, elFileLine line, elNodeKi k, elNodeTy t, elNodeId x, elNodeId y) {
	return elf_make_node_xyz(fs,line,k,t,x,y,elNil);
}


elNodeId elf_make_node_unary(elFileState *fs, elFileLine line, elNodeKi k, elNodeTy t, elNodeId x) {
	return elf_make_binary_node(fs,line,k,t,x,NO_NODE);
}


elNodeId elf_make_node_nullary(elFileState *fs, elFileLine line, elNodeKi k, elNodeTy t) {
	return elf_make_node_unary(fs,line,k,t,NO_NODE);
}


elNodeId elf_make_load_node(elFileState *fs, elFileLine line, elNodeId x, elNodeId y) {
	return elf_make_binary_node(fs,line,NODE_LOAD,NT_ANY,x,y);
}


elNodeId elf_make_type_guard_node(elFileState *fs, elFileLine line, elNodeId x, elNodeTy y) {
	return elf_make_binary_node(fs,line,NODE_TYPEGUARD,y,x,y);
}


elNodeId elf_make_region_node(elFileState *fs, elFileLine line, elNodeId x, elNodeId *z) {
	return elf_make_node_xyz(fs,line,NODE_REGION,elf_get_node_type(fs,x),x,NO_NODE,z);
}


elNodeId elf_make_group_node(elFileState *fs, elFileLine line, elNodeId x) {
	elNodeId id = elf_make_node_unary(fs,line,NODE_GROUP,elf_get_node_type(fs,x),x);
	return id;
}


elNodeId elf_make_integer_node(elFileState *fs, elFileLine line, elInteger i) {
	elNodeId v = elf_make_node_nullary(fs,line,NODE_INTEGER,NT_INT);
	fs->nodes[v].lit.i = i;
	return v;
}


elNodeId elf_make_number_node(elFileState *fs, elFileLine line, elNumber n) {
	elNodeId v = elf_make_node_nullary(fs,line,NODE_NUMBER,NT_NUM);
	fs->nodes[v].lit.n = n;
	return v;
}


elNodeId elf_make_string_node(elFileState *fs, elFileLine line, char *s) {
	elNodeId v = elf_make_node_nullary(fs,line,NODE_STRING,NT_STR);
	fs->nodes[v].lit.s = s;
	return v;
}


elNodeId elf_make_table_node(elFileState *fs, elFileLine line, elNodeId *z) {
	return elf_make_node_xyz(fs,line,NODE_TABLE,NT_TAB,NO_NODE,NO_NODE,z);
}


elNodeId elf_make_closure_node(elFileState *fs, elFileLine line, elNodeId x, elNodeId *z) {
	return elf_make_node_xyz(fs,line,NODE_CLOSURE,NT_FUN,x,NO_NODE,z);
}


elNodeId elf_make_nil_node(elFileState *fs, elFileLine line) {
	return elf_make_node_nullary(fs,line,NODE_NIL,NT_NIL);
}


elNodeId elf_make_closure_value_node(elFileState *fs, elFileLine line, elRegId x) {
	return elf_make_node_unary(fs,line,NODE_CLOSURE_VALUE,NT_ANY,x);
}


elNodeId elf_make_register_node(elFileState *fs, elFileLine line, elRegId x) {
	return elf_make_node_unary(fs,line,NODE_LOCAL,NT_ANY,x);
}


elNodeId elf_make_global_value_node(elFileState *fs, elFileLine line, elSymbolId x) {
	return elf_make_node_unary(fs,line,NODE_GLOBAL,NT_ANY,x);
}


elNodeId elf_make_field_node(elFileState *fs, elFileLine line, elNodeId x, elNodeId y) {
	return elf_make_binary_node(fs,line,NODE_FIELD,NT_ANY,x,y);
}


elNodeId elf_make_index_node(elFileState *fs, elFileLine line, elNodeId x, elNodeId y) {
	return elf_make_binary_node(fs,line,NODE_INDEX,NT_ANY,x,y);
}


elNodeId elf_make_ranged_index_node(elFileState *fs, elFileLine line, elNodeId x, elNodeId y) {
	return elf_make_binary_node(fs,line,NODE_RANGE_INDEX,NT_ANY,x,y);
}


elNodeId elf_make_metafield_node(elFileState *fs, elFileLine line, elNodeId x, elNodeId y) {
	return elf_make_binary_node(fs,line,NODE_METAFIELD,NT_ANY,x,y);
}


elNodeId elf_make_call_node(elFileState *fs, elFileLine line, elNodeId x, elNodeId *z) {
	return elf_make_node_xyz(fs,line,NODE_CALL,NT_ANY,x,NO_NODE,z);
}

elNodeId elf_make_multi_node(elFileState *fs, elFileLine line, elNodeId *z) {
	return elf_make_node_xyz(fs,line,NODE_MULTI,NT_ANY,NO_NODE,NO_NODE,z);
}

elNodeId elf_make_node_less_than(elFileState *fs, elFileLine line, elNodeId x, elNodeId y) {
	return elf_make_binary_node(fs,line,NODE_LT,NT_BOL,x,y);
}

elNodeId elf_make_node_is_nil(elFileState *fs, elFileLine line, elNodeId x) {
	return elf_make_binary_node(fs,line,NODE_EQ,NT_BOL,x,elf_make_nil_node(fs,line));
}

elNodeId elf_make_call_metafield_node(elFileState *fs, elFileLine line, elNodeId x, elNodeId *z, char *name) {
	elNodeId field = elf_make_metafield_node(fs,line,x,elf_make_string_node(fs,line,name));
	return elf_make_call_node(fs,line,field,z);
}

