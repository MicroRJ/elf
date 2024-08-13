/*
** See Copyright Notice In elf.h
** emit.c
** Bytecode Generation
*/



elByteOP elf_fnodetobyte(elNodeKi tt);


elNode elf_fgetnode(elFileState *fs, elNodeId id) {
	elASSERT(id != NO_SLOT);
	return fs->nodes[id];
}


elNodeKi elf_fgetnodekind(elFileState *fs, elNodeId id) {
	return elf_fgetnode(fs,id).kind;
}


elNodeTy elf_fgetnodetype(elFileState *fs, elNodeId id) {
	return elf_fgetnode(fs,id).type;
}


char *elf_fgetnodeline(elFileState *fs, elNodeId id) {
	return elf_fgetnode(fs,id).line;
}


elByteId elf_femit_get_last_byte(elFileState *fs) {
	return fs->M->nbytes;
}


elRegId elf_fgetmem(elFileState *fs) {
	return fs->fn->xmemory;
}


void elf_fsetmem(elFileState *fs, elRegId memory) {
	fs->fn->xmemory = memory;
}


elValueTag elf_nodettotag(elNodeTy ty) {
	switch (ty) {
		case NT_SYS: return TAG_SYS;
		case NT_NUM: return TAG_NUM;
		case NT_INT: return TAG_INT;
		default: elNOCODE;
	}
	return TAG_NIL;
}


elBool elf_fnodeislvalue(elNodeKi kind) {
	switch (kind) {
		case NODE_GLOBAL:
		case NODE_LOCAL:
		case NODE_RANGE_INDEX:
		case NODE_INDEX: case NODE_FIELD: {
			return 1;
		}
		default: {
			return 0;
		}
	}
}


elNode elf_fgettargetnode(elFileState *fs, elNodeIdTypeGuard id) {
	elNode node = elf_fgetnode(fs,id.id);
	switch (node.kind) {
		case NODE_TYPEGUARD:
		case NODE_GROUP:
		case NODE_REGION: {
			return elf_fgettargetnode(fs,MAKE_NODE_ID(node.x));
		}
		default: {
			return node;
		}
	}
}


int elf_fgetnodereg(elFileState *fs, elNodeIdTypeGuard id) {
	if (id.id==NO_NODE) return NO_SLOT;
	int reg = NO_SLOT;
	elNode node = elf_fgettargetnode(fs,id);
	if (node.kind == NODE_LOCAL) {
		reg = node.x;
		if (reg > 0xff) {
			// todo: re-add support for specifying which register
			// of which block you're referring to
			elFileBlock *bl = elf_fgetloopblock(fs,-1);
			elASSERT(bl != 0);
			elASSERT(bl->flags & BLOCK_LOOP);
			/* these registers are evaluated dynamically... */
			switch (reg) {
				case SPECIAL_REGISTER_ARRAY: {
					reg = bl->loop.array_register;
					if (reg == NO_SLOT) {
						elf_fdialog(fs,node.line,"#array register is not provided by this loop");
					}
				} break;
				case SPECIAL_REGISTER_VALUE: {
					reg = bl->loop.value_register;
					if (reg == NO_SLOT) {
						elf_fdialog(fs,node.line,"#value register is not provided by this loop");
					}
				} break;
				case SPECIAL_REGISTER_INDEX: {
					reg = bl->loop.index_register;
					if (reg == NO_SLOT) {
						elf_fdialog(fs,node.line,"#index register is not provided by this loop");
					}
				} break;
				default: elNOCODE;
			}
		}
	}
	return reg;
}


int elf_fnewreg(elFileState *fs) {
	int reg = fs->fn->xmemory ++;
	elASSERT(reg<=0xff);
	fs->fn->nlocals = MAX(fs->fn->nlocals,fs->fn->xmemory);
	return reg;
}


elFileline elf_fgetlineforbyte(elFileState *fs, elByteId id) {
	return fs->M->lines[id];
}


elBytecode elf_fgetbyte(elFileState *fs, elByteId id) {
	return fs->M->bytes[id];
}


/* we just associate each byte with a line,
this is simple and is pretty great for debugging... */
elByteId elf_femitbyte(elFileState *fs, elFileline line, elBytecode byte) {
	elModule *M=fs->M;
	ARRAY_ADD(M->lines,line);
	ARRAY_ADD(M->bytes,byte);
	ARRAY_ADD(M->track,0);
	elf_bytefpf(stdout,M,-1,M->nbytes-fs->fn->bytes,byte);
	return M->nbytes ++;
}


elByteId elf_femitx(elFileState *fs, elFileline line, elByteOP k, elInteger i) {
	return elf_femitbyte(fs,line,(elBytecode){k,i});
}


elByteId elf_femitxy(elFileState *fs, elFileline line, elByteOP k, int x, int y) {
	elBytecode b = (elBytecode){k};
	b.x = x, b.y = y;
	return elf_femitbyte(fs,line,b);
}


elByteId elf_femitxyz(elFileState *fs, elFileline line, elByteOP k, int x, int y, int z) {
	elBytecode b = (elBytecode){k};
	b.x = x, b.y = y, b.z = z;
	return elf_femitbyte(fs,line,b);
}


void elf_emitter_patch_jump_to(elFileState *fs, elByteId to, elByteId j) {
	elModule *M = fs->M;
	elBytecode *bytes = M->bytes;
	elBytecode b = bytes[to];

	elByteId l = j - to;
	// if (l == NO_JUMP) {
	// 	elf_fdialog(fs,M->lines[to],"opt, no jump");
	// }
	switch (b.k) {
		case BC_J: case BC_DELAY: {
			bytes[to].i = l;
		} break;
		case BC_JZ: case BC_JNZ: case BC_YIELD: {
			bytes[to].x = l;
		} break;
		default: elNOCODE;
	}
}


void elf_emitter_patch_jumplist_to(elFileState *fs, elByteId *js, elByteId j) {
	FOR_ARRAY(i,js) {
		elf_emitter_patch_jump_to(fs,js[i],j);
	}
}


void elf_emitter_patch_jump(elFileState *fs, elByteId i) {
	elf_emitter_patch_jump_to(fs,i,elf_femit_get_last_byte(fs));
}


void elf_femit_patch_jumplist(elFileState *fs, elByteId *js) {
	FOR_ARRAY(i,js) {
		elf_emitter_patch_jump(fs,js[i]);
	}
}


elByteId elf_emit_jump(elFileState *fs, elFileline line, elByteId j) {
	return elf_femitx(fs,line,BC_J,j - fs->M->nbytes);
}



elFileBlock *elf_fgetblock(elFileState *fs, elBlockId id) {
	return &fs->blocks[id > -1 ? id : fs->nblocks + id];
}


/* todo: the block ended flag could be added automatically when
we add a terminating byte to the current block */
void elf_faddblockflags(elFileState *fs, int flags) {
	elf_fgetblock(fs,-1)->flags |= flags;
}


int elf_fbeginblock(elFileState *fs, elBool flags) {
	int level = fs->nblocks ++;
	if (ARRAY_LENGTH(fs->blocks) < fs->nblocks) {
		ARRAY_GROW(fs->blocks,1);
	}

	elFileBlock *bl = &fs->blocks[level];
	elf_clearmemory(bl,sizeof(*bl));

	bl->loop.array_register = NO_SLOT;
	bl->loop.index_register = NO_SLOT;
	bl->loop.value_register = NO_SLOT;

	bl->level = level;
	bl->xmemory = fs->fn->xmemory;
	bl->xentity = fs->nentities;
	bl->xnode = fs->nnodes;
	bl->flags = flags;
	bl->entry = elf_femit_get_last_byte(fs);
	bl->jumpover = bl->entry;

	fs->nloops += (flags & BLOCK_LOOP) != 0;
	return level;
}


void elf_fclose_block(elFileState *fs) {
	elASSERT(fs->nentities >= fs->fn->entities);
	elFileBlock *bl = elf_fgetblock(fs,-1);
	elEntityId id;
	/* xentity is the first entity within a block, if any. */
	for (id = bl->xentity; id < fs->nentities; ++ id) {
		if (~fs->entities[id].flags & ENTITY_REFERENCED) {
			elf_fdialog(fs,fs->entities[id].line,"'%s': unreferenced entity", fs->entities[id].name);
		}
	}
	fs->nentities = bl->xentity;
	fs->nnodes    = bl->xnode;
	fs->nblocks  -= 1;
	// elASSERT(bl->level == fs->level);
	// fs->fn->block = bl->enclosing;
	fs->fn->xmemory = bl->xmemory;
	if (bl->leavejumps != 0) {
		elf_femit_patch_jumplist(fs,bl->leavejumps);
		ARRAY_DELETE(bl->leavejumps);
		bl->leavejumps = 0;
	}
	fs->nloops -= (bl->flags & BLOCK_LOOP) != 0;
}


elFileBlock *elf_fgetloopblock(elFileState *fs, elRegId reg) {
	int level;
	for (level = fs->nblocks-1; level > -1; -- level) {
		elFileBlock *bl = elf_fgetblock(fs,level);
		if (bl->flags & BLOCK_LOOP) {
			if (reg < 0) return bl; else
			if (bl->loop.value_register == reg) return bl;
		}
	}
	return 0;
}


void elf_fbeginlastlyblock(elFileState *fs, elFileline line) {
	elByteId jo = elf_femitx(fs,line,BC_DELAY,NO_JUMP);
	elBlockId id = elf_fbeginblock(fs,BLOCK_DELAYED);
	fs->blocks[id].jumpover = jo;
}


void elf_fcloselastlyblock(elFileState *fs, elFileline line) {
	elFileBlock *bl = elf_fgetblock(fs,-1);
	elf_femitx(fs,line,BC_LEAVE,0);
	elf_faddblockflags(fs,BLOCK_ENDED);
	elf_fclose_block(fs);
	elASSERT(bl->entry != bl->jumpover);
	elf_emitter_patch_jump(fs,bl->jumpover);
}


int elf_femitbranchif(elFileState *fs, elFileExpr *js, elBool if_true, elNodeId id) {
	elNode node = elf_fgetnode(fs,id);
	int j=NO_BYTE;
	switch (node.kind) {
		case NODE_GROUP: {
			j = elf_femitbranchif(fs,js,if_true,node.x);
		} break;
		case NODE_AND: {
			elf_femitjumpiffalse(fs,js,node.x);
			j = elf_femitbranchif(fs,js,if_true,node.y);
		} break;
		case NODE_OR: {
			elf_femitjumpiftrue(fs,js,node.x);
			j = elf_femitbranchif(fs,js,if_true,node.y);
		} break;
		default: {
			int mem,reg;
			mem=elf_fgetmem(fs);
			reg=elf_femitter_load(fs,id);
			if (if_true) {
				j=elf_femitxy(fs,node.line,BC_JNZ,NO_JUMP,reg);
				ARRAY_ADD(js->t,j);
			} else {
				j=elf_femitxy(fs,node.line,BC_JZ,NO_JUMP,reg);
				ARRAY_ADD(js->f,j);
			}
			elf_fsetmem(fs,mem);
		} break;
	}
	return j;
}


int elf_femitbranchiffalse(elFileState *fs, elFileExpr *js, elNodeId id) {
	return elf_femitbranchif(fs,js,0,id);
}


int elf_femitbranchiftrue(elFileState *fs, elFileExpr *js, elNodeId id) {
	return elf_femitbranchif(fs,js,1,id);
}


/* similar to branch if true, but additionally all
false jumps converge here */
int *elf_femitjumpiftrue(elFileState *fs, elFileExpr *js, elNodeId id) {
	elf_femitbranchiftrue(fs,js,id);
	elf_femit_patch_jumplist(fs,js->f);
	ARRAY_DELETE(js->f);
	js->f = 0;
	return js->t;
}


int *elf_femitjumpiffalse(elFileState *fs, elFileExpr *js, elNodeId id) {
	elf_femitbranchiffalse(fs,js,id);
	elf_femit_patch_jumplist(fs,js->t);
	ARRAY_DELETE(js->t);
	js->t = 0;
	return js->f;
}


int *elf_femitjumpifnonil(elFileState *fs, elFileline line, elFileExpr *js, elNodeId id) {
	return elf_femitjumpiffalse(fs,js,elf_nodexy(fs,line,NODE_EQ,NT_BOL,id,elf_nnil(fs,line)));
}


int *elf_femitjumpifnil(elFileState *fs, elFileline line, elFileExpr *js, elNodeId id) {
	return elf_femitjumpiftrue(fs,js,elf_nodexy(fs,line,NODE_EQ,NT_BOL,id,elf_nnil(fs,line)));
}


void elf_femitcontinue(elFileState *fs, elFileline line, elRegId with_value_register) {
	elASSERT(fs->nloops > 0);
	elFileBlock *bl = elf_fgetloopblock(fs,with_value_register);
	elASSERT(bl != 0);
	elf_faddblockflags(fs,BLOCK_ENDED);

	elByteId j = elf_emit_jump(fs,line,elf_femit_get_last_byte(fs));
	ARRAY_ADD(bl->loop.true_jumps,j);
}


void elf_femitbreak(elFileState *fs, elFileline line, elRegId with_value_register) {
	elASSERT(fs->nloops > 0);
	elFileBlock *bl = elf_fgetloopblock(fs,with_value_register);
	elASSERT(bl != 0);
	/* 'add_block_flags' is for the current block, which
	has effectively ended, even though we're not
	"targetting" this block itself but the outer one... */
	elf_faddblockflags(fs,BLOCK_ENDED);
	elByteId j = elf_emit_jump(fs,line,elf_femit_get_last_byte(fs));
	ARRAY_ADD(bl->leavejumps,j);
}


/* Range Expressions Desugaring */
void elf_fdesugarrangeexprepilogue(elFileState *fs, elNodeId x) {
	elNode node = elf_fgetnode(fs,x);
	switch (node.kind) {
		case NODE_INDEX: case NODE_FIELD: {
			elf_fdesugarrangeexprepilogue(fs,node.x);
		} break;
		case NODE_RANGE_INDEX: {
			elf_fcloserangeloop(fs,NO_LINE);
			elf_fclose_block(fs);
			elf_fdesugarrangeexprepilogue(fs,node.x);
		} break;
		default: ;
	}
}


elNodeId elf_fdesugarrangeexpr(elFileState *fs, elNodeId x, elBool flags) {
	elNode node = elf_fgettargetnode(fs,MAKE_NODE_ID(x));
	switch (node.kind) {
		case NODE_INDEX: case NODE_FIELD: {
			elNodeId xx = elf_fdesugarrangeexpr(fs,node.x,flags & ~EXPR_LHS);
			return elf_nodexy(fs,node.line,node.kind,NT_ANY,xx,node.y);
		}
		case NODE_RANGE_INDEX: {
			elASSERT(elf_fgetnodekind(fs,node.y) == NODE_RANGE);

			elFileline line = node.line;

			elNodeId array,index,value;
			array = elf_fdesugarrangeexpr(fs,node.x,flags&~EXPR_LHS);

			int block = elf_fbeginblock(fs,BLOCK_LOOP);

			int array_reg,index_reg,value_reg;
			array_reg = elf_femitter_load(fs,array);
			index_reg = elf_fnewreg(fs);

			array = elf_fnodelocal(fs,line,array_reg);
			index = elf_fnodelocal(fs,line,index_reg);
			value = elf_node_index(fs,line,array,index);

			elNodeId lo,hi;
			lo=elf_fgetnode(fs,node.y).x;
			hi=elf_fgetnode(fs,node.y).y;
			if (lo==NO_NODE) lo=elf_node_integer(fs,line,0);
			if (hi==NO_NODE) hi=elf_ncallmetafield(fs,line,array,0,"length");

			elf_fbeginrangeloop(fs,line,index,lo,hi);
			value_reg=elf_femitter_load(fs,value);

			/* todo: all these should be nodes instead */
			elf_fgetblock(fs,block)->loop.array_register=array_reg;
			elf_fgetblock(fs,block)->loop.value_register=value_reg;
			return value;
		}

		default: ;
	}
	return x;
}


void elf_femitfieldiniter(elFileState *fs, elFileline line, int reg, elNodeId id) {
	int mem = elf_fgetmem(fs);
	elNode node = elf_fgetnode(fs,id);
	switch (node.k) {
		case NODE_LOAD: {
			elNode x = elf_fgetnode(fs,node.x);
			if ((x.kind == NODE_LOCAL)) {
				elNOCODE;
			} else if ((x.kind == NODE_FIELD) || (x.k == NODE_INDEX)) {
				// elRegId xx = elf_femitter_load(fs,x.x);
				elRegId xx = reg;
				elRegId xy = elf_femitter_load(fs,x.y);
				elRegId yy = elf_femitter_load(fs,node.y);
				if (x.k == NODE_FIELD) {
					elf_femitxyz(fs,line,BC_SETFIELD,xx,xy,yy);
				} else elf_femitxyz(fs,line,BC_SETINDEX,xx,xy,yy);
			} else elNOCODE;
		} break;
		default: elNOCODE;
	}
	elf_fsetmem(fs,mem);
}


/* emits code when the node is not already a register to
load that node into any register */
elRegId elf_femitter_load(elFileState *fs, elNodeId id) {
	int reg;
	reg=elf_fgetnodereg(fs,MAKE_NODE_ID(id));
	if (reg==NO_SLOT) {
		reg=elf_femittereval(fs,0,-1,1,id);
		elASSERT(reg!=-1);
	}
	return reg;
}


int elf_fgetlocalconststore(elFileState *fs, int reg) {
	if (fs->nloops > 0) {
		return -1;
	}
	elBytecode byte;

	elBytecode *bytes;
	int nbytes;

	bytes=fs->M->bytes;
	nbytes=fs->M->nbytes;

	int i;
	for (i=nbytes-1; i>=fs->fn->bytes; --i) {
		byte=bytes[i];
		if ((byte.k==BC_GETKNUM)||(byte.k==BC_GETKINT)||(byte.k==BC_LOADNIL)) {
			if (byte.x==reg) {
				return i;
			}
		}
	}
	return -1;
}


void elf_fconstfold(elFileState *fs, elNode node) {
	#define isconst(k) (k==NODE_STRING||k==NODE_INTEGER||k==NODE_NUMBER)
	elNode xx,yy;
	int xconst;
	int yconst;
	xx=elf_fgetnode(fs,node.x);
	yy=elf_fgetnode(fs,node.y);
	xconst = isconst(xx.k) || ((xx.k==NODE_LOCAL) && (elf_fgetlocalconststore(fs,xx.x) != -1));
	yconst = isconst(yy.k) || ((yy.k==NODE_LOCAL) && (elf_fgetlocalconststore(fs,yy.x) != -1));

	if (xconst && yconst) {
		elf_fdialog(fs,node.line,"possible constant fold");
	}
}


int elf_femitterevalbinary(elFileState *fs, int flags, int reg, int nreg, elNode node) {
	int rx,ry;
	int mem;
	if ((node.k==NODE_GT) || (node.k==NODE_GTEQ)) {
		mem=elf_fgetmem(fs); {
			rx=elf_femitter_load(fs,node.y);
			ry=elf_femitter_load(fs,node.x);
		} elf_fsetmem(fs,mem);
		if (nreg<1) goto esc;
		if (reg<0) reg=elf_fnewreg(fs);
		elf_femitxyz(fs,node.line,elf_fnodetobyte(node.k^1),reg,rx,ry);
	} else {
		mem=elf_fgetmem(fs); {
			rx=elf_femitter_load(fs,node.x);
			ry=elf_femitter_load(fs,node.y);
		} elf_fsetmem(fs,mem);
		if (nreg<1) goto esc;
		if (reg<0) reg=elf_fnewreg(fs);
		elf_femitxyz(fs,node.line,elf_fnodetobyte(node.k),reg,rx,ry);
	}
	esc:
	return reg;
}


int elf_femittereval(elFileState *fs, int flags, int reg, int nreg, elNodeId id) {
	elModule *M;
	elNode node;
	char *line;
	int mem, rx,ry;

	M=fs->M;
	node=elf_fgetnode(fs,id);
	line=node.line;

	switch (node.kind) {
		case NODE_LOCAL: {
			if (nreg<1) goto esc;
			if (reg<0) reg=elf_fnewreg(fs);
			int src;
			src=elf_fgetnodereg(fs,MAKE_NODE_ID(id));
			if (src<0||src>=elf_fgetmem(fs)) {
				elf_fdialog(fs,line,"invalid memory state!");
			}
			elASSERT(src>=0&&src<elf_fgetmem(fs));
			elf_femitxy(fs,line,BC_RELOAD,reg,src);
		} break;
		case NODE_CLSVAL: {
			if (nreg<1) goto esc;
			if (reg<0) reg=elf_fnewreg(fs);

			elf_femitxy(fs,line,BC_GETCLOSED,reg,node.x);
		} break;
		case NODE_GLOBAL: {
			if (nreg<1) goto esc;
			if (reg<0) reg=elf_fnewreg(fs);

			elf_femitxy(fs,line,BC_GETGLOBAL,reg,node.x);
		} break;
		case NODE_NIL: {
			if (nreg<1) goto esc;
			if (reg<0) reg=elf_fnewreg(fs);

			elf_femitxy(fs,line,BC_LOADNIL,reg,0);
		} break;
		case NODE_INTEGER: {
			if (nreg<1) goto esc;
			if (reg<0) reg=elf_fnewreg(fs);

			/* todo: interning */
			int yy = ARRAY_GROW(M->integers,1);
			M->integers[yy] = node.lit.i;
			elf_femitxy(fs,line,BC_GETKINT,reg,yy);
		} break;
		case NODE_NUMBER: {
			if (nreg<1) goto esc;
			if (reg<0) reg=elf_fnewreg(fs);

			/* todo: interning */
			int yy = ARRAY_GROW(M->numbers,1);
			M->numbers[yy] = node.lit.n;
			elf_femitxy(fs,line,BC_GETKNUM,reg,yy);
		} break;
		case NODE_STRING: {
			if (nreg<1) goto esc;
			if (reg<0) reg=elf_fnewreg(fs);

			/* todo: interning */
			int xx = elf_add_global_value(M,0,elSTR(elf_new_string(fs->R,node.lit.s)));
			elf_femitxy(fs,line,BC_GETGLOBAL,reg,xx);
		} break;
		case NODE_TABLE: {
			if (reg<0) reg=elf_fnewreg(fs);

			elf_femitxy(fs,line,BC_TABLE,reg,0);
			/* todo: turn this into sugar? we can't yet... */
			FOR_ARRAY(i,node.z) {
				elf_femitfieldiniter(fs,line,reg,node.z[i]);
			}
		} break;
		case NODE_METAFIELD: case NODE_FIELD: case NODE_INDEX: {
			if (nreg<1) goto esc;
			mem=elf_fgetmem(fs); {
				rx=elf_femitter_load(fs,node.x);
				ry=elf_femitter_load(fs,node.y);
			} elf_fsetmem(fs,mem);
			if (reg<0) reg=elf_fnewreg(fs);
			elf_femitxyz(fs,line,elf_fnodetobyte(node.k),reg,rx,ry);
		} break;
		case NODE_CLOSURE: {
			if (nreg<1) goto esc;
			/* todo: why is it that we can't just start at reg */
			mem=elf_fgetmem(fs); {
				FOR_ARRAY(i,node.z) {
					elf_femittereval(fs,0,elf_fnewreg(fs),1,node.z[i]);
				}
			} elf_fsetmem(fs,mem);
			if (reg<0) reg=elf_fnewreg(fs);
			elf_femitxy(fs,line,BC_CLOSURE,mem,node.x);
			if (mem!=reg) {
				elf_femitxy(fs,line,BC_RELOAD,reg,mem);
			}
		} break;
		case NODE_TYPEGUARD: {
			if (nreg<1) goto esc;
			reg=elf_femittereval(fs,flags,reg,nreg,node.x);
			elf_femitxy(fs,node.line,BC_TYPEGUARD,reg,elf_nodettotag(node.y));
		} break;
		/* why is this even a thing dude */
		case NODE_GROUP: {
			reg=elf_femittereval(fs,flags,reg,nreg,node.x);
		} break;
		/* ({x}).{y}(...) will not be recognized as a meta-call
		because it's wrapped in parenthesis, could this be
		a feature */
		case NODE_CALL: {
			elNode xx;
			int nargs;

			xx=elf_fgetnode(fs,node.x);

			mem=elf_fgetmem(fs); {
				elASSERT(reg<mem);
				if ((xx.k == NODE_FIELD) || (xx.k == NODE_METAFIELD)) {
					ry=elf_femittereval(fs,0,-1,1,xx.y);
					rx=elf_femittereval(fs,0,-1,1,xx.x);
					elf_femitxyz(fs,line,elf_fnodetobyte(xx.k),ry,rx,ry);
					elASSERT(ry==mem);
				} else {
					ry=elf_femittereval(fs,0,-1,1,node.x);
					rx=elf_femittereval(fs,0,-1,1,elf_fnodelocal(fs,line,0));
					elASSERT(ry==mem);
				}
				FOR_ARRAY(i,node.z) {
					elf_femittereval(fs,0,-1,1,node.z[i]);
				}
				/* +2 the closure and object */
				elASSERT(ARRAY_LENGTH(node.z)+2==(elf_fgetmem(fs)-mem));
			} elf_fsetmem(fs,mem);

			nargs=ARRAY_LENGTH(node.z)+1;
			elf_femitxyz(fs,line,BC_CALL,mem,nargs,nreg);
			if (nreg<1) goto esc;
			if (nreg>1) elf_fdialog(fs,line,"unsupported");
			if (reg<0) reg=elf_fnewreg(fs);
			if (reg!=mem) {
				elf_femitxy(fs,line,BC_RELOAD,reg,mem);
			}
		} break;
		/* (a !! b) = (a == nil ? a : b) */
		case NODE_NIL_AND: {
			elByteId *js;
			elFileExpr _ = {0};
			if (reg<0)reg=elf_fnewreg(fs);
			elf_femittereval(fs,0,reg,1,node.x);
			js=elf_femitjumpifnil(fs,NO_LINE,&_,elf_fnodelocal(fs,NO_LINE,reg));
			elf_femittereval(fs,0,reg,1,node.y);
			elf_femit_patch_jumplist(fs,js);
			ARRAY_DELETE(js);
		} break;
		case NODE_NIL_OR: {
			elByteId *js;
			elFileExpr _ = {0};
			if (reg<0)reg=elf_fnewreg(fs);
			elf_femittereval(fs,0,reg,1,node.x);
			js=elf_femitjumpifnonil(fs,NO_LINE,&_,elf_fnodelocal(fs,elf_fgetnodeline(fs,node.x),reg));
			elf_femittereval(fs,0,reg,1,node.y);
			elf_femit_patch_jumplist(fs,js);
			ARRAY_DELETE(js);
		} break;
		case NODE_AND: case NODE_OR: {
			if (reg<0)reg=elf_fnewreg(fs);
			elf_femittereval(fs,0,reg,1,elf_node_integer(fs,line,0));
			elFileExpr _ = {0};
			elByteId *js = elf_femitjumpiffalse(fs,&_,id);
			elf_femittereval(fs,0,reg,1,elf_node_integer(fs,line,1));
			elf_femit_patch_jumplist(fs,js);
			ARRAY_DELETE(js);
		} break;
		case NODE_EQ: case NODE_NEQ:
		case NODE_GT: case NODE_GTEQ: case NODE_LT: case NODE_LTEQ:
		case NODE_DIV: case NODE_MUL: case NODE_MOD:
		case NODE_SUB: case NODE_ADD: case NODE_POW:
		case NODE_BIT_SHL: case NODE_BIT_SHR:
		case NODE_BIT_XOR:
		case NODE_BIT_AND: case NODE_BIT_OR: {
			reg=elf_femitterevalbinary(fs,flags,reg,nreg,node);
		} break;
		default: {
			elf_fdialog(fs,line,"invalid node (%s)",node2s[node.kind]);
			elNOCODE;
		}
	}
	esc:
	return reg;
}


void elf_femitstore(elFileState *fs, elFileline line, elNodeId x, elNodeId y) {
	elNode v = elf_fgettargetnode(fs,MAKE_NODE_ID(x));
	elASSERT(elf_fnodeislvalue(v.kind));
	elASSERT(v.level < fs->nblocks);
	elASSERT(x >= 0);
	elASSERT(y >= 0);
	if (v.line != 0) line = v.line;

	/* keep local state, lastly free any temporary locals */
	elRegId mem = elf_fgetmem(fs);
	switch (v.k) {
		case NODE_GLOBAL: {
			elRegId yy = elf_femitter_load(fs,y);
			elf_femitxy(fs,line,BC_SETGLOBAL,v.x,yy);
		} break;
		case NODE_CLSVAL: {
			elf_fdialog(fs,line,"assignment to closure value is not supported yet");
		} break;
		case NODE_LOCAL: {
			elf_femittereval(fs,0,v.x,1,y);
		} break;
		case NODE_INDEX: case NODE_FIELD: {
			elNodeId table_node = v.x;
			elNodeId field_node = v.y;
			elNodeId value_node = y;
			elRegId table_register = elf_femitter_load(fs,table_node);
			elRegId field_register = elf_femitter_load(fs,field_node);
			elRegId value_register = elf_femitter_load(fs,value_node);
			elByteOP op = v.k == NODE_INDEX ? BC_SETINDEX : BC_SETFIELD;
			elf_femitxyz(fs,line,op,table_register,field_register,value_register);
		} break;
		case NODE_METAFIELD: {
			elf_fdialog(fs,line,"meta fields are constant");
		} break;
		default: {
			elNOCODE;
		} break;
	}

	elf_fsetmem(fs,mem);
}


void elf_fbeginif(elFileState *fs, elFileline line, elSelectState *s, elNodeId x, int z) {
	elFileExpr js = {0};
	elf_femitbranchif(fs,&js,z,x);
	// if  0 = jz
	// iff 1 = jnz
	if (z == L_IF) {
		elASSERT(js.f != 0);
		elf_femit_patch_jumplist(fs,js.t);
		ARRAY_DELETE(js.t);
		js.t = 0;
		s->jz = js.f;
	} else {
		elASSERT(js.t != 0);
		elf_femit_patch_jumplist(fs,js.f);
		ARRAY_DELETE(js.f);
		js.f = 0;
		s->jz = js.t;
	}
}


/*
** Closes previous conditional block by emitting
** escape jump, patches previous jz (jump if false)
** list to enter this block.
*/
void elf_fifaddelseclause(elFileState *fs, elFileline line, elSelectState *s) {
	if (s->jz == 0) {
		elf_fdialog(fs,line,"invalid else clause");
	}
	elASSERT(s->jz != 0);
	int j = elf_emit_jump(fs,line,-1);
	ARRAY_ADD(s->j,j);

	elf_femit_patch_jumplist(fs,s->jz);
	ARRAY_DELETE(s->jz);
	s->jz = 0;
}


void elf_fifaddelifclause(elFileState *fs, elFileline line, elSelectState *s, int x) {
	elf_fifaddelseclause(fs,line,s);
	elf_fbeginif(fs,line,s,x,L_IF);
}


void elf_fifaddthenclause(elFileState *fs, elFileline line, elSelectState *s) {
	/* we don't need to close the previous block, it can just fall
	through to our branch, do collect all the other exit jumps and
	tie them to this branch block, naturally we don't need to add
	an exit jump since else and elif or closeif will terminate
	this block, multiple then blocks are simply chained together
	naturally. */
	elf_femit_patch_jumplist(fs,s->j);
	ARRAY_DELETE(s->j);
	s->j = 0;
}


void elf_fcloseif(elFileState *fs, elFileline line, elSelectState *s) {
	/* collect missing else branch */
	if (s->jz != 0) {
		elf_femit_patch_jumplist(fs,s->jz);
		ARRAY_DELETE(s->jz);
		s->jz = 0;
	}
	/* collect missing then branch */
	if (s->j != 0) {
		elf_femit_patch_jumplist(fs,s->j);
		ARRAY_DELETE(s->j);
		s->j = 0;
	}
}


/* todo: add support for multiple results */
void elf_femit_yield(elFileState *fs, elFileline line, elNodeId id) {
	/* todo: if we only return one value we don't have to reload */
	int mem=elf_fgetmem(fs);
	if (id!=NO_NODE) {
		/* todo: get this from the expression */
		int reg,nreg,j;
		elf_femittereval(fs,0,reg=elf_fnewreg(fs),nreg=1,id);
		if (fs->fn->nyield < nreg) fs->fn->nyield = nreg;
		j=elf_femitxyz(fs,line,BC_YIELD,NO_JUMP,reg,nreg);
		ARRAY_ADD(fs->fn->yj,j);
	} else elf_femitx(fs,line,BC_LEAVE,0);
	elf_fsetmem(fs,mem);
	elf_faddblockflags(fs,BLOCK_ENDED);
}


void elf_fbegindowhileloop(elFileState *fs, elFileline line) {
	elFileBlock *bl = elf_fgetblock(fs,-1); // fs->fn->block
	elASSERT(bl->flags & BLOCK_LOOP);
	bl->loop.entry = elf_femit_get_last_byte(fs);
	bl->loop.false_jumps = 0;
	bl->loop.x = NO_NODE;
	bl->loop.index_register = NO_SLOT;
}


void elf_fclosedowhileloop(elFileState *fs, elFileline line, elNodeId x) {
	elFileBlock *bl = elf_fgetblock(fs,-1); // fs->fn->block;
	elASSERT(bl->flags & BLOCK_LOOP);

	elFileExpr js = {0};
	elf_femitjumpiftrue(fs,&js,x);

	elf_emitter_patch_jumplist_to(fs,js.t,bl->loop.entry);
	ARRAY_DELETE(js.t);
	js.t = 0;

	elf_emitter_patch_jumplist_to(fs,bl->loop.true_jumps,bl->loop.entry);
	ARRAY_DELETE(bl->loop.true_jumps);
	bl->loop.true_jumps = 0;
}


void elf_femit_begin_while_loop(elFileState *fs, elNodeId x) {
	elFileBlock *bl = elf_fgetblock(fs,-1);
	elASSERT(bl->flags & BLOCK_LOOP);

	bl->loop.x = x;

	bl->loop.entry = elf_femit_get_last_byte(fs);

	/* todo: this is temporary */
	elf_femitx(fs,NO_LINE,BC_LOOP,-1);

	elASSERT(bl->loop.false_jumps == 0);

	elFileExpr js = {0};
	bl->loop.false_jumps = elf_femitjumpiffalse(fs,&js,x);
}


void elf_femit_close_while_loop(elFileState *fs) {
	elFileBlock *bl = elf_fgetblock(fs,-1);
	elASSERT(bl->flags & BLOCK_LOOP);

	elf_femit_patch_jumplist(fs,bl->loop.true_jumps);
	ARRAY_DELETE(bl->loop.true_jumps);
	bl->loop.true_jumps = 0;

	/* todo: this is temporary */
	elASSERT(fs->M->bytes[bl->loop.entry].k==BC_LOOP);
	fs->M->bytes[bl->loop.entry].x = elf_femit_get_last_byte(fs);

	elf_emit_jump(fs,NO_LINE,bl->loop.entry);

	elf_femit_patch_jumplist(fs,bl->loop.false_jumps);
	ARRAY_DELETE(bl->loop.false_jumps);
	bl->loop.false_jumps = 0;
}


void elf_fbeginrangeloop(elFileState *fs, elFileline line, elNodeId index_node, elNodeId lo, elNodeId hi) {
	elFileBlock *bl = elf_fgetblock(fs,-1);
	elASSERT(bl->flags & BLOCK_LOOP);

	elASSERT(index_node != NO_NODE);

	elRegId index_register = elf_femitter_load(fs,index_node);
	index_node = elf_fnodelocal(fs,line,index_register);

	bl->loop.index_register = index_register;
	bl->loop.x = index_node;
	elf_femittereval(fs,0,index_register,1,elf_ntypeguard(fs,elf_fgetnodeline(fs,lo),lo,NT_INT));

	bl->loop.entry = elf_femit_get_last_byte(fs);

	elRegId hi_register = elf_femitter_load(fs,elf_ntypeguard(fs,elf_fgetnodeline(fs,hi),hi,NT_INT));
	hi = elf_fnodelocal(fs,line,hi_register);
	elNodeId c = elf_make_node_less_than(fs,line,index_node,hi);

	elASSERT(bl->loop.false_jumps == 0);

	elFileExpr js = {0};
	bl->loop.false_jumps = elf_femitjumpiffalse(fs,&js,c);
}


void elf_fcloserangeloop(elFileState *fs, elFileline line) {
	elFileBlock *bl = elf_fgetblock(fs,-1); // fs->fn->block;
	elASSERT(bl->flags & BLOCK_LOOP);

	elf_femit_patch_jumplist(fs,bl->loop.true_jumps);
	ARRAY_DELETE(bl->loop.true_jumps);
	bl->loop.true_jumps = 0;
	// elNodeId index_node = bl->loop.index_node;
	elNodeId index_node = elf_fnodelocal(fs,NO_LINE,bl->loop.index_register);
	elNodeId k = elf_nodexy(fs,NO_LINE,NODE_ADD,NT_INT,index_node,elf_node_integer(fs,NO_LINE,1));
	elf_femitstore(fs,line,index_node,k);
	elf_emit_jump(fs,line,bl->loop.entry);

	elf_femit_patch_jumplist(fs,bl->loop.false_jumps);
	ARRAY_DELETE(bl->loop.false_jumps);
	bl->loop.false_jumps = 0;
}


elByteOP elf_fnodetobyte(elNodeKi tt) {
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
		default: elNOCODE;
	}
	return BC_HALT;
}


elBool elf_is_binary_node(elNodeKi kind) {
	return kind >= NODE_AND && kind <= NODE_BIT_OR;
}

void elf_node_fpf(elFileState *fs, FILE *io, elNodeId id) {
	elNode node = elf_fgetnode(fs,id);
	if (elf_is_binary_node(node.kind)) {
		fprintf(io, "(%s ", node2s[node.kind]);
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
		default: fprintf(io,"%s",node2s[node.kind]);
	}
	if (elf_fgetnodereg(fs,MAKE_NODE_ID(id)) != NO_SLOT) {
		fprintf(io,"@r%i",elf_fgetnodereg(fs,MAKE_NODE_ID(id)));
	}
}


/* Node constructors */

elNodeId elf_fnodexyz(elFileState *fs, elFileline line, elNodeKi kind, elNodeTy type, elNodeId x, elNodeId y, elNodeId *z) {
	if (ARRAY_MIN(fs->nodes)<=fs->nnodes) {
		ARRAY_GROW(fs->nodes,1);
	}
	elNode *nd = fs->nodes+fs->nnodes;
	nd->level=fs->nblocks-1;
	nd->line=line;
	nd->type=type;
	nd->kind=kind;
	nd->x=x;nd->y=y;nd->z=z;
	return fs->nnodes ++;
}


elNodeId elf_nodexy(elFileState *fs, elFileline line, elNodeKi k, elNodeTy t, elNodeId x, elNodeId y) {
	return elf_fnodexyz(fs,line,k,t,x,y,0);
}


elNodeId elf_nodex(elFileState *fs, elFileline line, elNodeKi k, elNodeTy t, elNodeId x) {
	return elf_nodexy(fs,line,k,t,x,NO_NODE);
}


elNodeId elf_make_nullary_node(elFileState *fs, elFileline line, elNodeKi k, elNodeTy t) {
	return elf_nodex(fs,line,k,t,NO_NODE);
}


elNodeId elf_node_store(elFileState *fs, elFileline line, elNodeId x, elNodeId y) {
	return elf_nodexy(fs,line,NODE_LOAD,NT_ANY,x,y);
}


elNodeId elf_ntypeguard(elFileState *fs, elFileline line, elNodeId x, elNodeTy y) {
	return elf_nodexy(fs,line,NODE_TYPEGUARD,y,x,y);
}


elNodeId elf_make_group_node(elFileState *fs, elFileline line, elNodeId x) {
	elNodeId id = elf_nodex(fs,line,NODE_GROUP,elf_fgetnodetype(fs,x),x);
	return id;
}


elNodeId elf_node_integer(elFileState *fs, elFileline line, elInteger i) {
	elNodeId v = elf_make_nullary_node(fs,line,NODE_INTEGER,NT_INT);
	fs->nodes[v].lit.i = i;
	return v;
}


elNodeId elf_nnumber(elFileState *fs, elFileline line, elNumber n) {
	elNodeId v = elf_make_nullary_node(fs,line,NODE_NUMBER,NT_NUM);
	fs->nodes[v].lit.n = n;
	return v;
}


elNodeId elf_node_string(elFileState *fs, elFileline line, char *s) {
	elNodeId v = elf_make_nullary_node(fs,line,NODE_STRING,NT_STR);
	fs->nodes[v].lit.s = s;
	return v;
}


elNodeId elf_node_newtable(elFileState *fs, elFileline line, elNodeId *z) {
	return elf_fnodexyz(fs,line,NODE_TABLE,NT_TAB,NO_NODE,NO_NODE,z);
}


elNodeId elf_make_closure_node(elFileState *fs, elFileline line, elNodeId x, elNodeId *z) {
	return elf_fnodexyz(fs,line,NODE_CLOSURE,NT_FUN,x,NO_NODE,z);
}


elNodeId elf_nnil(elFileState *fs, elFileline line) {
	return elf_make_nullary_node(fs,line,NODE_NIL,NT_NIL);
}


elNodeId elf_nclosevalue(elFileState *fs, elFileline line, elRegId x) {
	return elf_nodex(fs,line,NODE_CLSVAL,NT_ANY,x);
}


elNodeId elf_fnodelocal(elFileState *fs, elFileline line, elRegId x) {
	return elf_nodex(fs,line,NODE_LOCAL,NT_ANY,x);
}


elNodeId elf_nodelocalxy(elFileState *fs, elFileline line, elRegId x, elNodeId y) {
	return elf_nodexy(fs,line,NODE_LOCAL,NT_ANY,x,y);
}


elNodeId elf_fnodeglobal(elFileState *fs, elFileline line, elSymbolId x) {
	return elf_nodex(fs,line,NODE_GLOBAL,NT_ANY,x);
}


elNodeId elf_node_field(elFileState *fs, elFileline line, elNodeId x, elNodeId y) {
	return elf_nodexy(fs,line,NODE_FIELD,NT_ANY,x,y);
}


elNodeId elf_node_index(elFileState *fs, elFileline line, elNodeId x, elNodeId y) {
	return elf_nodexy(fs,line,NODE_INDEX,NT_ANY,x,y);
}


elNodeId elf_make_ranged_index_node(elFileState *fs, elFileline line, elNodeId x, elNodeId y) {
	return elf_nodexy(fs,line,NODE_RANGE_INDEX,NT_ANY,x,y);
}


elNodeId elf_fnodemetafield(elFileState *fs, elFileline line, elNodeId x, elNodeId y) {
	return elf_nodexy(fs,line,NODE_METAFIELD,NT_ANY,x,y);
}


elNodeId elf_fnodecall(elFileState *fs, elFileline line, elNodeId x, elNodeId *z) {
	return elf_fnodexyz(fs,line,NODE_CALL,NT_ANY,x,NO_NODE,z);
}

elNodeId elf_nmulti(elFileState *fs, elFileline line, elNodeId *z) {
	return elf_fnodexyz(fs,line,NODE_MULTI,NT_ANY,NO_NODE,NO_NODE,z);
}

elNodeId elf_make_node_less_than(elFileState *fs, elFileline line, elNodeId x, elNodeId y) {
	return elf_nodexy(fs,line,NODE_LT,NT_BOL,x,y);
}


elNodeId elf_make_node_is_nil(elFileState *fs, elFileline line, elNodeId x) {
	return elf_nodexy(fs,line,NODE_EQ,NT_BOL,x,elf_nnil(fs,line));
}

elNodeId elf_ncallmetafield(elFileState *fs, elFileline line, elNodeId x, elNodeId *z, char *name) {
	elNodeId field = elf_fnodemetafield(fs,line,x,elf_node_string(fs,line,name));
	return elf_fnodecall(fs,line,field,z);
}

