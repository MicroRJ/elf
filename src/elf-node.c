/*
** See Copyright Notice In elf.h
** elf-node.c
** IR?...
*/


elf_nodeid elf_nodexyz(elFileState *fs, elf_lineid line, elf_nodeop k, elf_nodety t, elf_nodeid x, elf_nodeid y, elf_nodeid *z) {
	if (elf_varmin(fs->nodes) <= fs->nnodes) {
		elf_varaddi(fs->nodes,1);
	}
	elf_Node *nd = fs->nodes + fs->nnodes;
	nd->level = fs->level;
	nd->line = line;
	nd->t = t; nd->k = k;
	nd->r = NO_SLOT;
	/* todo: temporary */
	if (k == NODE_LOCAL) nd->r = x;

	nd->x = x; nd->y = y; nd->z = z;
	return fs->nnodes ++;
}


elf_nodeid elf_nodebinary(elFileState *fs, elf_lineid line, elf_nodeop k, elf_nodety t, elf_nodeid x, elf_nodeid y) {
	return elf_nodexyz(fs,line,k,t,x,y,elNIL);
}


elf_nodeid elf_nodeunary(elFileState *fs, elf_lineid line, elf_nodeop k, elf_nodety t, elf_nodeid x) {
	return elf_nodebinary(fs,line,k,t,x,NO_NODE);
}


elf_nodeid elf_nodenullary(elFileState *fs, elf_lineid line, elf_nodeop k, elf_nodety t) {
	return elf_nodeunary(fs,line,k,t,NO_NODE);
}


elf_nodeid elf_nodeload(elFileState *fs, elf_lineid line, elf_nodeid x, elf_nodeid y) {
	return elf_nodebinary(fs,line,NODE_LOAD,NT_ANY,x,y);
}


elf_nodeid elf_nodetypeguard(elFileState *fs, elf_lineid line, elf_nodeid x, elf_nodety y) {
	elf_nodeid id = elf_nodebinary(fs,line,NODE_TYPEGUARD,y,x,y);
	/* todo: could we do this better! maybe we have
	a specific function that checks for these sort
	of nodes, like groups or typeguards,
	additionally, it can be an extra safety layer? */
	fs->nodes[id].r = fs->nodes[x].r;
	return id;
}


elf_nodeid elf_nodegroup(elFileState *fs, elf_lineid line, elf_nodeid x) {
	elf_nodeid id = elf_nodeunary(fs,line,NODE_GROUP,fs->nodes[x].t,x);
	fs->nodes[id].r = fs->nodes[x].r;
	return id;
}


elf_nodeid elf_nodeint(elFileState *fs, elf_lineid line, elInteger i) {
	elf_nodeid v = elf_nodenullary(fs,line,NODE_INTEGER,NT_INT);
	fs->nodes[v].lit.i = i;
	return v;
}


elf_nodeid elf_nodenum(elFileState *fs, elf_lineid line, elNumber n) {
	elf_nodeid v = elf_nodenullary(fs,line,NODE_NUMBER,NT_NUM);
	fs->nodes[v].lit.n = n;
	return v;
}


elf_nodeid elf_nodestr(elFileState *fs, elf_lineid line, char *s) {
	elf_nodeid v = elf_nodenullary(fs,line,NODE_STRING,NT_STR);
	fs->nodes[v].lit.s = s;
	return v;
}


elf_nodeid elf_nodetab(elFileState *fs, elf_lineid line, elf_nodeid *z) {
	return elf_nodexyz(fs,line,NODE_TABLE,NT_TAB,NO_NODE,NO_NODE,z);
}


elf_nodeid elf_nodecls(elFileState *fs, elf_lineid line, elf_nodeid x, elf_nodeid *z) {
	return elf_nodexyz(fs,line,NODE_CLOSURE,NT_FUN,x,NO_NODE,z);
}


elf_nodeid elf_nodenil(elFileState *fs, elf_lineid line) {
	return elf_nodenullary(fs,line,NODE_NIL,NT_NIL);
}


elf_nodeid elf_nodeclsval(elFileState *fs, elf_lineid line, elf_localid x) {
	return elf_nodeunary(fs,line,NODE_CLSVAL,NT_ANY,x);
}


elf_nodeid elf_nodelocal(elFileState *fs, elf_lineid line, elf_localid x) {
	return elf_nodeunary(fs,line,NODE_LOCAL,NT_ANY,x);
}


elf_nodeid elf_nodeglobal(elFileState *fs, elf_lineid line, elf_globalid x) {
	return elf_nodeunary(fs,line,NODE_GLOBAL,NT_ANY,x);
}


elf_nodeid elf_nodefield(elFileState *fs, elf_lineid line, elf_nodeid x, elf_nodeid y) {
	return elf_nodebinary(fs,line,NODE_FIELD,NT_ANY,x,y);
}


elf_nodeid elf_nodeindex(elFileState *fs, elf_lineid line, elf_nodeid x, elf_nodeid y) {
	return elf_nodebinary(fs,line,NODE_INDEX,NT_ANY,x,y);
}


elf_nodeid elf_noderangedindex(elFileState *fs, elf_lineid line, elf_nodeid x, elf_nodeid y) {
	return elf_nodebinary(fs,line,NODE_RANGE_INDEX,NT_ANY,x,y);
}


elf_nodeid elf_nodemetafield(elFileState *fs, elf_lineid line, elf_nodeid x, elf_nodeid y) {
	return elf_nodebinary(fs,line,NODE_METAFIELD,NT_ANY,x,y);
}


elf_nodeid elf_nodecall(elFileState *fs, elf_lineid line, elf_nodeid x, elf_nodeid *z) {
	return elf_nodexyz(fs,line,NODE_CALL,NT_ANY,x,NO_NODE,z);
}


elf_nodeid elf_nodeloadfile(elFileState *fs, elf_lineid line, elf_nodeid x) {
	return elf_nodeunary(fs,line,NODE_FILE,NT_ANY,x);
}


elf_nodeid elf_nodebuiltincall(elFileState *fs, elf_lineid line, ltokentype k, elf_nodeid *z) {
	return elf_nodexyz(fs,line,NODE_BUILTIN,NT_ANY,k,NO_NODE,z);
}


elValue elf_nodetolitval(elFileState *fs, elf_nodeid id);


void elf_nodelitapply(elFileState *fs, elTable *tab, elf_nodeid id) {
	elf_Node v = fs->nodes[id];
	switch (v.k) {
		case NODE_LOAD: {
			elf_Node x = fs->nodes[v.x];
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


elValue elf_nodetolitval(elFileState *fs, elf_nodeid id) {
	elf_Node nd = fs->nodes[id];
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