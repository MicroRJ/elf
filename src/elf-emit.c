/*
** See Copyright Notice In elf.h
** elf-emit.c
** Bytecode Generator (node -> bytecode)
*/



elByteOP elf_node_to_byte(elNodeKi tt);


elByteId elf_get_last_byteid(elFileState *fs) {
	return fs->M->nbytes;
}


elBytecode elf_fgetbyte(elFileState *F) {
	return F->M->bytes[F->M->nbytes-1];
}

elBytecode elf_fpopbyte(elFileState *F) {
	return F->M->bytes[-- F->M->nbytes];
}


elRegId elf_fgetmem(elFileState *fs) {
	return fs->fn->xmemory;
}


void elf_fsetmem(elFileState *fs, elRegId memory) {
	fs->fn->xmemory = memory;
}


elBool elf_is_targetable_node(elNodeKi kind) {
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


elNode elf_get_targetable_node(elFileState *fs, elNodeIdTypeGuard id) {
	elNode node = elf_Nget(fs,id.id);
	switch (node.kind) {
		case NODE_TYPEGUARD:
		case NODE_GROUP:
		case NODE_REGION: {
			return elf_get_targetable_node(fs,MAKE_NODE_ID(node.x));
		}
		default: {
			return node;
		}
	}
}


elRegId elf_get_node_register(elFileState *fs, elNodeIdTypeGuard id) {
	if (id.id == NO_NODE) return NO_SLOT;
	elNode node = elf_get_targetable_node(fs,id);
	if (node.kind == NODE_LOCAL) {
		return node.x;
	} else
	if (node.kind == NODE_SPECIAL_REGISTER) {
		elFileBlock *bl = elf_emitter_get_loop_block(fs,-1); // todo: re-add support for specifying which register you're referring to
		elASSERT(bl != 0);
		elASSERT(bl->flags & BLOCK_LOOP);
		/* Special register are evaluated dynamically, meaning
		that if this same node were within some other loop we'd
		get a different result, So... that's pretty interesting
		and will have to keep an eye out for possible bugs... */
		elRegId reg = NO_SLOT;
		switch (node.x) {
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
		return reg;
	}
	return NO_SLOT;
}


int elf_fnewreg(elFileState *F) {
	elFileFnState *fn = F->fn;
	int reg = fn->xmemory ++;
	fn->nlocals = MAX(fn->nlocals,fn->xmemory);
	return reg;
}




elRegId elf_fregalloc2(elFileState *fs, elFileline line, int _, int __) {
	(void) _;
	(void) __;
	elFileFnState *fn = fs->fn;
	elRegId reg = fn->xmemory ++;
	fn->nlocals = MAX(fn->nlocals,fn->xmemory);
	return reg;
}


elFileline elf_emitter_get_line(elFileState *fs, elByteId id) {
	return fs->M->lines[id];
}


elBytecode elf_emitter_get_byte(elFileState *fs, elByteId id) {
	return fs->M->bytes[id];
}


elByteId elf_emitter_add_byte(elFileState *fs, elFileline line, elBytecode byte) {
	elModule *M=fs->M;
	if (ARRAY_LENGTH(M->bytes)<=M->nbytes) {
		ARRAY_GROW(M->bytes,1);
	}
	ARRAY_ADD(M->lines,line);
	ARRAY_ADD(M->track,0);
	M->bytes[M->nbytes]=byte;
	elFileFnState *fn = fs->fn;
	elf_bytefpf(stdout,M,-1,M->nbytes-fn->bytes,byte);
	return M->nbytes ++;
}


elByteId elf_femitx(elFileState *fs, elFileline line, elByteOP k, elInteger i) {
	return elf_emitter_add_byte(fs,line,(elBytecode){k,i});
}


elByteId elf_femitxy(elFileState *fs, elFileline line, elByteOP k, int x, int y) {
	elBytecode b = (elBytecode){k};
	b.x = x, b.y = y;
	return elf_emitter_add_byte(fs,line,b);
}


elByteId elf_femitxyz(elFileState *fs, elFileline line, elByteOP k, int x, int y, int z) {
	elBytecode b = (elBytecode){k};
	b.x = x, b.y = y, b.z = z;
	return elf_emitter_add_byte(fs,line,b);
}


/* Control Flow */

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
	elf_emitter_patch_jump_to(fs,i,elf_get_last_byteid(fs));
}


void elf_fpatchjs(elFileState *fs, elByteId *js) {
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
void elf_emitter_add_block_flags(elFileState *fs, int flags) {
	elf_fgetblock(fs,-1)->flags |= flags;
}


elBlockId elf_fbeginblock(elFileState *fs, elBool flags) {
	elBlockId level = fs->nblocks ++;
	if (ARRAY_LENGTH(fs->blocks) < fs->nblocks) {
		ARRAY_GROW(fs->blocks,1);
	}

	elFileBlock *bl = &fs->blocks[level];
	elf_clear_memory(bl,sizeof(*bl));

	bl->loop.array_register = NO_SLOT;
	bl->loop.index_register = NO_SLOT;
	bl->loop.value_register = NO_SLOT;

	bl->level = level;
	bl->xmemory = fs->fn->xmemory;
	bl->xentity = fs->nentities;
	bl->xnode = fs->nnodes;
	bl->flags = flags;
	bl->entry = elf_get_last_byteid(fs);
	bl->jumpover = bl->entry;

	/* todo: this is rather ugly, we can simply increment
	this when we enter an actual loop... */
	fs->nloops += (flags & BLOCK_LOOP) != 0;
	// fs->fn->nblocks += 1;
	return level;
}


void elf_fcloseblock(elFileState *fs) {
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
	fs->nnodes = bl->xnode;
	fs->level -= 1;
	// elASSERT(bl->level == fs->level);
	// fs->fn->block = bl->enclosing;
	fs->fn->xmemory = bl->xmemory;
	if (bl->leavejumps != 0) {
		elf_fpatchjs(fs,bl->leavejumps);
		ARRAY_DELETE(bl->leavejumps);
		bl->leavejumps = 0;
	}
	fs->nloops -= (bl->flags & BLOCK_LOOP) != 0;
}


elFileBlock *elf_emitter_get_loop_block(elFileState *fs, elRegId with_value_register) {
	int level;
	for (level = fs->level-1; level > -1; -- level) {
		elFileBlock *bl = elf_fgetblock(fs,level);
		if (bl->flags & BLOCK_LOOP) {
			if (with_value_register < 0) return bl; else
			if (bl->loop.value_register == with_value_register) return bl;
		}
	}
	return 0;
}


/* we could merge with adjacent blocks, but this
would change the order of execution, leave as is? */
void elf_Fbegindelayblock(elFileState *fs, elFileline line) {//, elFileBlock *bl
	elByteId jo = elf_femitx(fs,line,BC_DELAY,NO_JUMP);
	elBlockId id = elf_fbeginblock(fs,BLOCK_DELAYED);//bl,
	fs->blocks[id].jumpover = jo;
}


void elf_Fclosedelayblock(elFileState *fs, elFileline line) {//, elFileBlock *bl
	// elf_fdialog(fs,line,"closed block, %i",langL_getlocallabel(fs));
	elFileBlock *bl = elf_fgetblock(fs,-1); // fn = fs->fn;

	elf_femitx(fs,line,BC_LEAVE,0);
	elf_emitter_add_block_flags(fs,BLOCK_ENDED);
	elf_fcloseblock(fs);

	elASSERT(bl->entry != bl->jumpover);
	elf_emitter_patch_jump(fs,bl->jumpover);
}


/*
** Evaluates the given boolean or logical expression using short
** circuit evaluation, creating branches as it evaluates each
** expression, only one register is necessary, if no registers
** are given one is allocated and deallocated automatically.
*/
elByteId elf_emit_branch_if(elFileState *fs, elFileExpr *js, elBool z, elRegId x, elNodeId id) {
	elByteId j = NO_BYTE;

	elASSERT(x == NO_SLOT);
	elRegId mem = elf_fgetmem(fs);
	// /* allocate temporary register before diverging. */
	// if (x == NO_SLOT) {
	// 	x = elf_fregalloc2(fs,NO_LINE,NO_SLOT,id);
	// }

	elNode node = elf_Nget(fs,id);

	switch (node.kind) {
		case NODE_GROUP: {
			j = elf_emit_branch_if(fs,js,z,x,node.x);
		} break;
		case NODE_AND: {
			elf_emit_jump_if_false(fs,js,x,node.x);
			j = elf_emit_branch_if(fs,js,z,x,node.y);
		} break;
		case NODE_OR: {
			elf_emit_jump_if_true(fs,js,x,node.x);
			j = elf_emit_branch_if(fs,js,z,x,node.y);
		} break;
		default: {
			// elf_emitter_evaluate(fs,NO_LINE,false,x,1,id);
			elASSERT(x == NO_SLOT);
			x = elf_Zlocalize2reg(fs,NO_LINE,id);

			if (z != 0) {
				j = elf_femitxy(fs,node.line,BC_JNZ,NO_JUMP,x);
				ARRAY_ADD(js->t,j);
			} else {
				j = elf_femitxy(fs,node.line,BC_JZ,NO_JUMP,x);
				ARRAY_ADD(js->f,j);
			}
		} break;
	}

	elf_fsetmem(fs,mem);
	return j;
}


elByteId elf_branch_if_false(elFileState *fs, elFileExpr *js, elRegId x, elNodeId id) {
	return elf_emit_branch_if(fs,js,0,x,id);
}


elByteId elf_branch_if_true(elFileState *fs, elFileExpr *js, elRegId x, elNodeId id) {
	return elf_emit_branch_if(fs,js,1,x,id);
}


/*
** Similar to branch if true, but additionally this byte
** address becomes the false target, thus all false
** branches converge here. All true branches are returned.
*/
elByteId *elf_emit_jump_if_true(elFileState *fs, elFileExpr *js, elRegId x, elNodeId id) {
	elf_branch_if_true(fs,js,x,id);
	elf_fpatchjs(fs,js->f);
	ARRAY_DELETE(js->f);
	js->f = 0;
	return js->t;
}


elByteId *elf_emit_jump_if_false(elFileState *fs, elFileExpr *js, elRegId x, elNodeId id) {
	elf_branch_if_false(fs,js,x,id);
	elf_fpatchjs(fs,js->t);
	ARRAY_DELETE(js->t);
	js->t = 0;
	return js->f;
}

elByteId *elf_emit_jump_if_not_nil(elFileState *fs, elFileline line, elFileExpr *js, elNodeId id) {
	return elf_emit_jump_if_false(fs,js,NO_SLOT,elf_binary_node(fs,line,NODE_EQ,NT_BOL,id,elf_make_nil_node(fs,line)));
}


elByteId *elf_emit_jump_if_nil(elFileState *fs, elFileline line, elFileExpr *js, elNodeId id) {
	return elf_emit_jump_if_true(fs,js,NO_SLOT,elf_binary_node(fs,line,NODE_EQ,NT_BOL,id,elf_make_nil_node(fs,line)));
}


void elf_emit_continue(elFileState *fs, elFileline line, elRegId with_value_register) {
	elASSERT(fs->nloops > 0);
	elFileBlock *bl = elf_emitter_get_loop_block(fs,with_value_register);
	elASSERT(bl != 0);
	elf_emitter_add_block_flags(fs,BLOCK_ENDED);

	elByteId j = elf_emit_jump(fs,line,elf_get_last_byteid(fs));
	ARRAY_ADD(bl->loop.true_jumps,j);
}


void elf_emit_break(elFileState *fs, elFileline line, elRegId with_value_register) {
	elASSERT(fs->nloops > 0);
	elFileBlock *bl = elf_emitter_get_loop_block(fs,with_value_register);
	elASSERT(bl != 0);
	/* 'add_block_flags' is for the current block, which
	has effectively ended, even though we're not
	"targetting" this block itself but the outer one... */
	elf_emitter_add_block_flags(fs,BLOCK_ENDED);
	elByteId j = elf_emit_jump(fs,line,elf_get_last_byteid(fs));
	ARRAY_ADD(bl->leavejumps,j);
}


/* Range Expressions Desugaring */
void elf_emit_desugar_range_expr_epilogue(elFileState *fs, elNodeId x) {
	elNode node = elf_Nget(fs,x);
	switch (node.kind) {
		case NODE_INDEX: case NODE_FIELD: {
			elf_emit_desugar_range_expr_epilogue(fs,node.x);
		} break;
		case NODE_RANGE_INDEX: {
			elf_emitter_close_ranged_loop(fs,NO_LINE);
			elf_fcloseblock(fs);
			elf_emit_desugar_range_expr_epilogue(fs,node.x);
		} break;
		default: ;
	}
		/* if the expression is self contained, for
	instance array[...].items == 1, there's no
	need for emitting any epilogue since it was
	already emitted by the expression itself when
	it desugared its operands */
}


/* On Wednesday, July 24, 2024, 10:40 pm
**
** todo: finish this article...
**
** The idea here is to take an expression that
** is rather complicated and break it down into
** more assimilable parts, for the most part
** these are the projection expressions, which
** take a value and "project" it over a range of
** values.
** The code then has to iterate over each version
** of that value.
** There are cases in which is isn't obvious what the
** meaning of these expressions is if we were to
** derive meaning using a purely logical approach,
** even though I've tried my best to make this language
** as coherent as possible it isn't always the most
** profitable thing to follow this approach.
**
** node.edges[...].color is node.color
**
** There could be many logical interpretations
** one could deduce from this, from a language
** design and formal system point of view, yet
** the interpretation is biased based on what
** I've deemed to be most useful most often.
** Which could of course, be flawed.
** Languages like python offer a more generalist
** approach to this sort of stuff, however I've
** chosen a more natural language flavor to stick
** to. Which, even though I've done a decent job
** at keeping the language as small as possible,
** would imply having a larger vocabulary in
** exchange for compact, more expressive grammar,
** as supposed to more interplay of the same
** language constructs. Which could change either
** way...
**
** In the of boolean expressions:
**
** node.edges[...].color is node.color
**
** I've chosen the following derivation:
**
** node.edges[...].color is node.color ::=
**
** for edge = node.edges[...] ? {
**		if edge.color is node.color ? yield true
** } else yield false
**
**
**	let expr_yield = false
** for _ = node.edges[...] ? {
** 	if _ is node.color ? {
**			expr_yield = true | break
** 	}
** }
**
*/
//
// Something like this: array[0..1]
//
//	for 0..1 ? { <- the code we emitted
//		array[#index] <- what you get
// } <-- loop epilogue emitted once you're done using
// the expression, call the appropriate function.
//
//	For instance:
//
// Something like this: array[0..1][0..2] = 0
//
// Would turns into something like:
// for _ = 0..1 ? { <-- code we emitted
// 	for __ = 0..2 ? { <-- code we emitted
//			array[_,__] = 0 <-- what ever you want to do with it
//	|-->	^^^^^^^^^^^
//	|_______ the expression this fn returns
//		} <-- loop epilogue emitted by caller
// } <-- loop epilogue emitted by caller
//
//
elNodeId elf_emit_desugar_range_expr(elFileState *fs, elNodeId x, elBool flags) {
	elNode node = elf_Nget(fs,x);
	switch (node.kind) {
		case NODE_INDEX: case NODE_FIELD: {
			elNodeId xx = elf_emit_desugar_range_expr(fs,node.x,flags & ~EXPR_LHS);
			return elf_binary_node(fs,node.line,node.kind,NT_ANY,xx,node.y);
		}
		case NODE_RANGE_INDEX: {
			elASSERT(elf_Ngetkind(fs,node.y) == NODE_RANGE);

			elFileline line = node.line;

			elNodeId array,index,value;
			array = elf_emit_desugar_range_expr(fs,node.x,flags & ~EXPR_LHS);

			elBlockId block = elf_fbeginblock(fs,BLOCK_LOOP);

			/*  */
			elRegId array_register = elf_Zlocalize2reg(fs,line,array);
			elRegId index_register = elf_fregalloc2(fs,line,NO_SLOT,NO_NODE);
			/* todo: this is really wasteful, but we can't know whether
			the user will use the #value register or not, can we do this
			some other way cleanly?... Maybe we can take some flags that
			indicate whether to use the additional register or not...
			Or maybe we can instead have the nodes be exposed as supposed
			to the registers, and then if the user does #value we'll
			essentually do #array[#index], which wouldn't be any more
			expensive... */
			elRegId value_register = elf_fregalloc2(fs,line,NO_SLOT,NO_NODE);

			array = elf_Nlocal(fs,line,array_register);
			index = elf_Nlocal(fs,line,index_register);

			value = elf_make_index_node(fs,line,array,index);

			/* todo: could we make this more efficient by caching
			the result */
			elNodeId lo = elf_Nget(fs,node.y).x;
			elNodeId hi = elf_Nget(fs,node.y).y;
			if (lo == NO_NODE) lo = elf_make_integer_node(fs,line,0);
			if (hi == NO_NODE) hi = elf_make_call_metafield_node(fs,line,array,0,"length");

			elf_emitter_begin_ranged_loop(fs,line,index,lo,hi);

			elf_fgetblock(fs,block)->loop.array_register = array_register;
			elf_fgetblock(fs,block)->loop.value_register = elf_Zlocalize2reg(fs,line,value);

			return value;
		}

		default: ;
	}
	return x;
}


void elf_emit_initializer(elFileState *fs, elFileline line, elRegId target_register, elNodeId id) {
	elRegId mem = elf_fgetmem(fs);
	elNode v = fs->nodes[id];
	switch (v.k) {
		case NODE_LOAD: {
			elNode x = fs->nodes[v.x];
			if ((x.kind == NODE_LOCAL)) {
				elNOCODE;
			} else
			if ((x.kind == NODE_FIELD) || (x.k == NODE_INDEX)) {
				// elRegId xx = elf_Zlocalize2reg(fs,line,x.x);
				elRegId xx = target_register;
				elRegId xy = elf_Zlocalize2reg(fs,line,x.y);
				elRegId yy = elf_Zlocalize2reg(fs,line,v.y);
				if (x.k == NODE_FIELD) {
					elf_femitxyz(fs,line,BC_SETFIELD,xx,xy,yy);
				} else elf_femitxyz(fs,line,BC_SETINDEX,xx,xy,yy);
			} else elNOCODE;
		} break;
		default: elNOCODE;
	}
	elf_fsetmem(fs,mem);
}



/*
** Emits code to evaluate the node into any register, if the
** node already has a register no code is emitted.
*/
elRegId elf_Zlocalize2reg(elFileState *fs, elFileline line, elNodeId id) {
	elRegId reg = elf_get_node_register(fs,MAKE_NODE_ID(id));
	if (reg == NO_SLOT) {
		reg = elf_fregalloc2(fs,line,NO_SLOT,id);
		elRegId reg_final = elf_emitter_evaluate(fs,line,0,reg,1,id);
		elASSERT(reg_final == reg);
	}
	return reg;
}


/*
** Reloads the expression a to newly allocated register or
** the given register.
*/
elRegId elf_emitter_relocalize(elFileState *fs, elFileline line, int flags, elNodeIdTypeGuard id) {
	elRegId src = elf_get_node_register(fs,id);
	elRegId dst = elf_fregalloc2(fs,line,NO_SLOT,id.id);
	elASSERT(dst != src);
	elRegId fnl = elf_emitter_evaluate(fs,line,0,dst,1,id.id);
	elASSERT(fnl == dst);
	return dst;
}


/*
** emits bytecode to evaluate given node into
** the target register, if the target node is
** a local, then it is reloaded.
*/
elRegId elf_emitter_evaluate(elFileState *fs, elFileline line, elBool flags, elRegId target_register, elRegId y, elNodeId id) {
	elASSERT(flags == 0);

	elModule *md = fs->M;
	elFileFnState *fn = fs->fn;
	const elNode v = fs->nodes[id];
	const elNode node = v;
	elASSERT(v.level <= fs->level);

	if (v.line != 0) line = v.line;

	elASSERT(target_register < fs->fn->xmemory);

	/* keep track of memory state for allocating temporary
	registers */
	elRegId mem = elf_fgetmem(fs);

	#define UNUSED_CHECK \
	if ((y == 0)) {\
		elf_fdialog(fs,line,"warning: unused expression");\
		goto leave;\
	}

	switch (node.kind) {
		/* todo: remove this? */
		case NODE_LOCAL: case NODE_SPECIAL_REGISTER: {
			UNUSED_CHECK;
			elRegId reg = elf_get_node_register(fs,MAKE_NODE_ID(id));
			elf_femitxy(fs,line,BC_RELOAD,target_register,reg);
		} break;
		case NODE_CLOSURE_VALUE: {
			UNUSED_CHECK;
			elf_femitxy(fs,line,BC_LOADCACHE,target_register,v.x);
		} break;
		case NODE_GLOBAL: {
			UNUSED_CHECK;
			elf_femitxy(fs,line,BC_LOADGLOBAL,target_register,v.x);
		} break;
		case NODE_NIL: {
			UNUSED_CHECK;
			/* -- todo: coalesce */
			elf_femitxy(fs,line,BC_LOADNIL,target_register,y);
		} break;
		case NODE_INTEGER: {
			UNUSED_CHECK;
			/* todo: interning */
			int yy = ARRAY_GROW(fs->M->ki,1);
			fs->M->ki[yy] = v.lit.i;
			elf_femitxy(fs,line,BC_LOADINT,target_register,yy);
		} break;
		case NODE_NUMBER: {
			UNUSED_CHECK;
			/* todo: interning */
			int yy = ARRAY_GROW(fs->M->kn,1);
			fs->M->kn[yy] = node.lit.n;
			elf_femitxy(fs,line,BC_LOADNUM,target_register,yy);
		} break;
		case NODE_STRING: {
			UNUSED_CHECK;
			/* todo: interning */
			elSymbolId global = elf_add_global_value(fs->M,0,elSTR(elf_new_string(fs->R,node.lit.s)));
			elf_femitxy(fs,line,BC_LOADGLOBAL,target_register,global);
		} break;
		case NODE_TABLE: {
			UNUSED_CHECK;
			elf_femitxy(fs,line,BC_TABLE,target_register,0);
			/* todo: could this be turned into sugar */
			FOR_ARRAY(i,v.z) {
				elf_emit_initializer(fs,line,target_register,v.z[i]);
			}
		} break;
		// {x}[{x}]
		case NODE_FIELD: case NODE_INDEX: {
			UNUSED_CHECK;
			if (elf_Ngetkind(fs,v.x) == NODE_RANGE_INDEX) elf_debugger("test-break");
			if (elf_Ngetkind(fs,v.y) == NODE_RANGE_INDEX) elf_debugger("test-break");
			elRegId xx = elf_Zlocalize2reg(fs,line,v.x);
			elRegId yy = elf_Zlocalize2reg(fs,line,v.y);
			elf_femitxyz(fs,line,elf_node_to_byte(v.k),target_register,xx,yy);
		} break;
		case NODE_CLOSURE: {
			UNUSED_CHECK;
			elRegId head = fn->xmemory;
			elRegId tail = head;
			elRegId last = tail;
			FOR_ARRAY(i,v.z) {
				last = elf_emitter_relocalize(fs,line,NO_SLOT,MAKE_NODE_ID(v.z[i]));
				elASSERT(last == tail ++);
			}
			elf_femitxy(fs,line,BC_CLOSURE,head,v.x);
			if (head != target_register) {
				elf_femitxy(fs,line,BC_RELOAD,target_register,head);
			}
		} break;
		case NODE_TYPEGUARD: {
			UNUSED_CHECK;
			elf_emitter_evaluate(fs,line,flags,target_register,y,v.x);
			elf_femitxy(fs,v.line,BC_TYPEGUARD,target_register,elf_nodettotag(v.y));
		} break;
		case NODE_GROUP: {
			UNUSED_CHECK;
			elf_emitter_evaluate(fs,line,flags,target_register,y,v.x);
		} break;
		case NODE_REGION: {
			elNOCODE;
		} break;
		case NODE_METAFIELD: {
			UNUSED_CHECK;
			elRegId rx = elf_Zlocalize2reg(fs,line,v.x);
			elRegId ry = elf_Zlocalize2reg(fs,line,v.y);
			elf_femitxyz(fs,line,elf_node_to_byte(v.k),target_register,rx,ry);
		} break;
		case NODE_CALL: {
			elRegId head = fn->xmemory;
			elRegId tail = head;
			elRegId last = tail;
			/*
			todo: There's a slight intricacy with this, and that
			is that we're not checking for the target node,
			and so something like: ({x}).{y}(...) will not be
			recognized as a meta-call, this could actually be
			a feature since the user could actually intend for this...

			For these cases:

			{x}.{y}(...)
			{x}:{y}(...)

			The emitter isn't smart enough to figure out that
			we've already evaluated {x} and we could reuse that
			expression's register, we do it ourselves here.
			Additionally, we circumvent get field/metafield
			instruction so that the object is already in the
			proper register for the subsequent call instruction.
			This is effectively what a peephole optimizer could
			have achieved.
			But this is a bit simpler and more robust.
		 	*/
			elNode xx = fs->nodes[v.x];
			if ((xx.k == NODE_FIELD) || (xx.k == NODE_METAFIELD)) {

				/* first load the field name, at 'ry', this will
				then be overwritten by the field itself, which
				is the function we're about to call. */
				elRegId ry = last = elf_emitter_relocalize(fs,line,NO_SLOT,MAKE_NODE_ID(xx.y));
				elASSERT(last == tail ++);

				/* 'rx' contains the object (the thing we're getting
				the field from), and we've placed it just under 'ry',
				so this is already in proper order for the call
				instruction */
				elRegId rx = last = elf_emitter_relocalize(fs,line,NO_SLOT,MAKE_NODE_ID(xx.x));
				elASSERT(last == tail ++);

				/* now just emit the get field/metafield instruction, and overwrite
				'ry' with the field value (the function, allegedly) */
				elf_femitxyz(fs,line,elf_node_to_byte(xx.k),ry,rx,ry);
			} else {
				last = elf_emitter_relocalize(fs,line,NO_SLOT,MAKE_NODE_ID(v.x));
				elASSERT(last == tail ++);

				/* Since we are not calling a field or meta field,
				the this argument is implictly the current 'this',
				'this' is always register 0. */
				last = elf_emitter_relocalize(fs,line,NO_SLOT,MAKE_NODE_ID(elf_Nlocal(fs,line,0)));
				elASSERT(last == tail ++);
			}

			/* now load all the user arguments */
			FOR_ARRAY(i, v.z) {
				last = elf_emitter_relocalize(fs,line,NO_SLOT,MAKE_NODE_ID(v.z[i]));
				elASSERT(last == tail ++);
			}

			/* + 1 because of the 'this' argument */
			int nargs = ARRAY_LENGTH(v.z) + 1;
			elf_femitxyz(fs,line,BC_CALL,head,nargs,y);

			/* now we need to emit code to put the results
			of the function at the target_register, if
			we have one... */
			if (target_register != NO_SLOT && y != 0) {
				if (y > 1) {
					elf_fdialog(fs,line,"unsupported");
				}
				elf_femitxy(fs,line,BC_RELOAD,target_register,head);
			}
		} break;
		/* a !! b =
		nil_and = fun(a,b) ? {
			if a == nil ? leave a
			leave b
		} */
		case NODE_NIL_AND: {
			elf_emitter_evaluate(fs,line,LOAD_RELOAD,target_register,1,node.x);
			elFileExpr bool_expr = {0};
			elByteId *js = elf_emit_jump_if_nil(fs,NO_LINE,&bool_expr,elf_Nlocal(fs,NO_LINE,target_register));
			elf_emitter_evaluate(fs,line,LOAD_RELOAD,target_register,1,node.y);
			elf_fpatchjs(fs,js);
			ARRAY_DELETE(js);
		} break;
		case NODE_NIL_OR: {
			elf_emitter_evaluate(fs,line,LOAD_RELOAD,target_register,1,node.x);
			elFileExpr bool_expr = {0};
			elByteId *js = elf_emit_jump_if_not_nil(fs,NO_LINE,&bool_expr,elf_Nlocal(fs,NO_LINE,target_register));
			elf_emitter_evaluate(fs,line,LOAD_RELOAD,target_register,1,node.y);
			elf_fpatchjs(fs,js);
			ARRAY_DELETE(js);
		} break;
		case NODE_AND: case NODE_OR: {
			if (y == 0) goto leave;

			elf_emitter_evaluate(fs,line,LOAD_RELOAD,target_register,1,elf_make_integer_node(fs,line,0));

			elFileExpr bool_expr = {0};
			elByteId *js = elf_emit_jump_if_false(fs,&bool_expr,NO_SLOT,id);

			elf_emitter_evaluate(fs,line,LOAD_RELOAD,target_register,1,elf_make_integer_node(fs,line,1));

			elf_fpatchjs(fs,js);
			ARRAY_DELETE(js);
		} break;
		case NODE_EQ: case NODE_NEQ:
		case NODE_GT: case NODE_GTEQ: case NODE_LT: case NODE_LTEQ:
		/* In the case of relational operators we have some
		we could have sugar */
		if (elf_Ngetkind(fs,node.x) == NODE_RANGE_INDEX) {
			elASSERT(target_register > NO_SLOT);
			/* Set default value to false */
			elNodeId tar = elf_Nlocal(fs,node.line,target_register);
			elf_Zstore(fs,node.line,tar,elf_make_integer_node(fs,NO_LINE,0));

			/* ensure desugaring actually took place since
			we know node.x is a projection */
			elNodeId xx = elf_emit_desugar_range_expr(fs,node.x,flags);
			if (xx == node.x) {
				elf_rthrow(fs->R,NO_BYTE,"internal error");
			}

			elASSERT(elf_Ngetkind(fs,xx) == NODE_INDEX);

			elNodeId *z = {0};
			ARRAY_ADD(z,elf_Nget(fs,xx).y);

			xx = elf_make_call_metafield_node(fs,line,elf_Nget(fs,xx).x,z,"idx");

			/* convert node which operates on a projection
			to operate on an actual item */
			/* todo: here we can make some optimizations, we can
			hoist value evaluation (node.y) outside of the loop and
			keep in a register. */
			elNodeId per = elf_binary_node(fs,node.line,NODE_EQ,NT_ANY,xx,node.y);
			/* todo: Could we massively simplify things
			by passing in the register directly to jump
			emitters? I could just emit code to localize
			the expression? But not really because then
			we wouldn't optimize for short circuiting,
			which doesn't use any registers, so we'd have
			to that system more public for us to play with
			it more directly. Maybe even have an intermediate
			value structure which holds all the jumps from
			short circuiting and a final node which represents
			the thing that is actually evaluated when either true
			or false... */
			elFileExpr expr = {0};
			/* if true we can exit the loop */
			elByteId *js = elf_emit_jump_if_false(fs,&expr,NO_SLOT,elf_binary_node(fs,line,NODE_EQ,NT_BOL,xx,node.y));

			elf_Zstore(fs,NO_LINE,tar,elf_make_integer_node(fs,NO_LINE,1));
			elf_emit_break(fs,NO_LINE,NO_SLOT);
			/* otherwise continue checking */
			elf_fpatchjs(fs,js);
			/* finally, emit any necessary epilogue for
			the desugaring process */
			elf_emit_desugar_range_expr_epilogue(fs,node.x);
			break;
		 /* fallthrough */
		} else;
		case NODE_DIV: case NODE_MUL: case NODE_MOD:
		case NODE_SUB: case NODE_ADD: case NODE_POW:
		case NODE_BIT_SHL: case NODE_BIT_SHR:
		case NODE_BIT_XOR:
		case NODE_BIT_AND: case NODE_BIT_OR: {
			/* todo: do this properly */
			// if (y == 0) goto leave;
			if ((v.k == NODE_GT) || (v.k == NODE_GTEQ)) {
				elRegId xx = elf_Zlocalize2reg(fs,line,v.y);
				elRegId yy = elf_Zlocalize2reg(fs,line,v.x);
				elf_femitxyz(fs,line,elf_node_to_byte(v.k^1),target_register,xx,yy);
			} else if (v.k == NODE_EQ) {
				/* todo: enable this */
				if(1) goto _else;
				if (elf_Ngetkind(fs,v.y) == NODE_NIL) {
					elRegId xx = elf_Zlocalize2reg(fs,line,v.y);
					elf_femitxy(fs,line,BC_ISNIL,target_register,xx);
				} else goto _else;
			} else { _else:
				elRegId rx = elf_Zlocalize2reg(fs,line,v.x);
				elRegId ry = elf_Zlocalize2reg(fs,line,v.y);
				elf_femitxyz(fs,line,elf_node_to_byte(v.k),target_register,rx,ry);
			}
		} break;
		default: {
			elf_fdialog(fs,line,"invalid node (%s)",node2s[node.kind]);
			elNOCODE;
		}
	}

	leave:
	elf_fsetmem(fs,mem);
	return target_register;
}


void elf_Zstore(elFileState *fs, elFileline line, elNodeId x, elNodeId y) {
	elNode v = elf_get_targetable_node(fs,MAKE_NODE_ID(x));
	elASSERT(elf_is_targetable_node(v.kind));
	elASSERT(v.level <= fs->level);
	elASSERT(x >= 0);
	elASSERT(y >= 0);
	if (v.line != 0) line = v.line;

	elRegId mem = elf_fgetmem(fs);
	switch (v.k) {
		case NODE_GLOBAL: {
			elRegId yy = elf_Zlocalize2reg(fs,line,y);
			elf_femitxy(fs,line,BC_SETGLOBAL,v.x,yy);
		} break;
		case NODE_LOCAL: {
			elf_emitter_evaluate(fs,line,0,v.x,1,y);
		} break;
		case NODE_CLOSURE_VALUE: {
			elf_fdialog(fs,line,"assignment to closure value is not possible");
		} break;
		// {x}.a.z
		// get_field(register 0, get_global({x}), get_string("a"))
		// get_string(register 1, "b")
		// get_field(register 0, 0, 1)
		// get_global(register 0, {x})
		// get_string(register 1, "a")
		// get_field(register 0, 0, 1)
		// get_string(register 1, "b")
		// get_field(register 0, 0, 1)
		case NODE_INDEX: case NODE_FIELD: {
			elNodeId table_node = v.x;
			elNodeId field_node = v.y;
			elNodeId value_node = y;
			elRegId table_register = elf_Zlocalize2reg(fs,line,table_node);
			elRegId field_register = elf_Zlocalize2reg(fs,line,field_node);
			elRegId value_register = elf_Zlocalize2reg(fs,line,value_node);
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


/* todo: this can be removed */
void elf_Fbeginif(elFileState *fs, elFileline line, elSelectState *s, elNodeId x, int z) {
	elFileExpr js = {0};
	elf_emit_branch_if(fs,&js,z,NO_SLOT,x);
	// if  0 = jz
	// iff 1 = jnz
	if (z == L_IF) {
		elASSERT(js.f != 0);
		elf_fpatchjs(fs,js.t);
		ARRAY_DELETE(js.t);
		js.t = 0;
		s->jz = js.f;
	} else {
		elASSERT(js.t != 0);
		elf_fpatchjs(fs,js.f);
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
void elf_Faddelseclause(elFileState *fs, elFileline line, elSelectState *s) {
	if (s->jz == 0) {
		elf_fdialog(fs,line,"invalid else clause");
	}
	elASSERT(s->jz != 0);
	int j = elf_emit_jump(fs,line,-1);
	ARRAY_ADD(s->j,j);

	elf_fpatchjs(fs,s->jz);
	ARRAY_DELETE(s->jz);
	s->jz = 0;
}


void elf_Faddelifclause(elFileState *fs, elFileline line, elSelectState *s, int x) {
	elf_Faddelseclause(fs,line,s);
	elf_Fbeginif(fs,line,s,x,L_IF);
}


void elf_Faddthenclause(elFileState *fs, elFileline line, elSelectState *s) {
	/* we don't need to close the previous block, it can just fall
	through to our branch, do collect all the other exit jumps and
	tie them to this branch block, naturally we don't need to add
	an exit jump since else and elif or closeif will terminate
	this block, multiple then blocks are simply chained together
	naturally. */
	elf_fpatchjs(fs,s->j);
	ARRAY_DELETE(s->j);
	s->j = 0;
}


void elf_Fcloseif(elFileState *fs, elFileline line, elSelectState *s) {
	/* collect missing else branch */
	if (s->jz != 0) {
		elf_fpatchjs(fs,s->jz);
		ARRAY_DELETE(s->jz);
		s->jz = 0;
	}
	/* collect missing then branch */
	if (s->j != 0) {
		elf_fpatchjs(fs,s->j);
		ARRAY_DELETE(s->j);
		s->j = 0;
	}
}



/* todo: add support for multiple results */
void elf_Zyield(elFileState *fs, elFileline line, elNodeId id) {
	elRegId regress = elf_fgetmem(fs);
	if (id != NO_NODE) {
		/* todo: determine the number of values in
		tree, and allocate that many registers? */
		int n = 1;
		/* todo: if we only return one value we don't have
		to reload */
		elRegId x = elf_emitter_relocalize(fs,line,NO_SLOT,MAKE_NODE_ID(id));
		if (fs->fn->nyield < n) fs->fn->nyield = n;
		elByteId j = elf_femitxyz(fs,line,BC_YIELD,NO_JUMP,x,n);
		ARRAY_ADD(fs->fn->yj,j);
		/* if there are no results then simply leave directly */
	} else elf_femitx(fs,line,BC_LEAVE,0);
	elf_fsetmem(fs,regress);

	elf_emitter_add_block_flags(fs,BLOCK_ENDED);
}


void elf_Fbegindowhileloop(elFileState *fs, elFileline line) {
	elFileBlock *bl = elf_fgetblock(fs,-1); // fs->fn->block
	elASSERT(bl->flags & BLOCK_LOOP);
	bl->loop.entry = elf_get_last_byteid(fs);
	bl->loop.false_jumps = 0;
	bl->loop.x = NO_NODE;
	bl->loop.index_register = NO_SLOT;
}


void elf_Fclosedowhileloop(elFileState *fs, elFileline line, elNodeId x) {
	elFileBlock *bl = elf_fgetblock(fs,-1); // fs->fn->block;
	elASSERT(bl->flags & BLOCK_LOOP);

	elFileExpr js = {0};
	elf_emit_jump_if_true(fs,&js,NO_SLOT,x);

	elf_emitter_patch_jumplist_to(fs,js.t,bl->loop.entry);
	ARRAY_DELETE(js.t);
	js.t = 0;

	elf_emitter_patch_jumplist_to(fs,bl->loop.true_jumps,bl->loop.entry);
	ARRAY_DELETE(bl->loop.true_jumps);
	bl->loop.true_jumps = 0;
}


void elf_Fbeginwhileloop(elFileState *fs, elFileline line, elNodeId x) {
	elFileBlock *bl = elf_fgetblock(fs,-1);
	elASSERT(bl->flags & BLOCK_LOOP);

	bl->loop.entry = elf_get_last_byteid(fs);
	bl->loop.x = x;

	elASSERT(bl->loop.false_jumps == 0);

	elFileExpr js = {0};
	bl->loop.false_jumps = elf_emit_jump_if_false(fs,&js,NO_SLOT,x);
}


void elf_Fclosewhileloop(elFileState *fs, elFileline line) {
	elFileBlock *bl = elf_fgetblock(fs,-1);
	elASSERT(bl->flags & BLOCK_LOOP);

	elf_fpatchjs(fs,bl->loop.true_jumps);
	ARRAY_DELETE(bl->loop.true_jumps);
	bl->loop.true_jumps = 0;
	elf_emit_jump(fs,line,bl->loop.entry);
	elf_fpatchjs(fs,bl->loop.false_jumps);
	ARRAY_DELETE(bl->loop.false_jumps);
	bl->loop.false_jumps = 0;
}


void elf_emitter_begin_ranged_loop(elFileState *fs, elFileline line, elNodeId index_node, elNodeId lo, elNodeId hi) {
	elFileBlock *bl = elf_fgetblock(fs,-1);
	elASSERT(bl->flags & BLOCK_LOOP);

	elASSERT(index_node != NO_NODE);

	elRegId index_register = elf_Zlocalize2reg(fs,line,index_node);
	index_node = elf_Nlocal(fs,line,index_register);

	bl->loop.index_register = index_register;
	bl->loop.x = index_node;
	elf_emitter_evaluate(fs,line,1,index_register,1,elf_make_type_guard_node(fs,elf_get_node_line(fs,lo),lo,NT_INT));

	bl->loop.entry = elf_get_last_byteid(fs);

	elRegId hi_register = elf_Zlocalize2reg(fs,line,elf_make_type_guard_node(fs,elf_get_node_line(fs,hi),hi,NT_INT));
	hi = elf_Nlocal(fs,line,hi_register);
	elNodeId c = elf_make_node_less_than(fs,line,index_node,hi);

	elASSERT(bl->loop.false_jumps == 0);

	elFileExpr js = {0};
	bl->loop.false_jumps = elf_emit_jump_if_false(fs,&js,NO_SLOT,c);
}


void elf_emitter_close_ranged_loop(elFileState *fs, elFileline line) {
	elFileBlock *bl = elf_fgetblock(fs,-1); // fs->fn->block;
	elASSERT(bl->flags & BLOCK_LOOP);

	elf_fpatchjs(fs,bl->loop.true_jumps);
	ARRAY_DELETE(bl->loop.true_jumps);
	bl->loop.true_jumps = 0;
	// elNodeId index_node = bl->loop.index_node;
	elNodeId index_node = elf_Nlocal(fs,NO_LINE,bl->loop.index_register);
	elNodeId k = elf_binary_node(fs,NO_LINE,NODE_ADD,NT_INT,index_node,elf_make_integer_node(fs,NO_LINE,1));
	elf_Zstore(fs,line,index_node,k);
	elf_emit_jump(fs,line,bl->loop.entry);

	elf_fpatchjs(fs,bl->loop.false_jumps);
	ARRAY_DELETE(bl->loop.false_jumps);
	bl->loop.false_jumps = 0;
}


elByteOP elf_node_to_byte(elNodeKi tt) {
	switch (tt) {
		case NODE_FIELD: return BC_FIELD;
		case NODE_INDEX: return BC_INDEX;
		case NODE_CALL: return BC_CALL;
		case NODE_METAFIELD: return BC_METAFIELD;
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

