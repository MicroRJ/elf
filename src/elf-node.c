/*
** See Copyright Notice In elf.h
** elf-node.c
** IR?...
*/



void elf_nodesetr(elFileState *fs, elf_lineid line, elNodeID id, int r) {
	// if (fs->nodes[id]._r != -1) {
		// elf_filediag(fs,line,"resetting register to %i",r);
	// }
	fs->nodes[id]._r = r;
}


elNodeID elf_nodexyz(elFileState *fs, elf_lineid line, elNodeOP k, elNodeTy t, elNodeID x, elNodeID y, elNodeID *z) {
	if (elf_varmin(fs->nodes) <= fs->nnodes) {
		elf_varaddi(fs->nodes,1);
	}
	elNode *nd = fs->nodes + fs->nnodes;
	nd->level = fs->level;
	nd->line = line;
	nd->t = t; nd->k = k;
	/* todo: temporary */
	nd->_r = NO_SLOT;
	if (k == NODE_LOCAL) nd->_r = x;

	nd->x = x; nd->y = y; nd->z = z;
	return fs->nnodes ++;
}


elNodeID elf_nodebinary(elFileState *fs, elf_lineid line, elNodeOP k, elNodeTy t, elNodeID x, elNodeID y) {
	return elf_nodexyz(fs,line,k,t,x,y,elNIL);
}


elNodeID elf_nodeunary(elFileState *fs, elf_lineid line, elNodeOP k, elNodeTy t, elNodeID x) {
	return elf_nodebinary(fs,line,k,t,x,NO_NODE);
}


elNodeID elf_nodenullary(elFileState *fs, elf_lineid line, elNodeOP k, elNodeTy t) {
	return elf_nodeunary(fs,line,k,t,NO_NODE);
}


elNodeID elf_nodeload(elFileState *fs, elf_lineid line, elNodeID x, elNodeID y) {
	return elf_nodebinary(fs,line,NODE_LOAD,NT_ANY,x,y);
}


elNodeID elf_nodetypeguard(elFileState *fs, elf_lineid line, elNodeID x, elNodeTy y) {
	elNodeID id = elf_nodebinary(fs,line,NODE_TYPEGUARD,y,x,y);
	/* todo: could we do this better! maybe we have
	a specific function that checks for these sort
	of nodes, like groups or typeguards,
	additionally, it can be an extra safety layer? */
	elf_nodesetr(fs,line,id,fs->nodes[x]._r);
	// fs->nodes[id].r = fs->nodes[x].r;
	return id;
}


elNodeID elf_nodegroup(elFileState *fs, elf_lineid line, elNodeID x) {
	elNodeID id = elf_nodeunary(fs,line,NODE_GROUP,fs->nodes[x].t,x);
	elf_nodesetr(fs,line,id,fs->nodes[x]._r);
	return id;
}


elNodeID elf_nodeint(elFileState *fs, elf_lineid line, elInteger i) {
	elNodeID v = elf_nodenullary(fs,line,NODE_INTEGER,NT_INT);
	fs->nodes[v].lit.i = i;
	return v;
}


elNodeID elf_nodenum(elFileState *fs, elf_lineid line, elNumber n) {
	elNodeID v = elf_nodenullary(fs,line,NODE_NUMBER,NT_NUM);
	fs->nodes[v].lit.n = n;
	return v;
}


elNodeID elf_nodestr(elFileState *fs, elf_lineid line, char *s) {
	elNodeID v = elf_nodenullary(fs,line,NODE_STRING,NT_STR);
	fs->nodes[v].lit.s = s;
	return v;
}


elNodeID elf_nodetab(elFileState *fs, elf_lineid line, elNodeID *z) {
	return elf_nodexyz(fs,line,NODE_TABLE,NT_TAB,NO_NODE,NO_NODE,z);
}


elNodeID elf_nodecls(elFileState *fs, elf_lineid line, elNodeID x, elNodeID *z) {
	return elf_nodexyz(fs,line,NODE_CLOSURE,NT_FUN,x,NO_NODE,z);
}


elNodeID elf_nodenil(elFileState *fs, elf_lineid line) {
	return elf_nodenullary(fs,line,NODE_NIL,NT_NIL);
}


elNodeID elf_nodeclsval(elFileState *fs, elf_lineid line, elf_localid x) {
	return elf_nodeunary(fs,line,NODE_CLSVAL,NT_ANY,x);
}


elNodeID elf_nodelocal(elFileState *fs, elf_lineid line, elf_localid x) {
	return elf_nodeunary(fs,line,NODE_LOCAL,NT_ANY,x);
}


elNodeID elf_nodeglobal(elFileState *fs, elf_lineid line, elf_globalid x) {
	return elf_nodeunary(fs,line,NODE_GLOBAL,NT_ANY,x);
}


elNodeID elf_nodefield(elFileState *fs, elf_lineid line, elNodeID x, elNodeID y) {
	return elf_nodebinary(fs,line,NODE_FIELD,NT_ANY,x,y);
}


elNodeID elf_nodeindex(elFileState *fs, elf_lineid line, elNodeID x, elNodeID y) {
	return elf_nodebinary(fs,line,NODE_INDEX,NT_ANY,x,y);
}


elNodeID elf_noderangedindex(elFileState *fs, elf_lineid line, elNodeID x, elNodeID y) {
	return elf_nodebinary(fs,line,NODE_RANGE_INDEX,NT_ANY,x,y);
}


elNodeID elf_nodemetafield(elFileState *fs, elf_lineid line, elNodeID x, elNodeID y) {
	return elf_nodebinary(fs,line,NODE_METAFIELD,NT_ANY,x,y);
}


elNodeID elf_nodecall(elFileState *fs, elf_lineid line, elNodeID x, elNodeID *z) {
	return elf_nodexyz(fs,line,NODE_CALL,NT_ANY,x,NO_NODE,z);
}


elNodeID elf_nodeloadfile(elFileState *fs, elf_lineid line, elNodeID x) {
	return elf_nodeunary(fs,line,NODE_FILE,NT_ANY,x);
}


elNodeID elf_nodebuiltincall(elFileState *fs, elf_lineid line, ltokentype k, elNodeID *z) {
	return elf_nodexyz(fs,line,NODE_BUILTIN,NT_ANY,k,NO_NODE,z);
}


elValue elf_nodetolitval(elFileState *fs, elNodeID id);


void elf_nodelitapply(elFileState *fs, elTable *tab, elNodeID id) {
	elNode v = fs->nodes[id];
	switch (v.k) {
		case NODE_LOAD: {
			elNode x = fs->nodes[v.x];
			if (x.k == NODE_LOCAL) {
				elf_unreachable;
			} else
			if ((x.k == NODE_FIELD) || (x.k == NODE_INDEX)) {
				elValue key = elf_nodetolitval(fs,x.x);
				elValue val = elf_nodetolitval(fs,v.y);
				elf_tabset(tab,key,val);
			} else elf_unreachable;
		} break;
		default: elf_unreachable;
	}
}


elValue elf_nodetolitval(elFileState *fs, elNodeID id) {
	elNode nd = fs->nodes[id];
	switch (nd.k) {
		case NODE_INTEGER: {
			return elf_valint(nd.lit.i);
		}
		case NODE_NUMBER: {
			return elf_valnum(nd.lit.n);
		}
		case NODE_TABLE: {
			elTable *tab = elf_newtab(fs->R);
			elf_arrfori(nd.z) {
				elf_nodelitapply(fs,tab,nd.z[i]);
			}
			return elf_valsys(tab);
		}
		default: elf_unreachable;
	}
	return (elValue){TAG_NIL};
}