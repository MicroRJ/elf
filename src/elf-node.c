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


elf_lineid elf_get_node_line(elFileState *fs, elNodeId id) {
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


elNodeId elf_make_node_xyz(elFileState *fs, elf_lineid line, elNodeKi k, elNodeTy t, elNodeId x, elNodeId y, elNodeId *z) {
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


elNodeId elf_make_binary_node(elFileState *fs, elf_lineid line, elNodeKi k, elNodeTy t, elNodeId x, elNodeId y) {
	return elf_make_node_xyz(fs,line,k,t,x,y,elNil);
}


elNodeId elf_make_node_unary(elFileState *fs, elf_lineid line, elNodeKi k, elNodeTy t, elNodeId x) {
	return elf_make_binary_node(fs,line,k,t,x,NO_NODE);
}


elNodeId elf_make_node_nullary(elFileState *fs, elf_lineid line, elNodeKi k, elNodeTy t) {
	return elf_make_node_unary(fs,line,k,t,NO_NODE);
}


elNodeId elf_make_load_node(elFileState *fs, elf_lineid line, elNodeId x, elNodeId y) {
	return elf_make_binary_node(fs,line,NODE_LOAD,NT_ANY,x,y);
}


elNodeId elf_make_type_guard_node(elFileState *fs, elf_lineid line, elNodeId x, elNodeTy y) {
	elNodeId id = elf_make_binary_node(fs,line,NODE_TYPEGUARD,y,x,y);
	/* todo: could we do this better! maybe we have
	a specific function that checks for these sort
	of nodes, like groups or typeguards,
	additionally, it can be an extra safety layer? */
	// elf_set_node_register(fs,line,id,elf_get_node_register(fs,x));
	// fs->nodes[id].r = fs->nodes[x].r;
	return id;
}


elNodeId elf_make_region_node(elFileState *fs, elf_lineid line, elNodeId x, elNodeId *z) {
	elNodeId id = elf_make_node_xyz(fs,line,NODE_REGION,fs->nodes[x].ty,x,NO_NODE,z);
	// elf_set_node_register(fs,line,id,elf_get_node_register(fs,x));
	return id;
}


elNodeId elf_make_group_node(elFileState *fs, elf_lineid line, elNodeId x) {
	elNodeId id = elf_make_node_unary(fs,line,NODE_GROUP,fs->nodes[x].t,x);
	// elf_set_node_register(fs,line,id,elf_get_node_register(fs,x));
	return id;
}


elNodeId elf_make_node_integer(elFileState *fs, elf_lineid line, elInteger i) {
	elNodeId v = elf_make_node_nullary(fs,line,NODE_INTEGER,NT_INT);
	fs->nodes[v].lit.i = i;
	return v;
}


elNodeId elf_make_number_node(elFileState *fs, elf_lineid line, elNumber n) {
	elNodeId v = elf_make_node_nullary(fs,line,NODE_NUMBER,NT_NUM);
	fs->nodes[v].lit.n = n;
	return v;
}


elNodeId elf_make_string_node(elFileState *fs, elf_lineid line, char *s) {
	elNodeId v = elf_make_node_nullary(fs,line,NODE_STRING,NT_STR);
	fs->nodes[v].lit.s = s;
	return v;
}


elNodeId elf_make_table_node(elFileState *fs, elf_lineid line, elNodeId *z) {
	return elf_make_node_xyz(fs,line,NODE_TABLE,NT_TAB,NO_NODE,NO_NODE,z);
}


elNodeId elf_make_closure_node(elFileState *fs, elf_lineid line, elNodeId x, elNodeId *z) {
	return elf_make_node_xyz(fs,line,NODE_CLOSURE,NT_FUN,x,NO_NODE,z);
}


elNodeId elf_make_nil_node(elFileState *fs, elf_lineid line) {
	return elf_make_node_nullary(fs,line,NODE_NIL,NT_NIL);
}


elNodeId elf_make_closure_value_node(elFileState *fs, elf_lineid line, elRegId x) {
	return elf_make_node_unary(fs,line,NODE_CLOSURE_VALUE,NT_ANY,x);
}


elNodeId elf_make_node_local_register(elFileState *fs, elf_lineid line, elRegId x) {
	return elf_make_node_unary(fs,line,NODE_LOCAL,NT_ANY,x);
}


elNodeId elf_make_global_value_node(elFileState *fs, elf_lineid line, elSymbolId x) {
	return elf_make_node_unary(fs,line,NODE_GLOBAL,NT_ANY,x);
}


elNodeId elf_make_field_node(elFileState *fs, elf_lineid line, elNodeId x, elNodeId y) {
	return elf_make_binary_node(fs,line,NODE_FIELD,NT_ANY,x,y);
}


elNodeId elf_make_index_node(elFileState *fs, elf_lineid line, elNodeId x, elNodeId y) {
	return elf_make_binary_node(fs,line,NODE_INDEX,NT_ANY,x,y);
}


elNodeId elf_make_ranged_index_node(elFileState *fs, elf_lineid line, elNodeId x, elNodeId y) {
	return elf_make_binary_node(fs,line,NODE_RANGE_INDEX,NT_ANY,x,y);
}


elNodeId elf_make_meta_field_node(elFileState *fs, elf_lineid line, elNodeId x, elNodeId y) {
	return elf_make_binary_node(fs,line,NODE_METAFIELD,NT_ANY,x,y);
}


elNodeId elf_make_call_node(elFileState *fs, elf_lineid line, elNodeId x, elNodeId *z) {
	return elf_make_node_xyz(fs,line,NODE_CALL,NT_ANY,x,NO_NODE,z);
}


elNodeId elf_make_node_less_than(elFileState *fs, elf_lineid line, elNodeId x, elNodeId y) {
#if 0
	/* todo: this isn't really necessary, and it will get
	in the way as we introduce operator overloading */
	if (elf_get_node_type(fs,x) != NT_INT) {
		x = elf_make_type_guard_node(fs,elf_get_node_line(fs,x),x,NT_INT);
	}
	if (elf_get_node_type(fs,y) != NT_INT) {
		y = elf_make_type_guard_node(fs,elf_get_node_line(fs,y),y,NT_INT);
	}
#endif
	return elf_make_binary_node(fs,line,NODE_LT,NT_BOL,x,y);
}
