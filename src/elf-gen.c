/*
** See Copyright Notice In elf.h
** elf-gen.c
** Bytecode Generator (node -> bytecode)
*/


elf_byteop nodetobyte(elNodeOP tt);


elf_byteid elf_getlastbyteid(elFileState *fs) {
	return fs->md->nbytes;
}


elf_localid elf_genlocalalloc(elFileState *fs, elf_localid n) {
	elf_ensure(n > NO_SLOT);

	elFileFnState *fn = fs->fn;

	elf_localid id = fn->xmemory;
	fn->xmemory += n;

	if (fn->nlocals < fn->xmemory) {
		fn->nlocals = fn->xmemory;
	}
	return id;
}


void langL_localdealloc(elFileState *fs, elf_localid x) {
	elf_ensure(x > NO_SLOT);

	elFileFnState *fn = fs->fn;

	elf_ensure(x == fn->xmemory-1);
	fn->xmemory = x;
}


/* note: we just associate each byte with a line,
this is pretty simple and tends to yield excellent
debug info... */
elf_byteid elf_emitbyte(elFileState *fs, elf_lineid line, elf_Bytecode byte) {
	elFileFnState *fn = fs->fn;
	elModule *md = fs->md;
	// elf_bytefpf(md,stdout,md->nbytes-fn->bytes,byte);
	elf_varadd(md->lines,line);
	elf_varadd(md->bytes,byte);
	elf_varadd(md->track,0);
	return md->nbytes ++;
}


elf_byteid elf_emitbyteop(elFileState *fs, elf_lineid line, elf_byteop k, elInteger i) {
	return elf_emitbyte(fs,line,(elf_Bytecode){k,i});
}


elf_byteid elf_emitbytexy(elFileState *fs, elf_lineid line, elf_byteop k, int x, int y) {
	elf_Bytecode b = (elf_Bytecode){k};
	b.x = x;
	b.y = y;
	return elf_emitbyte(fs,line,b);
}


elf_byteid elf_emitbytexyz(elFileState *fs, elf_lineid line, elf_byteop k, int x, int y, int z) {
	elf_Bytecode b = (elf_Bytecode){k};
	b.x = x;
	b.y = y;
	b.z = z;
	return elf_emitbyte(fs,line,b);
}


void langL_tieloosejto(elFileState *fs, elf_byteid id, elf_byteid j) {
	elModule *md = fs->md;
	elf_Bytecode *bytes = md->bytes;
	elf_Bytecode b = bytes[id];

	elf_byteid l = j - id;
	// if (l == NO_JUMP) {
	// 	elf_filediag(fs,md->lines[id],"opt, no jump");
	// }
	switch (b.k) {
		case BC_J:
		case BC_DELAY: {
			bytes[id].i = l;
		} break;
		case BC_JZ:
		case BC_JNZ:
		case BC_YIELD: {
			bytes[id].x = l;
		} break;
		default: elf_unreachable;
	}
}


void langL_tieloosej(elFileState *fs, elf_byteid i) {
	langL_tieloosejto(fs,i,elf_getlastbyteid(fs));
}


void langL_tieloosejs(elFileState *fs, elf_byteid *js) {
	elf_arrfori(js) {
		langL_tieloosej(fs,js[i]);
	}
}


void langL_tieloosejsto(elFileState *fs, elf_byteid *js, elf_byteid j) {
	elf_arrfori(js) {
		langL_tieloosejto(fs,js[i],j);
	}
}


elf_byteid elf_emitjump(elFileState *fs, elf_lineid line, elf_byteid j) {
	return elf_emitbyteop(fs,line,BC_J,j-fs->md->nbytes);
}


void langL_fnepiloge(elFileState *fs, elf_lineid line) {
	elModule *md = fs->md;
	elFileFnState *fn = fs->fn;

	/* todo: this is unncessary, we know were
	the return instruction is at. */
	langL_tieloosejs(fs,fn->yj);
	elf_delvar(fn->yj);
	fn->yj = elNIL;

	/* finally, return control flow */
	elf_emitbyteop(fs,line,BC_LEAVE,0);
}


/*
** Evaluates the given boolean or logical expression using short
** circuit evaluation, creating branches as it evaluates each
** expression, only one register is necessary, if no registers
** are given one is allocated and deallocated automatically.
*/
elf_byteid elf_genbranchif(elFileState *fs, ljlist *js, elBool z, elf_localid x, elNodeID id) {
	elNode v = fs->nodes[id];
	elf_byteid j = NO_BYTE;

	elf_localid mem = fs->fn->xmemory;
	/* -------------------------------
	if not provided one, allocate temporary
	register here, notice how this is done
	before we keep splitting, this will make
	it so subsequent recursive calls to this
	function won't keep allocating registers,
	we get away with using only one register
	due to the transitive nature of short
	circuiting, expressible in the bytecode. */
	if (x == NO_SLOT) x = elf_genlocalalloc(fs,1);

	switch (v.k) {
		case NODE_GROUP: {
			j = elf_genbranchif(fs,js,z,x,v.x);
		} break;
		/* '&&' expressions short-circuits to a false
		jump as early as possible and the opposite
		is true for the '||' expressions, last jump is
		whatever the user specified */
		case NODE_AND: {
			langL_jumpiffalse(fs,js,x,v.x);
			j = elf_genbranchif(fs,js,z,x,v.y);
		} break;
		case NODE_OR: {
			langL_jumpiftrue(fs,js,x,v.x);
			j = elf_genbranchif(fs,js,z,x,v.y);
		} break;
		default: {
			langL_localload(fs,NO_LINE,lfalse,x,1,id);

			if (z != 0) {
				j = elf_emitbytexy(fs,v.line,BC_JNZ,NO_JUMP,x);
				elf_varadd(js->t,j);
			} else {
				j = elf_emitbytexy(fs,v.line,BC_JZ,NO_JUMP,x);
				elf_varadd(js->f,j);
			}
		} break;
	}


	fs->fn->xmemory = mem;
	return j;
}


elf_byteid langL_branchiffalse(elFileState *fs, ljlist *js, elf_localid x, elNodeID id) {
	return elf_genbranchif(fs,js,lfalse,x,id);
}


elf_byteid langL_branchiftrue(elFileState *fs, ljlist *js, elf_localid x, elNodeID id) {
	return elf_genbranchif(fs,js,ltrue,x,id);
}


/*
** Similar to branch if true, but additionally this byte
** address becomes the false target, thus all false
** branches converge here. all true branches are returned.
*/
elf_byteid *langL_jumpiftrue(elFileState *fs, ljlist *js, elf_localid x, elNodeID id) {
	langL_branchiftrue(fs,js,x,id);
	langL_tieloosejs(fs,js->f);
	elf_delvar(js->f);
	js->f = elNIL;
	return js->t;
}


elf_byteid *langL_jumpiffalse(elFileState *fs, ljlist *js, elf_localid x, elNodeID id) {
	langL_branchiffalse(fs,js,x,id);
	langL_tieloosejs(fs,js->t);
	elf_delvar(js->t);
	js->t = elNIL;
	return js->f;
}


elf_byteid *langL_jumpifnotnil(elFileState *fs, elf_lineid line, ljlist *js, elNodeID id) {
	return langL_jumpiffalse(fs,js,NO_SLOT,elf_nodebinary(fs,line,NODE_EQ,NT_BOL,id,elf_nodenil(fs,line)));
}


/* todo: add support for multiple results */
void elf_emityield(elFileState *fs, elf_lineid line, elNodeID id) {
	int n = 0;
	elf_localid x = 0;
	if (id != NO_NODE) {
		/* todo: determine the number of values in
		tree, and allocate that many registers? */
		x = elf_genlocalalloc(fs,n=1);
		langL_localload(fs,line,ltrue,x,n,id);
		langL_localdealloc(fs,x);

		if (fs->fn->nyield < n) fs->fn->nyield = n;
		elf_byteid j = elf_emitbytexyz(fs,line,BC_YIELD,NO_JUMP,x,n);
		elf_varadd(fs->fn->yj,j);

		/* if there are no results then simply leave directly */
	} else elf_emitbyteop(fs,line,BC_LEAVE,0);
}


/*
*/
void elf_gentestmem(elFileState *fs, elf_lineid line, elf_localid r) {
	if ((fs->fn->xmemory - r) != 1) {
		elf_filediag(fs,line,"invalid memory state");
	}
}


/*
** Ensures that the next free local is the
** given one and loads the node to that
** local.
*/
void langL_localloadin(elFileState *fs, elf_lineid line, elf_localid r, elNodeID id) {
	/* the user can provide exactly the next free register,
	or exactly the last allocated register  */
	if ((fs->fn->xmemory - r) == 0) {
		elf_genlocalalloc(fs,1);
	}
	if ((fs->fn->xmemory - r) != 1) {
		elf_filediag(fs,line,"invalid memory state");
	}
	langL_localload(fs,line,ltrue,r,1,id);
}


elf_localid elf_genlocalize(elFileState *fs, elf_lineid line, elNodeID id) {
	elNode v = fs->nodes[id];
	/* todo: this literally contradicts SSA, this system is
	obsolete, replace with something else... */
	elf_localid r = v._r;
	/* the node is currently allocated */
	if ((r != NO_SLOT) && (r < fs->fn->xmemory)) {
		goto leave;
	} else r = elf_genlocalalloc(fs,1);
	langL_localload(fs,line,lfalse,r,1,id);
	leave: return r;
}


void langL_emit(elFileState *fs, elf_lineid line, elNodeID id) {
	elf_localid mem = fs->fn->xmemory;
	elNode v = fs->nodes[id];
	switch (v.k) {
		case NODE_LOAD: {
			elNode x = fs->nodes[v.x];
			if ((x.k == NODE_LOCAL)) {
				elf_unreachable;
			} else
			if ((x.k == NODE_FIELD) || (x.k == NODE_INDEX)) {
				elf_localid xx = elf_genlocalize(fs,line,x.x);
				elf_localid xy = elf_genlocalize(fs,line,x.y);
				elf_localid yy = elf_genlocalize(fs,line,v.y);
				if (x.k == NODE_FIELD) {
					elf_emitbytexyz(fs,line,BC_SETFIELD,xx,xy,yy);
				} else elf_emitbytexyz(fs,line,BC_SETINDEX,xx,xy,yy);
			} else elf_unreachable;
		} break;
		default: elf_unreachable;
	}
	fs->fn->xmemory = mem;
}


/*
** emits bytecode to load id into local x,
** if y is 0 the instruction is omitted if
** no side effects.
*/
void langL_localload(elFileState *fs, elf_lineid line, elBool reload, elf_localid x, elf_localid y, elNodeID id) {
	elf_ensure(x > NO_SLOT);
	elf_ensure(x < fs->fn->xmemory);

	elNode v = fs->nodes[id];
	elf_ensure(v.level <= fs->level);

	if (v.line != 0) line = v.line;

	elModule *md = fs->md;
	elFileFnState *fn = fs->fn;

	/* finally restore memory state */
	elf_localid mem = fs->fn->xmemory;

	if ((v._r != NO_SLOT) && (reload != ltrue)) {
		/* node is already allocated, and we're
		not asked to reload it */
		if (v._r <= mem) goto leave;
		elf_nodesetr(fs,line,id,NO_SLOT); (v._r = NO_SLOT);
	}
	elf_ensure((v._r < mem));
	elf_ensure((v.k != NODE_LOCAL) || (v._r == v.x));


	elf_nodesetr(fs,line,id,x);
	// fs->nodes[id].r = x;


	#define UNUSED_CHECK \
		if (y == 0) {\
			elf_filediag(fs,line,"warning: unused expression");\
			goto leave;\
		}

	/* some instructions have no side effects,
	in which case, if the yield count is 0, the
	instruction is not emitted */
	switch (v.k) {
		/* todo: remove this? */
		case NODE_LOCAL: {
			UNUSED_CHECK;
			if (reload) {
				elf_emitbytexy(fs,line,BC_RELOAD,x,v.x);
			} else goto leave;
		} break;
		case NODE_THIS: {
			UNUSED_CHECK;
			elf_emitbyteop(fs,line,BC_LOADTHIS,x);
		} break;
		case NODE_CLSVAL: {
			UNUSED_CHECK;
			elf_emitbytexy(fs,line,BC_LOADCACHED,x,v.x);
		} break;
		case NODE_GLOBAL: {
			UNUSED_CHECK;
			elf_emitbytexy(fs,line,BC_LOADGLOBAL,x,v.x);
		} break;
		case NODE_NIL: {
			UNUSED_CHECK;
			/* -- todo: coalesce */
			elf_emitbytexy(fs,line,BC_LOADNIL,x,y);
		} break;
		case NODE_INTEGER: {
			UNUSED_CHECK;
			/* todo: interning */
			int yy = elf_varaddi(fs->md->ki,1);
			fs->md->ki[yy] = v.lit.i;
			elf_emitbytexy(fs,line,BC_LOADINT,x,yy);
		} break;
		case NODE_NUMBER: {
			UNUSED_CHECK;
			/* todo: interning */
			int yy = elf_varaddi(fs->md->kn,1);
			fs->md->kn[yy] = v.lit.n;
			elf_emitbytexy(fs,line,BC_LOADNUM,x,yy);
		} break;
		case NODE_STRING: {
			UNUSED_CHECK;
			/* -- todo: allocate this in constant pool */
			int g = lang_addglobal(fs->md,0,elf_valstr(elf_newstr(fs->rt,v.lit.s)));
			elf_emitbytexy(fs,line,BC_LOADGLOBAL,x,g);
		} break;
		case NODE_TABLE: {
			UNUSED_CHECK;
			elf_ensure(v.line != 0);
			elf_emitbytexy(fs,line,BC_TABLE,x,0);
			elf_arrfori(v.z) langL_emit(fs,line,v.z[i]);
		} break;
		case NODE_FIELD: case NODE_INDEX: {
			UNUSED_CHECK;
			elf_localid xx = elf_genlocalize(fs,line,v.x);
			elf_localid yy = elf_genlocalize(fs,line,v.y);
			elf_emitbytexyz(fs,line,nodetobyte(v.k),x,xx,yy);
		} break;
		case NODE_CLOSURE: {
			UNUSED_CHECK;
			elf_localid xx = x;
			elf_arrfori(v.z) {
				if (i != 0) xx = elf_genlocalalloc(fs,1);
				if ((xx - x) != i) elf_unreachable;
				langL_localload(fs,line,ltrue,xx++,1,v.z[i]);
			}
			elf_emitbytexy(fs,line,BC_CLOSURE,x,v.x);
		} break;
		case NODE_TYPEGUARD: {
			UNUSED_CHECK;
			langL_localload(fs,line,reload,x,y,v.x);
			elf_nodesetr(fs,line,id,fs->nodes[v.x]._r);
			x = fs->nodes[v.x]._r;
			// fs->nodes[id].r = x = fs->nodes[v.x].r;
			elf_emitbytexy(fs,v.line,BC_TYPEGUARD,x,elf_nodettotag(v.y));
		} break;
		case NODE_GROUP: {
			UNUSED_CHECK;
			langL_localload(fs,line,reload,x,y,v.x);
		} break;
		case NODE_FILE: {
			langL_localload(fs,line,ltrue,x,1,v.x);
			elf_emitbytexy(fs,line,BC_LOADFILE,x,y);
		} break;
		/* deprecated */
		case NODE_BUILTIN: {
			elf_unreachable;
		} break;
		case NODE_METAFIELD: {
			if (y == 0) goto leave;
			elf_localid rx = elf_genlocalize(fs,line,v.x);
			elf_localid ry = elf_genlocalize(fs,line,v.y);
			elf_emitbytexyz(fs,line,nodetobyte(v.k),x,rx,ry);
		} break;
		case NODE_CALL: {
			// the register x given to us is not safe to
			// use for arguments... as there could be
			// data in it
			elf_localid head = elf_genlocalalloc(fs,1);
			elf_ensure((head - x) != 0);
			elf_gentestmem(fs,line,head);
			elNode xx = fs->nodes[v.x];
			/* allocate v.x.x (the object) here because
			obviously if we just load v.x it'll reset
			its memory state and we'll loose v.x, so
			just do it here beforehand for convenience.
			once we load v.x, since v.x.x was already
			allocated and we didn't free it because
			we're still within the subexpression, it'll
			reuse that register. */
			elf_localid tail = head;
			if (xx.k == NODE_METAFIELD) {
				langL_localloadin(fs,line,tail ++,xx.x);
			}
			langL_localloadin(fs,line,tail ++,v.x);
			elf_arrfori(v.z) {
				langL_localloadin(fs,line,tail ++,v.z[i]);
			}
			int n = elf_varlen(v.z);
			elf_emitbytexyz(fs,line,xx.k == NODE_METAFIELD ? BC_METACALL : BC_CALL,head,n,y);
			if (y != 0) {
				/* ensure that we don't expect because
				we don't support that */
				if (y > 1) elf_filediag(fs,line,"unsupported");
				elf_emitbytexy(fs,line,BC_RELOAD,x,head);
			}
		} break;
		case NODE_AND: case NODE_OR: {
			if (y == 0) goto leave;
			/* todo: we need to optimize this, better support for
			boolean expressions */
			ljlist js = {0};
			langL_jumpiffalse(fs,&js,NO_SLOT,id);
			langL_localload(fs,line,ltrue,x,1,elf_nodeint(fs,line,ltrue));
			int j = elf_emitjump(fs,line,-1);
			langL_tieloosejs(fs,js.f);
			elf_delvar(js.f);
			js.f = elNIL;
			langL_localload(fs,line,ltrue,x,1,elf_nodeint(fs,line,lfalse));
			langL_tieloosej(fs,j);
		} break;
		case NODE_NEQ: case NODE_EQ:
		case NODE_GT: case NODE_LT:
		case NODE_GTEQ: case NODE_LTEQ:
		case NODE_DIV: case NODE_MUL: case NODE_MOD:
		case NODE_SUB: case NODE_ADD:
		case NODE_BITSHL: case NODE_BITSHR:
		case NODE_BITXOR: case NODE_BITOR: {
			/* todo: do this properly */
			// if (y == 0) goto leave;
			if ((v.k == NODE_GT) || (v.k == NODE_GTEQ)) {
				elf_localid xx = elf_genlocalize(fs,line,v.y);
				elf_localid yy = elf_genlocalize(fs,line,v.x);
				elf_emitbytexyz(fs,line,nodetobyte(v.k^1),x,xx,yy);
			} else
			if (v.k == NODE_EQ) {
				/* todo: enable this */
				if(1) goto _else;
				if (fs->nodes[v.y].k == NODE_NIL) {
					elf_localid xx = elf_genlocalize(fs,line,v.y);
					elf_emitbytexy(fs,line,BC_ISNIL,x,xx);
				} else goto _else;
			} else { elf_localid xx,yy; _else:
				xx = elf_genlocalize(fs,line,v.x);
				yy = elf_genlocalize(fs,line,v.y);
				elf_emitbytexyz(fs,line,nodetobyte(v.k),x,xx,yy);
			}
		} break;
		default: elf_unreachable;
	}

	leave:
	fs->fn->xmemory = mem;
}


void langL_moveto(elFileState *fs, elf_lineid line, elNodeID x, elNodeID y) {
	elNode v = fs->nodes[x];
	elf_ensure(v.level <= fs->level);
	elf_ensure(x >= 0);
	elf_ensure(y >= 0);
	if (v.line != 0) line = v.line;

	/* keep local state, finally free any temporary locals */
	elf_localid mem = fs->fn->xmemory;
	switch (v.k) {
		case NODE_GLOBAL: {
			elf_localid yy = elf_genlocalize(fs,line,y);
			elf_emitbytexy(fs,line,BC_SETGLOBAL,v.x,yy);
		} break;
		case NODE_CLSVAL: {
			elf_filediag(fs,line,"assignment to cache value is not supported yet");
		} break;
		case NODE_LOCAL: {
			/* -- todo: account for {x = x} opt ?  */
			langL_localload(fs,line,ltrue,v.x,1,y);
		} break;
		case NODE_INDEX: case NODE_FIELD: {
			elf_localid xx = elf_genlocalize(fs,line,v.x);
			elf_localid ii = elf_genlocalize(fs,line,v.y);
			elf_localid yy = elf_genlocalize(fs,line,y);
			elf_byteop op = v.k == NODE_INDEX ? BC_SETINDEX : BC_SETFIELD;
			elf_emitbytexyz(fs,line,op,xx,ii,yy);
		} break;
		case NODE_METAFIELD: {
			elf_localid xx = elf_genlocalize(fs,line,v.x);
			elf_localid ii = elf_genlocalize(fs,line,v.y);
			elf_localid yy = elf_genlocalize(fs,line,y);
			elf_emitbytexyz(fs,line,BC_SETMETAFIELD,xx,ii,yy);
		} break;
		// {x}[{x}..{x}] = {y}
		case NODE_RANGE_INDEX: {
			elNodeID lo = fs->nodes[v.y].x;
			elNodeID hi = fs->nodes[v.y].y;
			elNodeID ii = elf_nodelocal(fs,line,elf_genlocalalloc(fs,1));
			elf_localid xx = elf_genlocalize(fs,line,v.x);
			elf_localid yy = elf_genlocalize(fs,line,y);
			elf_fileblock block = {0};
			elf_enterblock(fs,&block,BLOCK_LOOP);
			elf_beginrangedloop(fs,line,ii,lo,hi);
			elf_emitbytexyz(fs,line,BC_SETINDEX,xx,block.loop.r,yy);
			elf_closerangedloop(fs,line);
			elf_leaveblock(fs);
		} break;
		default: {
			elf_unreachable;
		} break;
	}


	fs->fn->xmemory = mem;
}


void elf_genbeginif(elFileState *fs, elf_lineid line, Select *s, elNodeID x, int z) {
	ljlist js = {0};
	elf_genbranchif(fs,&js,z,NO_SLOT,x);
	// if  0 = jz
	// iff 1 = jnz
	if (z == L_IF) {
		elf_ensure(js.f != 0);
		langL_tieloosejs(fs,js.t);
		elf_delvar(js.t);
		js.t = 0;
		s->jz = js.f;
	} else {
		elf_ensure(js.t != 0);
		langL_tieloosejs(fs,js.f);
		elf_delvar(js.f);
		js.f = 0;
		s->jz = js.t;
	}
}


/*
** Closes previous conditional block by emitting
** escape jump, patches previous jz (jump if false)
** list to enter this block.
*/
void langL_addelse(elFileState *fs, elf_lineid line, Select *s) {
	elf_ensure(s->jz != 0);
	int j = elf_emitjump(fs,line,-1);
	elf_varadd(s->j,j);

	langL_tieloosejs(fs,s->jz);
	elf_delvar(s->jz);
	s->jz = elNIL;
}


void langL_addelif(elFileState *fs, elf_lineid line, Select *s, int x) {
	langL_addelse(fs,line,s);
	elf_genbeginif(fs,line,s,x,L_IF);
}


void langL_addthen(elFileState *fs, elf_lineid line, Select *s) {
	/* we don't need to close the previous block, it can just fall
	through to our branch, do collect all the other exit jumps and
	tie them to this branch block, naturally we don't need to add
	an exit jump since else and elif or closeif will terminate
	this block, multiple then blocks are simply chained together
	naturally. */
	langL_tieloosejs(fs,s->j);
	elf_delvar(s->j);
	s->j = elNIL;
}


void langL_closeif(elFileState *fs, elf_lineid line, Select *s) {
	/* collect missing else branch */
	if (s->jz != elNIL) {
		langL_tieloosejs(fs,s->jz);
		elf_delvar(s->jz);
		s->jz = elNIL;
	}
	/* collect missing then branch */
	if (s->j != elNIL) {
		langL_tieloosejs(fs,s->j);
		elf_delvar(s->j);
		s->j = elNIL;
	}
}


elf_fileblock *elf_getloopblock(elFileState *fs) {
	elf_fileblock *bl = fs->fn->block;
	while (bl != 0 && ~bl->flags & BLOCK_LOOP) {
	 	bl = bl->enclosing;
	}
	elf_ensure(bl != 0);
	return bl;
}


void elf_thisblockended(elFileState *fs) {
	fs->fn->block->flags |= BLOCK_ENDED;
}


void elf_emitcontinue(elFileState *fs, elf_lineid line) {
	elf_ensure(fs->fn->nloops > 0);
	elf_fileblock *bl = elf_getloopblock(fs);
	elf_thisblockended(fs);

	elf_byteid j = elf_emitjump(fs,line,elf_getlastbyteid(fs));
	elf_varadd(bl->loop.truejumps,j);
}


void elf_emitbreak(elFileState *fs, elf_lineid line) {
	elf_ensure(fs->fn->nloops > 0);
	elf_fileblock *bl = elf_getloopblock(fs);
	elf_thisblockended(fs);
	elf_byteid j = elf_emitjump(fs,line,elf_getlastbyteid(fs));
	elf_varadd(bl->leavejumps,j);
}


void langL_begindowhile(elFileState *fs, elf_lineid line) {
	elf_fileblock *bl = fs->fn->block;
	elf_ensure(bl->flags & BLOCK_LOOP);
	bl->loop.entry = elf_getlastbyteid(fs);
	bl->loop.falsejumps = elNIL;
	bl->loop.x = NO_NODE;
	bl->loop.r = NO_SLOT;
}


void langL_closedowhile(elFileState *fs, elf_lineid line, elNodeID x) {
	elf_fileblock *bl = fs->fn->block;
	elf_ensure(bl->flags & BLOCK_LOOP);

	ljlist js = {elNIL};
	langL_jumpiftrue(fs,&js,NO_SLOT,x);

	langL_tieloosejsto(fs,js.t,bl->loop.entry);
	elf_delvar(js.t);
	js.t = elNIL;

	langL_tieloosejsto(fs,bl->loop.truejumps,bl->loop.entry);
	elf_delvar(bl->loop.truejumps);
	bl->loop.truejumps = elNIL;
}


void langL_beginwhile(elFileState *fs, elf_lineid line, elNodeID x) {
	elf_fileblock *bl = fs->fn->block;
	elf_ensure(bl->flags & BLOCK_LOOP);

	bl->loop.entry = elf_getlastbyteid(fs);
	bl->loop.x = x;

	elf_ensure(bl->loop.falsejumps == 0);

	ljlist js = {elNIL};
	bl->loop.falsejumps = langL_jumpiffalse(fs,&js,NO_SLOT,x);
}


void langL_closewhile(elFileState *fs, elf_lineid line) {
	elf_fileblock *bl = fs->fn->block;
	elf_ensure(bl->flags & BLOCK_LOOP);

	langL_tieloosejs(fs,bl->loop.truejumps);
	elf_delvar(bl->loop.truejumps);
	bl->loop.truejumps = elNIL;
	elf_emitjump(fs,line,bl->loop.entry);
	langL_tieloosejs(fs,bl->loop.falsejumps);
	elf_delvar(bl->loop.falsejumps);
	bl->loop.falsejumps = elNIL;
}


elNodeID elf_nodeilessthan(elFileState *fs, elf_lineid line, elNodeID x, elNodeID y) {
	x = elf_nodetypeguard(fs,fs->nodes[x].line,x,NT_INT);
	y = elf_nodetypeguard(fs,fs->nodes[y].line,y,NT_INT);
	return elf_nodebinary(fs,line,NODE_LT,NT_BOL,x,y);
}


void elf_beginrangedloop(elFileState *fs, elf_lineid line, elNodeID x, elNodeID lo, elNodeID hi) {
	elf_fileblock *bl = fs->fn->block;
	elf_ensure(bl->flags & BLOCK_LOOP);

	elf_ensure(x != NO_NODE);
	bl->loop.x = x;
	bl->loop.r = elf_genlocalize(fs,line,x);
	langL_localload(fs,line,ltrue,bl->loop.r,1,lo);

	bl->loop.entry = elf_getlastbyteid(fs);

	elf_genlocalize(fs,line,hi);
	elNodeID c = elf_nodeilessthan(fs,line,bl->loop.x,hi);

	elf_ensure(bl->loop.falsejumps == 0);

	ljlist js = {elNIL};
	bl->loop.falsejumps = langL_jumpiffalse(fs,&js,NO_SLOT,c);
}


void elf_closerangedloop(elFileState *fs, elf_lineid line) {
	elf_fileblock *bl = fs->fn->block;
	elf_ensure(bl->flags & BLOCK_LOOP);

	langL_tieloosejs(fs,bl->loop.truejumps);
	elf_delvar(bl->loop.truejumps);
	bl->loop.truejumps = elNIL;
	elf_ensure(bl->loop.r == fs->nodes[bl->loop.x]._r);
	int x = bl->loop.x;
	int k = elf_nodebinary(fs,NO_LINE,NODE_ADD,NT_INT,x,elf_nodeint(fs,NO_LINE,1));
	langL_moveto(fs,line,x,k);
	elf_emitjump(fs,line,bl->loop.entry);
	langL_tieloosejs(fs,bl->loop.falsejumps);
	elf_delvar(bl->loop.falsejumps);
	bl->loop.falsejumps = 0;
}


/* -- todo? we could merge with adjacent blocks, but this
would change the order of execution, should leave as is? */
void elf_enterlastlyblock(elFileState *fs, elf_lineid line, elf_fileblock *bl) {
	elf_byteid jo = elf_emitbyteop(fs,line,BC_DELAY,NO_JUMP);
	elf_enterblock(fs,bl,BLOCK_DELAYED);
	bl->jumpover = jo;
}


void elf_closelastlyblock(elFileState *fs, elf_lineid line, elf_fileblock *bl) {
	// elf_filediag(fs,line,"closed block, %i",langL_getlocallabel(fs));
	elFileFnState *fn = fs->fn;
	elf_thisblockended(fs);
	elf_emitbyteop(fs,line,BC_LEAVE,0);
	elf_leaveblock(fs);
	elf_ensure(bl->entry != bl->jumpover);
	langL_tieloosej(fs,bl->jumpover);
}


elf_byteop nodetobyte(elNodeOP tt) {
	switch (tt) {
		case NODE_FIELD: 	  	return BC_FIELD;
		case NODE_INDEX: 	  	return BC_INDEX;
		case NODE_CALL:     	return BC_CALL;
		case NODE_METAFIELD:	return BC_METAFIELD;
		case NODE_ADD:     	return BC_ADD;
		case NODE_SUB:     	return BC_SUB;
		case NODE_DIV:     	return BC_DIV;
		case NODE_MUL:     	return BC_MUL;
		case NODE_MOD:     	return BC_MOD;
		case NODE_NEQ:     	return BC_NEQ;
		case NODE_EQ:      	return BC_EQ;
		case NODE_LT:      	return BC_LT;
		case NODE_LTEQ:    	return BC_LTEQ;
		case NODE_BITOR:    	return BC_BITOR;
		case NODE_BITSHL: return BC_SHL;
		case NODE_BITSHR: return BC_SHR;
		case NODE_BITXOR: return BC_XOR;
		/* for the intended use cases, this is an error */
		default: elf_unreachable;
	}
	return BC_HALT;
}

