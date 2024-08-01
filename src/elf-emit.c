/*
** See Copyright Notice In elf.h
** elf-emit.c
** Bytecode Generator (node -> bytecode)
*/



elByteOP elf_node_to_byte(elNodeKi tt);


elByteId elf_get_last_byteid(elFileState *fs) {
	return fs->M->nbytes;
}


elRegId elf_get_memory_state(elFileState *fs) {
	return fs->fn->xmemory;
}


void elf_set_memory_state(elFileState *fs, elRegId memory) {
	// int dif = memory - fs->fn->xmemory;
	// if (dif != 0) {
	// 	elf_debug_log("restore memory %i -> %i (%i)",fs->fn->xmemory,memory,dif);
	// }
	fs->fn->xmemory = memory;
}


elBool elf_is_targetable_node(elNodeKi kind) {
	switch (kind) {
		case NODE_GLOBAL:
		case NODE_LOCAL:
		case NODE_RANGE_INDEX:
		case NODE_INDEX: case NODE_FIELD: {
			return elTrue;
		}
		default: {
			return elFalse;
		}
	}
}


elNode elf_get_targetable_node(elFileState *fs, elNodeIdTypeGuard id) {
	elNode node = elf_get_node(fs,id.id);
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
					elf_file_dialog(fs,node.line,"#array register is not provided by this loop");
				}
			} break;
			case SPECIAL_REGISTER_VALUE: {
				reg = bl->loop.value_register;
				if (reg == NO_SLOT) {
					elf_file_dialog(fs,node.line,"#value register is not provided by this loop");
				}
			} break;
			case SPECIAL_REGISTER_INDEX: {
				reg = bl->loop.index_register;
				if (reg == NO_SLOT) {
					elf_file_dialog(fs,node.line,"#index register is not provided by this loop");
				}
			} break;
			default: elNOCODE;
		}
		return reg;
	}
	return NO_SLOT;
}


elRegId elf_emitter_local_alloc(elFileState *fs, elFileLine line, int _, int __) {
	(void) _;
	(void) __;
	elFileFnState *fn = fs->fn;
	elRegId reg = fn->xmemory ++;
	fn->nlocals = MAX(fn->nlocals,fn->xmemory);
	return reg;
}


elFileLine elf_emitter_get_line(elFileState *fs, elByteId id) {
	return fs->M->lines[id];
}


elBytecode elf_emitter_get_byte(elFileState *fs, elByteId id) {
	return fs->M->bytes[id];
}


elByteId elf_emitter_add_byte(elFileState *fs, elFileLine line, elBytecode byte) {
	/* we just associate each byte with a line,
	this is simple and is pretty great for debugging... */
	ARRAY_ADD(fs->M->lines,line);
	ARRAY_ADD(fs->M->bytes,byte);
	ARRAY_ADD(fs->M->track,0);
	// elFileFnState *fn = fs->fn;
	// elf_bytefpf(stdout,fs->M,-1,fs->M->nbytes-fn->bytes,byte);
	return fs->M->nbytes ++;
}


elByteId elf_emitter_add_byteop(elFileState *fs, elFileLine line, elByteOP k, elInteger i) {
	return elf_emitter_add_byte(fs,line,(elBytecode){k,i});
}


elByteId elf_emitter_add_bytexy(elFileState *fs, elFileLine line, elByteOP k, int x, int y) {
	elBytecode b = (elBytecode){k};
	b.x = x, b.y = y;
	return elf_emitter_add_byte(fs,line,b);
}


elByteId elf_emitter_add_bytexyz(elFileState *fs, elFileLine line, elByteOP k, int x, int y, int z) {
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
	// 	elf_file_dialog(fs,M->lines[to],"opt, no jump");
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
	elf_xarray_foreachi(js) {
		elf_emitter_patch_jump_to(fs,js[i],j);
	}
}


void elf_emitter_patch_jump(elFileState *fs, elByteId i) {
	elf_emitter_patch_jump_to(fs,i,elf_get_last_byteid(fs));
}


void elf_emitter_patch_jumplist(elFileState *fs, elByteId *js) {
	elf_xarray_foreachi(js) {
		elf_emitter_patch_jump(fs,js[i]);
	}
}


elByteId elf_emit_jump(elFileState *fs, elFileLine line, elByteId j) {
	return elf_emitter_add_byteop(fs,line,BC_J,j - fs->M->nbytes);
}



elFileBlock *elf_emitter_get_block(elFileState *fs, elBlockId id) {
	return &fs->blocks[id > -1 ? id : fs->nblocks + id];
}


/* todo: the block ended flag could be added automatically when
we add a terminating byte to the current block */
void elf_emitter_add_block_flags(elFileState *fs, int flags) {
	elf_emitter_get_block(fs,-1)->flags |= flags;
}


elBlockId elf_emitter_begin_block(elFileState *fs, elBool flags) {
	elBlockId level = fs->nblocks ++;
	if (array_length(fs->blocks) < fs->nblocks) {
		elf_xarray_growby(fs->blocks,1);
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
	fs->fn->nloops += (flags & BLOCK_LOOP) != 0;
	// fs->fn->nblocks += 1;
	return level;
}


void elf_emitter_close_block(elFileState *fs) {
	elASSERT(fs->nentities >= fs->fn->entities);
	elFileBlock *bl = elf_emitter_get_block(fs,-1);
	elEntityId id;
	/* xentity is the first entity within a block, if any. */
	for (id = bl->xentity; id < fs->nentities; ++ id) {
		if (~fs->entities[id].flags & ENTITY_REFERENCED) {
			elf_file_dialog(fs,fs->entities[id].line,"unreferenced entity");
		}
	}
	fs->nentities = bl->xentity;
	fs->nnodes = bl->xnode;
	fs->level -= 1;
	// elASSERT(bl->level == fs->level);
	// fs->fn->block = bl->enclosing;
	fs->fn->xmemory = bl->xmemory;
	if (bl->leavejumps != 0) {
		elf_emitter_patch_jumplist(fs,bl->leavejumps);
		elf_xarray_delete(bl->leavejumps);
		bl->leavejumps = 0;
	}
	fs->fn->nloops -= (bl->flags & BLOCK_LOOP) != 0;
}


elFileBlock *elf_emitter_get_loop_block(elFileState *fs, elRegId with_value_register) {
	int level;
	for (level = fs->level-1; level > -1; -- level) {
		elFileBlock *bl = elf_emitter_get_block(fs,level);
		if (bl->flags & BLOCK_LOOP) {
			if (with_value_register < 0) return bl; else
			if (bl->loop.value_register == with_value_register) return bl;
		}
	}
	return 0;
}


/* we could merge with adjacent blocks, but this
would change the order of execution, leave as is? */
void elf_emitter_enter_delayed_block(elFileState *fs, elFileLine line) {//, elFileBlock *bl
	elByteId jo = elf_emitter_add_byteop(fs,line,BC_DELAY,NO_JUMP);
	elBlockId id = elf_emitter_begin_block(fs,BLOCK_DELAYED);//bl,
	fs->blocks[id].jumpover = jo;
}


void elf_emitter_leave_delayed_block(elFileState *fs, elFileLine line) {//, elFileBlock *bl
	// elf_file_dialog(fs,line,"closed block, %i",langL_getlocallabel(fs));
	elFileBlock *bl = elf_emitter_get_block(fs,-1); // fn = fs->fn;

	elf_emitter_add_byteop(fs,line,BC_LEAVE,0);
	elf_emitter_add_block_flags(fs,BLOCK_ENDED);
	elf_emitter_close_block(fs);

	elASSERT(bl->entry != bl->jumpover);
	elf_emitter_patch_jump(fs,bl->jumpover);
}


/*
** Evaluates the given boolean or logical expression using short
** circuit evaluation, creating branches as it evaluates each
** expression, only one register is necessary, if no registers
** are given one is allocated and deallocated automatically.
*/
elByteId elf_emit_branch_if(elFileState *fs, elFileBoolExpr *js, elBool z, elRegId x, elNodeId id) {
	elByteId j = NO_BYTE;

	elASSERT(x == NO_SLOT);
	elRegId mem = elf_get_memory_state(fs);
	// /* allocate temporary register before diverging. */
	// if (x == NO_SLOT) {
	// 	x = elf_emitter_local_alloc(fs,NO_LINE,NO_SLOT,id);
	// }

	elNode node = elf_get_node(fs,id);

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
			// elf_emitter_local_load(fs,NO_LINE,false,x,1,id);
			elASSERT(x == NO_SLOT);
			x = elf_emitter_localize(fs,NO_LINE,id);

			if (z != 0) {
				j = elf_emitter_add_bytexy(fs,node.line,BC_JNZ,NO_JUMP,x);
				ARRAY_ADD(js->t,j);
			} else {
				j = elf_emitter_add_bytexy(fs,node.line,BC_JZ,NO_JUMP,x);
				ARRAY_ADD(js->f,j);
			}
		} break;
	}

	elf_set_memory_state(fs,mem);
	return j;
}


elByteId elf_branch_if_false(elFileState *fs, elFileBoolExpr *js, elRegId x, elNodeId id) {
	return elf_emit_branch_if(fs,js,elFalse,x,id);
}


elByteId elf_branch_if_true(elFileState *fs, elFileBoolExpr *js, elRegId x, elNodeId id) {
	return elf_emit_branch_if(fs,js,elTrue,x,id);
}


/*
** Similar to branch if true, but additionally this byte
** address becomes the false target, thus all false
** branches converge here. All true branches are returned.
*/
elByteId *elf_emit_jump_if_true(elFileState *fs, elFileBoolExpr *js, elRegId x, elNodeId id) {
	elf_branch_if_true(fs,js,x,id);
	elf_emitter_patch_jumplist(fs,js->f);
	elf_xarray_delete(js->f);
	js->f = elNil;
	return js->t;
}


elByteId *elf_emit_jump_if_false(elFileState *fs, elFileBoolExpr *js, elRegId x, elNodeId id) {
	elf_branch_if_false(fs,js,x,id);
	elf_emitter_patch_jumplist(fs,js->t);
	elf_xarray_delete(js->t);
	js->t = elNil;
	return js->f;
}

elByteId *elf_emit_jump_if_not_nil(elFileState *fs, elFileLine line, elFileBoolExpr *js, elNodeId id) {
	return elf_emit_jump_if_false(fs,js,NO_SLOT,elf_make_binary_node(fs,line,NODE_EQ,NT_BOL,id,elf_make_nil_node(fs,line)));
}

elByteId *elf_emit_jump_if_nil(elFileState *fs, elFileLine line, elFileBoolExpr *js, elNodeId id) {
	return elf_emit_jump_if_true(fs,js,NO_SLOT,elf_make_binary_node(fs,line,NODE_EQ,NT_BOL,id,elf_make_nil_node(fs,line)));
}


void elf_emit_continue(elFileState *fs, elFileLine line, elRegId with_value_register) {
	elASSERT(fs->fn->nloops > 0);
	elFileBlock *bl = elf_emitter_get_loop_block(fs,with_value_register);
	elASSERT(bl != 0);
	elf_emitter_add_block_flags(fs,BLOCK_ENDED);

	elByteId j = elf_emit_jump(fs,line,elf_get_last_byteid(fs));
	ARRAY_ADD(bl->loop.true_jumps,j);
}


void elf_emit_break(elFileState *fs, elFileLine line, elRegId with_value_register) {
	elASSERT(fs->fn->nloops > 0);
	elFileBlock *bl = elf_emitter_get_loop_block(fs,with_value_register);
	elASSERT(bl != 0);
	/* fs->level is one past the current level :( */
	if (bl->level == fs->level-1) {
		elf_emitter_add_block_flags(fs,BLOCK_ENDED);
	}
	elByteId j = elf_emit_jump(fs,line,elf_get_last_byteid(fs));
	ARRAY_ADD(bl->leavejumps,j);
}

/* Functions */

/* todo: remove this */
void elf_emit_function_epilogue(elFileState *fs, elFileLine line) {
	elModule *md = fs->M;
	elFileFnState *fn = fs->fn;

	/* todo: rename this to be clearer. */
	elf_emitter_patch_jumplist(fs,fn->yj);
	elf_xarray_delete(fn->yj);
	fn->yj = elNil;

	/* finally, return control flow... */
	elf_emitter_add_byteop(fs,line,BC_LEAVE,0);
}


void elf_emitter_begin_function(elFileState *fs, elFileFnState *fn, char *line) {
	fn->enclosing = fs->fn;
	fn->entities = fs->nentities;
	fn->bytes = fs->md->nbytes;
	fn->line = line;
	fn->yj = elNil;
	/* todo: "begin_block" requires fn to be set
	for xmemory, can xmemory simply be in the
	file state instead? */
	fs->fn = fn;
	fn->entry_block = elf_emitter_begin_block(fs,0);
}


void elf_emitter_close_function(elFileState *fs) {
	elf_emit_function_epilogue(fs,fs->last_token.line);
	elf_emitter_close_block(fs);
	elASSERT(fs->fn->entry_block == fs->level);
	/* ensure all locals were deallocated
	properly */
	elASSERT(fs->nentities == fs->fn->entities);
	fs->fn = fs->fn->enclosing;
}

/* Range Expressions Desugaring */
void elf_emit_desugar_range_expr_epilogue(elFileState *fs, elNodeId x) {
	elNode node = elf_get_node(fs,x);
	switch (node.kind) {
		case NODE_INDEX: case NODE_FIELD: {
			elf_emit_desugar_range_expr_epilogue(fs,node.x);
		} break;
		case NODE_RANGE_INDEX: {
			elf_emitter_close_ranged_loop(fs,NO_LINE);
			elf_emitter_close_block(fs);
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
** There are cases in which is isn't straight forward
** to translate some of these expressions into actual
** code if we were to take a purely systematic and or
** perhaps mathematical approach, even though I've
** tried my best to make this language as coherent as
** possible, for instance take the expression:
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
** In the case of the boolean expression case:
**
** node.edges[...].color is node.color
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
	elNode node = elf_get_node(fs,x);
	switch (node.kind) {
		case NODE_INDEX: case NODE_FIELD: {
			elNodeId xx = elf_emit_desugar_range_expr(fs,node.x,flags & ~FILE_LHS);
			return elf_make_binary_node(fs,node.line,node.kind,NT_ANY,xx,node.y);
		}
		case NODE_RANGE_INDEX: {
			elASSERT(elf_get_node_kind(fs,node.y) == NODE_RANGE);

			elFileLine line = node.line;

			elNodeId array,index,value;
			array = elf_emit_desugar_range_expr(fs,node.x,flags & ~FILE_LHS);

			elBlockId block = elf_emitter_begin_block(fs,BLOCK_LOOP);

			/*  */
			elRegId array_register = elf_emitter_localize(fs,line,array);
			elRegId index_register = elf_emitter_local_alloc(fs,line,NO_SLOT,NO_NODE);
			/* todo: this is really wasteful, but we can't know whether
			the user will use the #value register or not, can we do this
			some other way cleanly?... Maybe we can take some flags that
			indicate whether to use the additional register or not...
			Or maybe we can instead have the nodes be exposed as supposed
			to the registers, and then if the user does #value we'll
			essentually do #array[#index], which wouldn't be any more
			expensive... */
			elRegId value_register = elf_emitter_local_alloc(fs,line,NO_SLOT,NO_NODE);

			array = elf_make_register_node(fs,line,array_register);
			index = elf_make_register_node(fs,line,index_register);

			value = elf_make_index_node(fs,line,array,index);

			/* todo: could we make this more efficient by caching
			the result */
			elNodeId lo = elf_get_node(fs,node.y).x;
			elNodeId hi = elf_get_node(fs,node.y).y;
			if (lo == NO_NODE) lo = elf_make_integer_node(fs,line,0);
			if (hi == NO_NODE) hi = elf_make_call_metafield_node(fs,line,array,0,"length");

			elf_emitter_begin_ranged_loop(fs,line,index,lo,hi);

			elf_emitter_get_block(fs,block)->loop.array_register = array_register;
			elf_emitter_get_block(fs,block)->loop.value_register = elf_emitter_localize(fs,line,value);

			return value;
		}

		default: ;
	}
	return x;
}


void elf_emit_initializer(elFileState *fs, elFileLine line, elRegId target_register, elNodeId id) {
	elRegId mem = elf_get_memory_state(fs);
	elNode v = fs->nodes[id];
	switch (v.k) {
		case NODE_LOAD: {
			elNode x = fs->nodes[v.x];
			if ((x.kind == NODE_LOCAL)) {
				elNOCODE;
			} else
			if ((x.kind == NODE_FIELD) || (x.k == NODE_INDEX)) {
				// elRegId xx = elf_emitter_localize(fs,line,x.x);
				elRegId xx = target_register;
				elRegId xy = elf_emitter_localize(fs,line,x.y);
				elRegId yy = elf_emitter_localize(fs,line,v.y);
				if (x.k == NODE_FIELD) {
					elf_emitter_add_bytexyz(fs,line,BC_SETFIELD,xx,xy,yy);
				} else elf_emitter_add_bytexyz(fs,line,BC_SETINDEX,xx,xy,yy);
			} else elNOCODE;
		} break;
		default: elNOCODE;
	}
	elf_set_memory_state(fs,mem);
}



/*
** Emits code to evaluate the node into any register, if the
** node already has a register no code is emitted.
*/
elRegId elf_emitter_localize(elFileState *fs, elFileLine line, elNodeId id) {
	elRegId target_register = elf_get_node_register(fs,MAKE_NODE_ID(id));
	if (target_register == NO_SLOT) {
		target_register = elf_emitter_local_alloc(fs,line,NO_SLOT,id);
		// elRegId final_register = elf_emitter_local_load(fs,line,0,target_register,1,id);
		// elASSERT(final_register == target_register);
	}
	elRegId final_register = elf_emitter_local_load(fs,line,0,target_register,1,id);
	elASSERT(final_register == target_register);
	return target_register;
}


/*
** Reloads the expression a to newly allocated register or
** the given register.
*/
elRegId elf_emitter_relocalize(elFileState *fs, elFileLine line, elRegId target_register, elNodeIdTypeGuard id) {
	elRegId source_register = elf_get_node_register(fs,id);
	if (target_register == NO_SLOT) {
		target_register = elf_emitter_local_alloc(fs,line,NO_SLOT,id.id);
	}
	/* whenever you call this function is becuase you want to
	move some node but also, it's always to a new register */
	elASSERT(target_register != source_register);
	// if (target_register != source_register) {
	elRegId final_register = elf_emitter_local_load(fs,line,LOAD_RELOAD,target_register,1,id.id);
	elASSERT(final_register == target_register);
	// }
	return target_register;
}

/*
** emits bytecode to evaluate given node into
** the target register.
*/
elRegId elf_emitter_local_load(elFileState *fs, elFileLine line
, 	elBool flags, elRegId target_register
, 	elRegId y, elNodeId id) {

	elModule *md = fs->M;
	elFileFnState *fn = fs->fn;

	const elNode v = fs->nodes[id];
	const elNode node = v;

	elASSERT(v.level <= fs->level);

	if (v.line != 0) line = v.line;

	// elASSERT(target_register > NO_SLOT);
	elASSERT(target_register < fs->fn->xmemory);

	if (target_register == NO_SLOT) {
		if (flags & LOAD_ALLOCATE) {
			elf_file_dialog(fs,line,"here!");
			__debugbreak();
			target_register = elf_emitter_local_alloc(fs,line,NO_SLOT,id);
		}
	}

	/* keep track of memory state for allocating temporary
	variables */
	elRegId mem = elf_get_memory_state(fs);

	/* todo: this is flawed, if we have some node like a
	type-guard wrapping a local, 'get_register' will find
	the register of the node local within, which will
	cause the type-guard not to be emitted, one solution
	would be to avoid doing this sort of logic here or
	checking only whether the node itself has a register
	and not some target node within... */
	elRegId already_register = elf_get_node_register(fs,MAKE_NODE_ID(id));
	/* if node is already local and no reloading is necessary */
	if ((target_register != NO_SLOT) && (already_register != NO_SLOT) && (already_register != target_register)) {
		if (flags & LOAD_RELOAD) {
			elf_emitter_add_bytexy(fs,line,BC_RELOAD,target_register,already_register);
			// target_register = already_register;
			goto leave;
		} else {
			__debugbreak();
			elf_emitter_add_bytexy(fs,line,BC_RELOAD,target_register,already_register);
			// elf_debug_log("node is already localized and no reloading is necessary, node: %i, target_register: %i, already_register: %i, (reload: %s)"
			// , id, target_register, already_register, (flags & LOAD_RELOAD) ? "true" : "false");
			target_register = already_register;
			goto leave;
		}
	}
	// elf_node_fpf(fs,stdout,id);
	// printf("\n");

	#define UNUSED_CHECK \
	if ((y == 0)) {\
		elf_file_dialog(fs,line,"warning: unused expression");\
		goto leave;\
	}

	switch (node.kind) {
		/* todo: remove this? */
		case NODE_LOCAL: case NODE_SPECIAL_REGISTER: {
			UNUSED_CHECK;
			elRegId reg = elf_get_node_register(fs,MAKE_NODE_ID(id));
			/* The compiler knows this node doesn't have to be reloaded,
			so if we got here it means it wants us to reload to some
			other register */
			elf_emitter_add_bytexy(fs,line,BC_RELOAD,target_register,reg);
		} break;
		case NODE_CLOSURE_VALUE: {
			UNUSED_CHECK;
			elf_emitter_add_bytexy(fs,line,BC_LOADCACHE,target_register,v.x);
		} break;
		case NODE_THIS: {
			UNUSED_CHECK;
			elf_emitter_add_byteop(fs,line,BC_LOADTHIS,target_register);
		} break;
		case NODE_GLOBAL: {
			UNUSED_CHECK;
			elf_emitter_add_bytexy(fs,line,BC_LOADGLOBAL,target_register,v.x);
		} break;
		case NODE_NIL: {
			UNUSED_CHECK;
			/* -- todo: coalesce */
			elf_emitter_add_bytexy(fs,line,BC_LOADNIL,target_register,y);
		} break;
		case NODE_INTEGER: {
			UNUSED_CHECK;
			/* todo: interning */
			int yy = elf_xarray_growby(fs->M->ki,1);
			fs->M->ki[yy] = v.lit.i;
			elf_emitter_add_bytexy(fs,line,BC_LOADINT,target_register,yy);
		} break;
		case NODE_NUMBER: {
			UNUSED_CHECK;
			/* todo: interning */
			int yy = elf_xarray_growby(fs->M->kn,1);
			fs->M->kn[yy] = node.lit.n;
			elf_emitter_add_bytexy(fs,line,BC_LOADNUM,target_register,yy);
		} break;
		case NODE_STRING: {
			UNUSED_CHECK;
			/* todo: interning */
			int g = elf_add_global_value(fs->M,0,elf_string_value(elf_new_string(fs->rt,v.lit.s)));
			elf_emitter_add_bytexy(fs,line,BC_LOADGLOBAL,target_register,g);
		} break;
		case NODE_TABLE: {
			UNUSED_CHECK;
			elf_emitter_add_bytexy(fs,line,BC_TABLE,target_register,0);
			elf_xarray_foreachi(v.z) elf_emit_initializer(fs,line,target_register,v.z[i]);
		} break;
		// {x}[{x}]
		case NODE_FIELD: case NODE_INDEX: {
			UNUSED_CHECK;
			if (elf_get_node_kind(fs,v.x) == NODE_RANGE_INDEX) elf_debugger("test-break");
			if (elf_get_node_kind(fs,v.y) == NODE_RANGE_INDEX) elf_debugger("test-break");
			elRegId xx = elf_emitter_localize(fs,line,v.x);
			elRegId yy = elf_emitter_localize(fs,line,v.y);
			elf_emitter_add_bytexyz(fs,line,elf_node_to_byte(v.k),target_register,xx,yy);
		} break;
		case NODE_CLOSURE: {
			UNUSED_CHECK;
			elRegId head = fn->xmemory;
			elRegId tail = head;
			elRegId last = tail;
			elf_xarray_foreachi(v.z) {
				last = elf_emitter_relocalize(fs,line,NO_SLOT,MAKE_NODE_ID(v.z[i]));
				elASSERT(last == tail ++);
			}
			elf_emitter_add_bytexy(fs,line,BC_CLOSURE,head,v.x);
			if (head != target_register) {
				elf_emitter_add_bytexy(fs,line,BC_RELOAD,target_register,head);
			}
		} break;
		case NODE_TYPEGUARD: {
			UNUSED_CHECK;
			elf_emitter_local_load(fs,line,flags,target_register,y,v.x);
			elf_emitter_add_bytexy(fs,v.line,BC_TYPEGUARD,target_register,elf_nodettotag(v.y));
		} break;
		case NODE_GROUP: {
			UNUSED_CHECK;
			elf_emitter_local_load(fs,line,flags,target_register,y,v.x);
		} break;
		case NODE_REGION: {
			elNOCODE;
		} break;
		case NODE_METAFIELD: {
			UNUSED_CHECK;
			elRegId rx = elf_emitter_localize(fs,line,v.x);
			elRegId ry = elf_emitter_localize(fs,line,v.y);
			elf_emitter_add_bytexyz(fs,line,elf_node_to_byte(v.k),target_register,rx,ry);
		} break;
		case NODE_CALL: {
			elNode vx = fs->nodes[v.x];
			elRegId head = fn->xmemory;
			elRegId tail = head;
			elRegId last = tail;
			if (vx.k == NODE_METAFIELD) {
				elRegId rx = last = elf_emitter_relocalize(fs,line,NO_SLOT,MAKE_NODE_ID(vx.x));
				elASSERT(last == tail ++);
				elRegId ry = last = elf_emitter_relocalize(fs,line,NO_SLOT,MAKE_NODE_ID(vx.y));
				elASSERT(last == tail ++);
				elf_emitter_add_bytexyz(fs,line,elf_node_to_byte(vx.k),ry,rx,ry);
			} else {
				last = elf_emitter_relocalize(fs,line,NO_SLOT,MAKE_NODE_ID(v.x));
				elASSERT(last == tail ++);
			}


			ARRAY_FOR(i, v.z) {
				last = elf_emitter_relocalize(fs,line,NO_SLOT,MAKE_NODE_ID(v.z[i]));
				elASSERT(last == tail ++);
			}
			int n = array_length(v.z);
			elf_emitter_add_bytexyz(fs,line,vx.k == NODE_METAFIELD ? BC_METACALL : BC_CALL,head,n,y);
			if (target_register != NO_SLOT && y != 0) {
				if (y > 1) elf_file_dialog(fs,line,"unsupported");
				elf_emitter_add_bytexy(fs,line,BC_RELOAD,target_register,head);
			}
		} break;
		/* a !! b =
		nil_and = fun(a,b) ? {
			if a == nil ? leave a
			leave b
		} */
		case NODE_NIL_AND: {
			elf_emitter_local_load(fs,line,LOAD_RELOAD,target_register,1,node.x);
			elFileBoolExpr bool_expr = {0};
			elByteId *js = elf_emit_jump_if_nil(fs,NO_LINE,&bool_expr,elf_make_register_node(fs,NO_LINE,target_register));
			elf_emitter_local_load(fs,line,LOAD_RELOAD,target_register,1,node.y);
			elf_emitter_patch_jumplist(fs,js);
			elf_xarray_delete(js);
		} break;
		case NODE_NIL_OR: {
			elf_emitter_local_load(fs,line,LOAD_RELOAD,target_register,1,node.x);
			elFileBoolExpr bool_expr = {0};
			elByteId *js = elf_emit_jump_if_not_nil(fs,NO_LINE,&bool_expr,elf_make_register_node(fs,NO_LINE,target_register));
			elf_emitter_local_load(fs,line,LOAD_RELOAD,target_register,1,node.y);
			elf_emitter_patch_jumplist(fs,js);
			elf_xarray_delete(js);
		} break;
		case NODE_AND: case NODE_OR: {
			if (y == 0) goto leave;

			elf_emitter_local_load(fs,line,LOAD_RELOAD,target_register,1,elf_make_integer_node(fs,line,elFalse));

			elFileBoolExpr bool_expr = {0};
			elByteId *js = elf_emit_jump_if_false(fs,&bool_expr,NO_SLOT,id);

			elf_emitter_local_load(fs,line,LOAD_RELOAD,target_register,1,elf_make_integer_node(fs,line,elTrue));

			elf_emitter_patch_jumplist(fs,js);
			elf_xarray_delete(js);
		} break;
		case NODE_EQ: case NODE_NEQ:
		case NODE_GT: case NODE_GTEQ: case NODE_LT: case NODE_LTEQ:
		/* In the case of relational operators we have some
		we could have sugar */
		if (elf_get_node_kind(fs,node.x) == NODE_RANGE_INDEX) {
			elASSERT(target_register > NO_SLOT);
			/* Set default value to false */
			elNodeId tar = elf_make_register_node(fs,node.line,target_register);
			elf_emitter_emit_store(fs,node.line,tar,elf_make_integer_node(fs,NO_LINE,elFalse));

			/* ensure desugaring actually took place since
			we know node.x is a projection */
			elNodeId xx = elf_emit_desugar_range_expr(fs,node.x,flags);
			if (xx == node.x) {
				elf_throw(fs->R,NO_BYTE,"internal error");
			}

			elASSERT(elf_get_node_kind(fs,xx) == NODE_INDEX);

			elNodeId *z = {0};
			ARRAY_ADD(z,elf_get_node(fs,xx).y);

			xx = elf_make_call_metafield_node(fs,line,elf_get_node(fs,xx).x,z,"idx");

			/* convert node which operates on a projection
			to operate on an actual item */
			/* todo: here we can make some optimizations, we can
			hoist value evaluation (node.y) outside of the loop and
			keep in a register. */
			elNodeId per = elf_make_binary_node(fs,node.line,NODE_EQ,NT_ANY,xx,node.y);
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
			elFileBoolExpr expr = {0};
			/* if true we can exit the loop */
			elByteId *js = elf_emit_jump_if_false(fs,&expr,NO_SLOT,elf_make_binary_node(fs,line,NODE_EQ,NT_BOL,xx,node.y));

			elf_emitter_emit_store(fs,NO_LINE,tar,elf_make_integer_node(fs,NO_LINE,elTrue));
			elf_emit_break(fs,NO_LINE,NO_SLOT);
			/* otherwise continue checking */
			elf_emitter_patch_jumplist(fs,js);
			/* finally, emit any necessary epilogue for
			the desugaring process */
			elf_emit_desugar_range_expr_epilogue(fs,node.x);
			break;
		} else ; /* fallthrough */
			case NODE_DIV: case NODE_MUL: case NODE_MOD:
			case NODE_SUB: case NODE_ADD: case NODE_POW:
			case NODE_BIT_SHL: case NODE_BIT_SHR:
			case NODE_BIT_XOR:
			case NODE_BIT_AND: case NODE_BIT_OR: {
			/* todo: do this properly */
			// if (y == 0) goto leave;
				if ((v.k == NODE_GT) || (v.k == NODE_GTEQ)) {
					elRegId xx = elf_emitter_localize(fs,line,v.y);
					elRegId yy = elf_emitter_localize(fs,line,v.x);
					elf_emitter_add_bytexyz(fs,line,elf_node_to_byte(v.k^1),target_register,xx,yy);
				} else if (v.k == NODE_EQ) {
				/* todo: enable this */
					if(1) goto _else;
					if (elf_get_node_kind(fs,v.y) == NODE_NIL) {
						elRegId xx = elf_emitter_localize(fs,line,v.y);
						elf_emitter_add_bytexy(fs,line,BC_ISNIL,target_register,xx);
					} else goto _else;
				} else { _else:
					elRegId rx = elf_emitter_localize(fs,line,v.x);
					elRegId ry = elf_emitter_localize(fs,line,v.y);
					elf_emitter_add_bytexyz(fs,line,elf_node_to_byte(v.k),target_register,rx,ry);
				}
			} break;
			default: {
				elf_file_dialog(fs,line,"invalid node (%s)",elNodeToStr[node.kind]);
				elNOCODE;
			}
		}

		leave:
		elf_set_memory_state(fs,mem);
		return target_register;
	}


	void elf_emitter_emit_store(elFileState *fs, elFileLine line, elNodeId x, elNodeId y) {
		elNode v = elf_get_targetable_node(fs,MAKE_NODE_ID(x));
		elASSERT(elf_is_targetable_node(v.kind));
		elASSERT(v.level <= fs->level);
		elASSERT(x >= 0);
		elASSERT(y >= 0);
		if (v.line != 0) line = v.line;

	/* keep local state, lastly free any temporary locals */
		elRegId mem = elf_get_memory_state(fs);
		switch (v.k) {
			case NODE_GLOBAL: {
				elRegId yy = elf_emitter_localize(fs,line,y);
				elf_emitter_add_bytexy(fs,line,BC_SETGLOBAL,v.x,yy);
			} break;
			case NODE_CLOSURE_VALUE: {
				elf_file_dialog(fs,line,"assignment to closure value is not supported yet");
			} break;
			case NODE_LOCAL: {
				elf_emitter_local_load(fs,line,LOAD_RELOAD,v.x,1,y);
			} break;
			case NODE_INDEX: case NODE_FIELD: {
				elNodeId table_node = v.x;
				elNodeId field_node = v.y;
				elNodeId value_node = y;
				elRegId table_register = elf_emitter_localize(fs,line,table_node);
				elRegId field_register = elf_emitter_localize(fs,line,field_node);
				elRegId value_register = elf_emitter_localize(fs,line,value_node);
				elByteOP op = v.k == NODE_INDEX ? BC_SETINDEX : BC_SETFIELD;
				elf_emitter_add_bytexyz(fs,line,op,table_register,field_register,value_register);
			} break;
			case NODE_METAFIELD: {
				elf_file_dialog(fs,line,"meta fields are constant");
			} break;
			default: {
				elNOCODE;
			} break;
		}

		elf_set_memory_state(fs,mem);
	}


	void elf_emitter_begin_if(elFileState *fs, elFileLine line, elSelectState *s, elNodeId x, int z) {
		elFileBoolExpr js = {0};
		elf_emit_branch_if(fs,&js,z,NO_SLOT,x);
	// if  0 = jz
	// iff 1 = jnz
		if (z == L_IF) {
			elASSERT(js.f != 0);
			elf_emitter_patch_jumplist(fs,js.t);
			elf_xarray_delete(js.t);
			js.t = 0;
			s->jz = js.f;
		} else {
			elASSERT(js.t != 0);
			elf_emitter_patch_jumplist(fs,js.f);
			elf_xarray_delete(js.f);
			js.f = 0;
			s->jz = js.t;
		}
	}


/*
** Closes previous conditional block by emitting
** escape jump, patches previous jz (jump if false)
** list to enter this block.
*/
	void elf_emitter_add_else_clause(elFileState *fs, elFileLine line, elSelectState *s) {
		if (s->jz == 0) {
			elf_file_dialog(fs,line,"invalid else clause");
		}
		elASSERT(s->jz != 0);
		int j = elf_emit_jump(fs,line,-1);
		ARRAY_ADD(s->j,j);

		elf_emitter_patch_jumplist(fs,s->jz);
		elf_xarray_delete(s->jz);
		s->jz = elNil;
	}


	void elf_emitter_add_elif_clause(elFileState *fs, elFileLine line, elSelectState *s, int x) {
		elf_emitter_add_else_clause(fs,line,s);
		elf_emitter_begin_if(fs,line,s,x,L_IF);
	}


	void elf_emitter_add_then_clause(elFileState *fs, elFileLine line, elSelectState *s) {
	/* we don't need to close the previous block, it can just fall
	through to our branch, do collect all the other exit jumps and
	tie them to this branch block, naturally we don't need to add
	an exit jump since else and elif or closeif will terminate
	this block, multiple then blocks are simply chained together
	naturally. */
		elf_emitter_patch_jumplist(fs,s->j);
		elf_xarray_delete(s->j);
		s->j = elNil;
	}


	void elf_emitter_close_if(elFileState *fs, elFileLine line, elSelectState *s) {
	/* collect missing else branch */
		if (s->jz != elNil) {
			elf_emitter_patch_jumplist(fs,s->jz);
			elf_xarray_delete(s->jz);
			s->jz = elNil;
		}
	/* collect missing then branch */
		if (s->j != elNil) {
			elf_emitter_patch_jumplist(fs,s->j);
			elf_xarray_delete(s->j);
			s->j = elNil;
		}
	}



/* todo: add support for multiple results */
	void elf_emit_yield(elFileState *fs, elFileLine line, elNodeId id) {
		elRegId regress = elf_get_memory_state(fs);
		if (id != NO_NODE) {
		/* todo: determine the number of values in
		tree, and allocate that many registers? */
			int n = 1;
		/* todo: if we only return one value we don't have
		to reload */
			elRegId x = elf_emitter_relocalize(fs,line,NO_SLOT,MAKE_NODE_ID(id));
			if (fs->fn->nyield < n) fs->fn->nyield = n;
			elByteId j = elf_emitter_add_bytexyz(fs,line,BC_YIELD,NO_JUMP,x,n);
			ARRAY_ADD(fs->fn->yj,j);
		/* if there are no results then simply leave directly */
		} else elf_emitter_add_byteop(fs,line,BC_LEAVE,0);
		elf_set_memory_state(fs,regress);

		elf_emitter_add_block_flags(fs,BLOCK_ENDED);
	}


	void elf_emitter_begin_do_while_loop(elFileState *fs, elFileLine line) {
	elFileBlock *bl = elf_emitter_get_block(fs,-1); // fs->fn->block
	elASSERT(bl->flags & BLOCK_LOOP);
	bl->loop.entry = elf_get_last_byteid(fs);
	bl->loop.false_jumps = elNil;
	bl->loop.x = NO_NODE;
	bl->loop.index_register = NO_SLOT;
}


void elf_emitter_close_do_while_loop(elFileState *fs, elFileLine line, elNodeId x) {
	elFileBlock *bl = elf_emitter_get_block(fs,-1); // fs->fn->block;
	elASSERT(bl->flags & BLOCK_LOOP);

	elFileBoolExpr js = {elNil};
	elf_emit_jump_if_true(fs,&js,NO_SLOT,x);

	elf_emitter_patch_jumplist_to(fs,js.t,bl->loop.entry);
	elf_xarray_delete(js.t);
	js.t = elNil;

	elf_emitter_patch_jumplist_to(fs,bl->loop.true_jumps,bl->loop.entry);
	elf_xarray_delete(bl->loop.true_jumps);
	bl->loop.true_jumps = elNil;
}


void elf_emitter_begin_while_loop(elFileState *fs, elFileLine line, elNodeId x) {
	elFileBlock *bl = elf_emitter_get_block(fs,-1);
	elASSERT(bl->flags & BLOCK_LOOP);

	bl->loop.entry = elf_get_last_byteid(fs);
	bl->loop.x = x;

	elASSERT(bl->loop.false_jumps == 0);

	elFileBoolExpr js = {elNil};
	bl->loop.false_jumps = elf_emit_jump_if_false(fs,&js,NO_SLOT,x);
}


void elf_emitter_close_while_loop(elFileState *fs, elFileLine line) {
	elFileBlock *bl = elf_emitter_get_block(fs,-1);
	elASSERT(bl->flags & BLOCK_LOOP);

	elf_emitter_patch_jumplist(fs,bl->loop.true_jumps);
	elf_xarray_delete(bl->loop.true_jumps);
	bl->loop.true_jumps = elNil;
	elf_emit_jump(fs,line,bl->loop.entry);
	elf_emitter_patch_jumplist(fs,bl->loop.false_jumps);
	elf_xarray_delete(bl->loop.false_jumps);
	bl->loop.false_jumps = elNil;
}


void elf_emitter_begin_ranged_loop(elFileState *fs, elFileLine line, elNodeId index_node, elNodeId lo, elNodeId hi) {
	elFileBlock *bl = elf_emitter_get_block(fs,-1);
	elASSERT(bl->flags & BLOCK_LOOP);

	elASSERT(index_node != NO_NODE);

	elRegId index_register = elf_emitter_localize(fs,line,index_node);
	index_node = elf_make_register_node(fs,line,index_register);

	bl->loop.index_register = index_register;
	bl->loop.x = index_node;
	elf_emitter_local_load(fs,line,elTrue,index_register,1,elf_make_type_guard_node(fs,elf_get_node_line(fs,lo),lo,NT_INT));

	bl->loop.entry = elf_get_last_byteid(fs);

	elRegId hi_register = elf_emitter_localize(fs,line,elf_make_type_guard_node(fs,elf_get_node_line(fs,hi),hi,NT_INT));
	hi = elf_make_register_node(fs,line,hi_register);
	elNodeId c = elf_make_node_less_than(fs,line,index_node,hi);

	elASSERT(bl->loop.false_jumps == elNil);

	elFileBoolExpr js = {elNil};
	bl->loop.false_jumps = elf_emit_jump_if_false(fs,&js,NO_SLOT,c);
}


void elf_emitter_close_ranged_loop(elFileState *fs, elFileLine line) {
	elFileBlock *bl = elf_emitter_get_block(fs,-1); // fs->fn->block;
	elASSERT(bl->flags & BLOCK_LOOP);

	elf_emitter_patch_jumplist(fs,bl->loop.true_jumps);
	elf_xarray_delete(bl->loop.true_jumps);
	bl->loop.true_jumps = elNil;
	// elNodeId index_node = bl->loop.index_node;
	elNodeId index_node = elf_make_register_node(fs,NO_LINE,bl->loop.index_register);
	elNodeId k = elf_make_binary_node(fs,NO_LINE,NODE_ADD,NT_INT,index_node,elf_make_integer_node(fs,NO_LINE,1));
	elf_emitter_emit_store(fs,line,index_node,k);
	elf_emit_jump(fs,line,bl->loop.entry);

	elf_emitter_patch_jumplist(fs,bl->loop.false_jumps);
	elf_xarray_delete(bl->loop.false_jumps);
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

