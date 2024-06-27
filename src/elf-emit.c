/*
** See Copyright Notice In elf.h
** elf-emit.c
** Bytecode Generator (node -> bytecode)
*/


elMemoryRegion elf_enter_memory_region(elFileState *fs) {
	elMemoryRegion restore = fs->memory_region;
	if (restore.registry == elNil) restore.registry = fs->memory_region_registry;
	fs->memory_region.registry = restore.registry + restore.length;
	fs->memory_region.length = 0;
	fs->memory_region_level ++;
	return restore;
}


void elf_leave_memory_region(elFileState *fs, elMemoryRegion restore) {
	fs->memory_region = restore;
	fs->memory_region_level --;
}


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
	// 	elf_logdebug("restore memory %i -> %i (%i)",fs->fn->xmemory,memory,dif);
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
	// elf_ensure(id.id != NO_NODE);
	elNode node = elf_get_targetable_node(fs,id);

	/* unaffected by memory regions */
	if ((node.kind == NODE_LOCAL)) {
		return node.x;
	}

	if (fs->memory_region_level > 0) {
		elMemoryRegion mr = fs->memory_region;
		for (int i = 0; i < mr.length; ++ i) {
			if (mr.registry[i].node == id.id) {
				return mr.registry[i].reg;
			}
		}
	}
	return NO_SLOT;
}


elRegId elf_emitter_local_alloc(elFileState *fs, elf_lineid line, elRegId target, elNodeId id) {
	elFileFnState *fn = fs->fn;
	elRegId reg;
#if 0
	if (id != NO_NODE) {
		line = fs->nodes[id].line;
		elf_ensure(!elf_is_target_node(elf_get_node_kind(fs,id)));
		reg = elf_get_node_register(fs,MAKE_NODE_ID(id));
		if (reg != NO_SLOT && elf_get_node_kind(fs,id) != NODE_LOCAL) {
			elf_file_dialog(fs,line,"node already allocated");
		}
	}
#endif

	// if ((reg != NO_SLOT) && (reg == target_register)) {
	//  	return reg;
	// }
	reg = fn->xmemory ++;
	fn->nlocals = MAX(fn->nlocals,fn->xmemory);
	elf_ensure((target == NO_SLOT) || (target == reg));

#if 0
 	if (id != NO_SLOT && fs->memory_region_level > 0) {
		elf_file_dialog(fs,line,"added to memory region %s", elNodeToStr[elf_get_node_kind(fs,id)]);
		elMemoryRegion mr = fs->memory_region;
		mr.registry[mr.length].node = id;
		mr.registry[mr.length].reg = reg;
		fs->memory_region.length ++;
	}
#endif
	return reg;
}


/* we just associate each byte with a line,
this is simple and is pretty great for debugging... */
elByteId elf_emit_byte(elFileState *fs, elf_lineid line, elBytecode byte) {
	elFileFnState *fn = fs->fn;
	elModule *md = fs->M;
	elf_xarray_add(md->lines,line);
	elf_xarray_add(md->bytes,byte);
	elf_xarray_add(md->track,0);
	// elf_bytefpf(stdout,md,-1,md->nbytes-fn->bytes,byte);
	return md->nbytes ++;
}


elByteId elf_emitter_add_byteop(elFileState *fs, elf_lineid line, elByteOP k, elInteger i) {
	return elf_emit_byte(fs,line,(elBytecode){k,i});
}


elByteId elf_emitter_add_bytexy(elFileState *fs, elf_lineid line, elByteOP k, int x, int y) {
	elBytecode b = (elBytecode){k};
	b.x = x, b.y = y;
	return elf_emit_byte(fs,line,b);
}


elByteId elf_emitter_add_bytexyz(elFileState *fs, elf_lineid line, elByteOP k, int x, int y, int z) {
	elBytecode b = (elBytecode){k};
	b.x = x, b.y = y, b.z = z;
	return elf_emit_byte(fs,line,b);
}


void elf_tie_loose_jump_to(elFileState *fs, elByteId to, elByteId j) {
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
		default: elf_unreachable;
	}
}


void elf_tie_loose_jump_list_to(elFileState *fs, elByteId *js, elByteId j) {
	elf_xarray_foreachi(js) {
		elf_tie_loose_jump_to(fs,js[i],j);
	}
}


void elf_emitter_patch_jump(elFileState *fs, elByteId i) {
	elf_tie_loose_jump_to(fs,i,elf_get_last_byteid(fs));
}


void elf_tie_loose_jump_list(elFileState *fs, elByteId *js) {
	elf_xarray_foreachi(js) {
		elf_emitter_patch_jump(fs,js[i]);
	}
}


elByteId elf_emit_jump(elFileState *fs, elf_lineid line, elByteId j) {
	return elf_emitter_add_byteop(fs,line,BC_J,j - fs->M->nbytes);
}


void elf_emitter_add_function_epiloge(elFileState *fs, elf_lineid line) {
	elModule *md = fs->M;
	elFileFnState *fn = fs->fn;

	/* todo: this is unnecessary, we know were
	the return instruction is at. */
	elf_tie_loose_jump_list(fs,fn->yj);
	elf_xarray_delete(fn->yj);
	fn->yj = elNil;

	/* finally, return control flow... */
	elf_emitter_add_byteop(fs,line,BC_LEAVE,0);
}


/*
** Evaluates the given boolean or logical expression using short
** circuit evaluation, creating branches as it evaluates each
** expression, only one register is necessary, if no registers
** are given one is allocated and deallocated automatically.
*/
elByteId elf_emit_branch_if(elFileState *fs, elFileJumplist *js, elBool z, elRegId x, elNodeId id) {
	elByteId j = NO_BYTE;

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
			elf_ensure(x == NO_SLOT);
			x = elf_emitter_localize(fs,NO_LINE,id);

			if (z != 0) {
				j = elf_emitter_add_bytexy(fs,node.line,BC_JNZ,NO_JUMP,x);
				elf_xarray_add(js->t,j);
			} else {
				j = elf_emitter_add_bytexy(fs,node.line,BC_JZ,NO_JUMP,x);
				elf_xarray_add(js->f,j);
			}
		} break;
	}

	elf_set_memory_state(fs,mem);
	return j;
}


elByteId elf_branch_if_false(elFileState *fs, elFileJumplist *js, elRegId x, elNodeId id) {
	return elf_emit_branch_if(fs,js,elFalse,x,id);
}


elByteId elf_branch_if_true(elFileState *fs, elFileJumplist *js, elRegId x, elNodeId id) {
	return elf_emit_branch_if(fs,js,elTrue,x,id);
}


/*
** Similar to branch if true, but additionally this byte
** address becomes the false target, thus all false
** branches converge here. All true branches are returned.
*/
elByteId *elf_emit_jump_if_true(elFileState *fs, elFileJumplist *js, elRegId x, elNodeId id) {
	elf_branch_if_true(fs,js,x,id);
	elf_tie_loose_jump_list(fs,js->f);
	elf_xarray_delete(js->f);
	js->f = elNil;
	return js->t;
}


elByteId *elf_emit_jump_if_false(elFileState *fs, elFileJumplist *js, elRegId x, elNodeId id) {
	elf_branch_if_false(fs,js,x,id);
	elf_tie_loose_jump_list(fs,js->t);
	elf_xarray_delete(js->t);
	js->t = elNil;
	return js->f;
}


elByteId *elf_emit_jump_if_not_nil(elFileState *fs, elf_lineid line, elFileJumplist *js, elNodeId id) {
	return elf_emit_jump_if_false(fs,js,NO_SLOT,elf_make_binary_node(fs,line,NODE_EQ,NT_BOL,id,elf_make_nil_node(fs,line)));
}


/* todo: add support for multiple results */
void elf_emit_yield(elFileState *fs, elf_lineid line, elNodeId id) {
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
		elf_xarray_add(fs->fn->yj,j);
		/* if there are no results then simply leave directly */
	} else elf_emitter_add_byteop(fs,line,BC_LEAVE,0);
	elf_set_memory_state(fs,regress);
}


void elf_emit_initializer(elFileState *fs, elf_lineid line, elRegId target_register, elNodeId id) {
	elRegId mem = elf_get_memory_state(fs);
	elNode v = fs->nodes[id];
	switch (v.k) {
		case NODE_LOAD: {
			elNode x = fs->nodes[v.x];
			if ((x.kind == NODE_LOCAL)) {
				elf_unreachable;
			} else
			if ((x.kind == NODE_FIELD) || (x.k == NODE_INDEX)) {
				// elRegId xx = elf_emitter_localize(fs,line,x.x);
				elRegId xx = target_register;
				elRegId xy = elf_emitter_localize(fs,line,x.y);
				elRegId yy = elf_emitter_localize(fs,line,v.y);
				if (x.k == NODE_FIELD) {
					elf_emitter_add_bytexyz(fs,line,BC_SETFIELD,xx,xy,yy);
				} else elf_emitter_add_bytexyz(fs,line,BC_SETINDEX,xx,xy,yy);
			} else elf_unreachable;
		} break;
		default: elf_unreachable;
	}
	elf_set_memory_state(fs,mem);
}



/*
** Emits code to evaluate the node into any register, if the
** node already has a register no code is emitted.
*/
elRegId elf_emitter_localize(elFileState *fs, elf_lineid line, elNodeId id) {
	elRegId target_register = elf_get_node_register(fs,MAKE_NODE_ID(id));
	if (target_register == NO_SLOT) {
		target_register = elf_emitter_local_alloc(fs,line,NO_SLOT,id);
		elRegId final_register = elf_emitter_local_load(fs,line,0,target_register,1,id);
		elf_ensure(final_register == target_register);
	}
	return target_register;
}


/*
** Reloads the expression a to newly allocated register or
** the given register.
*/
elRegId elf_emitter_relocalize(elFileState *fs, elf_lineid line, elRegId target_register, elNodeIdTypeGuard id) {
	elRegId source_register = elf_get_node_register(fs,id);
	if (target_register == NO_SLOT) {
		target_register = elf_emitter_local_alloc(fs,line,NO_SLOT,id.id);
	}
	/* whenever you call this function is becuase you want to
	move some node but also, it's always to a new register */
	elf_ensure(target_register != source_register);
	// if (target_register != source_register) {
	elRegId final_register = elf_emitter_local_load(fs,line,LOAD_RELOAD,target_register,1,id.id);
	elf_ensure(final_register == target_register);
	// }
	return target_register;
}


void elf_emitter_unload(elFileState *fs, elNodeId id) {
	elNode node = elf_get_node(fs,id);
	elf_lineid line = node.line;
	switch (node.kind) {
		case NODE_GROUP:
		case NODE_FIELD: case NODE_INDEX: {
			elf_emitter_unload(fs,node.x);
		} break;
		case NODE_RANGE_INDEX: {
			elf_emitter_close_ranged_loop(fs,line);
			elf_emitter_leave_block(fs);
			elf_emitter_unload(fs,node.x);
		} break;
		default: ; // elf_ensure(elf_is_targetable_node(node.kind));
	}
}


/*
** emits bytecode to evaluate given node into
** the target register.
*/
elRegId elf_emitter_local_load(elFileState *fs, elf_lineid line
, 	elBool flags, elRegId target_register
, 	elRegId y, elNodeId id) {

	elModule *md = fs->M;
	elFileFnState *fn = fs->fn;

	const elNode v = fs->nodes[id];
	const elNode target_node = v;

	elf_ensure(v.level <= fs->level);

	if (v.line != 0) line = v.line;

	// elf_ensure(target_register > NO_SLOT);
	elf_ensure(target_register < fs->fn->xmemory);

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
			// elf_logdebug("node is already localized and no reloading is necessary, node: %i, target_register: %i, already_register: %i, (reload: %s)"
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

	switch (v.k) {
		/* todo: remove this? */
		case NODE_LOCAL: {
			UNUSED_CHECK;
			elf_emitter_add_bytexy(fs,line,BC_RELOAD,target_register,v.x);
		} break;
		case NODE_FILE_VALUE: {
			UNUSED_CHECK;
			// elf_emitter_add_bytexy(fs,line,BC_RELOAD,target_register,v.x);
		} break;
		case NODE_CLOSURE_VALUE: {
			UNUSED_CHECK;
			elf_emitter_add_bytexy(fs,line,BC_LOAD_CLOSURE_VALUE,target_register,v.x);
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
			fs->M->kn[yy] = v.lit.n;
			elf_emitter_add_bytexy(fs,line,BC_LOADNUM,target_register,yy);
		} break;
		case NODE_STRING: {
			UNUSED_CHECK;
			/* -- todo: allocate this in constant pool */
			int g = elf_add_global_value(fs->M,0,elf_valstr(elf_newstr(fs->rt,v.lit.s)));
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
			elf_emitter_unload(fs,v.x);
			elf_emitter_unload(fs,v.y);
		} break;
		//
		// expressions on the left hand side of an
		// assignment statement, of the form:
		//
		// {x}[{x}..{x}]
		//
		// e.g: A[B..C][D..E][F..G]
		//
		// take the following semantic meaning:
		//
		// for __0 over B..C ? {
		// 	for __1 over D..E ? {
		//			for __2 over F..G ? {
		//				A[__0][__1][__2] = H
		//			}
		//		}
		// }
		//
		// this case handles converting range
		// expressions B..C and D..E into
		// registers, call unload with the
		// node id to emit epiloge.
		//
		case NODE_RANGE_INDEX: {
			if (elf_get_node_kind(fs,target_node.x) == NODE_RANGE_INDEX) elf_debugger("test-break");
			if (elf_get_node_kind(fs,target_node.y) == NODE_RANGE_INDEX) elf_debugger("test-break");
			elf_ensure(target_register != NO_SLOT);
			flags |= LOAD_KEEPALIVE;
			elf_emitter_enter_block(fs,BLOCK_LOOP);
			elRegId xx = elf_emitter_localize(fs,line,target_node.x);
			elRegId yy = elf_emitter_local_alloc(fs,line,NO_SLOT,NO_NODE);
			elf_emitter_begin_ranged_loop(fs,line
			,	elf_make_node_local_register(fs,line,yy)
			,	elf_get_node(fs,target_node.y).x
			, 	elf_get_node(fs,target_node.y).y);
			elf_emitter_add_bytexyz(fs,line,BC_INDEX,target_register,xx,yy);
		} break;
		case NODE_CLOSURE: {
			UNUSED_CHECK;
			elRegId head = fn->xmemory;
			elRegId tail = head;
			elRegId last = tail;
			elf_xarray_foreachi(v.z) {
				last = elf_emitter_relocalize(fs,line,NO_SLOT,MAKE_NODE_ID(v.z[i]));
				elf_ensure(last == tail ++);
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
			elMemoryRegion restore = elf_enter_memory_region(fs);
			elf_xarray_foreachi(v.z) {
				elf_emitter_localize(fs,line,v.z[i]);
			}
			elf_ensure((y == 0) || (elf_get_node_kind(fs,v.x) != NODE_NONE));
			elf_emitter_local_load(fs,line,flags,target_register,1,v.x);
			elf_leave_memory_region(fs,restore);
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
				elf_ensure(last == tail ++);
				elRegId ry = last = elf_emitter_relocalize(fs,line,NO_SLOT,MAKE_NODE_ID(vx.y));
				elf_ensure(last == tail ++);
				elf_emitter_add_bytexyz(fs,line,elf_node_to_byte(vx.k),ry,rx,ry);
			} else {
				last = elf_emitter_relocalize(fs,line,NO_SLOT,MAKE_NODE_ID(v.x));
				elf_ensure(last == tail ++);
			}
			elf_xarray_foreachi(v.z) {
				last = elf_emitter_relocalize(fs,line,NO_SLOT,MAKE_NODE_ID(v.z[i]));
				elf_ensure(last == tail ++);
			}
			int n = elf_xarray_length(v.z);
			elf_emitter_add_bytexyz(fs,line,vx.k == NODE_METAFIELD ? BC_METACALL : BC_CALL,head,n,y);
			if (target_register != NO_SLOT && y != 0) {
				if (y > 1) elf_file_dialog(fs,line,"unsupported");
				elf_emitter_add_bytexy(fs,line,BC_RELOAD,target_register,head);
			}
		} break;
		case NODE_AND: case NODE_OR: {
			if (y == 0) goto leave;
			/* todo: we need to optimize this, better support for
			boolean expressions */
			elFileJumplist js = {0};
			elf_emit_jump_if_false(fs,&js,NO_SLOT,id);
			elf_emitter_local_load(fs,line,LOAD_RELOAD,target_register,1,elf_make_node_integer(fs,line,elTrue));
			int j = elf_emit_jump(fs,line,-1);
			elf_tie_loose_jump_list(fs,js.f);
			elf_xarray_delete(js.f);
			js.f = elNil;
			elf_emitter_local_load(fs,line,LOAD_RELOAD,target_register,1,elf_make_node_integer(fs,line,false));
			elf_emitter_patch_jump(fs,j);
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
				elRegId xx = elf_emitter_localize(fs,line,v.y);
				elRegId yy = elf_emitter_localize(fs,line,v.x);
				elf_emitter_add_bytexyz(fs,line,elf_node_to_byte(v.k^1),target_register,xx,yy);
			} else
			if (v.k == NODE_EQ) {
				/* todo: enable this */
				if(1) goto _else;
				if (elf_get_node_kind(fs,v.y) == NODE_NIL) {
					elRegId xx = elf_emitter_localize(fs,line,v.y);
					elf_emitter_add_bytexy(fs,line,BC_ISNIL,target_register,xx);
				} else goto _else;
			} else { _else:
				elRegId rx = elf_emitter_localize(fs,line,v.x);
				elRegId ry = elf_emitter_localize(fs,line,v.y);
				// elf_file_dialog(fs,line,"%s(r%i, r%i)",elNodeToStr[v.kind],rx,ry);
				// elf_ensure(rx != NO_SLOT);
				// elf_ensure(ry != NO_SLOT);
				elf_emitter_add_bytexyz(fs,line,elf_node_to_byte(v.k),target_register,rx,ry);
			}
		} break;
		default: {
			elf_file_dialog(fs,line,"invalid node (%s)",elNodeToStr[target_node.kind]);
			elf_unreachable;
		}
	}

	leave:
	if (~flags & LOAD_KEEPALIVE) {
		elf_set_memory_state(fs,mem);
	}
	return target_register;
}


void elf_emit_load_to_target(elFileState *fs, elf_lineid line, elNodeId x, elNodeId y) {
	elNode v = elf_get_targetable_node(fs,MAKE_NODE_ID(x));
	elf_ensure(elf_is_targetable_node(v.kind));
	elf_ensure(v.level <= fs->level);
	elf_ensure(x >= 0);
	elf_ensure(y >= 0);
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
			elf_emitter_unload(fs,table_node);
		} break;
		case NODE_METAFIELD: {
			elf_file_dialog(fs,line,"meta fields are constant");
#if 0
			elRegId xx = elf_emitter_localize(fs,line,v.x);
			elRegId ii = elf_emitter_localize(fs,line,v.y);
			elRegId yy = elf_emitter_localize(fs,line,y);
			elf_emitter_add_bytexyz(fs,line,BC_SETMETAFIELD,xx,ii,yy);
#endif
		} break;

		case NODE_RANGE_INDEX: {
			elRegId yy = elf_emitter_localize(fs,line,y);
			elRegId ii = elf_emitter_local_alloc(fs,line,NO_SLOT,NO_NODE);
			elRegId xx = elf_emitter_localize(fs,line,v.x);
			elf_emitter_enter_block(fs,BLOCK_LOOP);
			elf_emitter_begin_ranged_loop(fs,line
			,	elf_make_node_local_register(fs,line,ii)
			,	elf_get_node(fs,v.y).x
			,	elf_get_node(fs,v.y).y);
			elf_emitter_add_bytexyz(fs,line,BC_SETINDEX,xx,ii,yy);
			elf_emitter_close_ranged_loop(fs,line);
			elf_emitter_leave_block(fs);
			elf_emitter_unload(fs,v.x);
		} break;
		default: {
			elf_unreachable;
		} break;
	}

	elf_set_memory_state(fs,mem);
}


void elf_emitter_begin_if(elFileState *fs, elf_lineid line, elSelectState *s, elNodeId x, int z) {
	elFileJumplist js = {0};
	elf_emit_branch_if(fs,&js,z,NO_SLOT,x);
	// if  0 = jz
	// iff 1 = jnz
	if (z == L_IF) {
		elf_ensure(js.f != 0);
		elf_tie_loose_jump_list(fs,js.t);
		elf_xarray_delete(js.t);
		js.t = 0;
		s->jz = js.f;
	} else {
		elf_ensure(js.t != 0);
		elf_tie_loose_jump_list(fs,js.f);
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
void elf_emitter_add_else_clause(elFileState *fs, elf_lineid line, elSelectState *s) {
	elf_ensure(s->jz != 0);
	int j = elf_emit_jump(fs,line,-1);
	elf_xarray_add(s->j,j);

	elf_tie_loose_jump_list(fs,s->jz);
	elf_xarray_delete(s->jz);
	s->jz = elNil;
}


void elf_emitter_add_elif_clause(elFileState *fs, elf_lineid line, elSelectState *s, int x) {
	elf_emitter_add_else_clause(fs,line,s);
	elf_emitter_begin_if(fs,line,s,x,L_IF);
}


void elf_emitter_add_then_clause(elFileState *fs, elf_lineid line, elSelectState *s) {
	/* we don't need to close the previous block, it can just fall
	through to our branch, do collect all the other exit jumps and
	tie them to this branch block, naturally we don't need to add
	an exit jump since else and elif or closeif will terminate
	this block, multiple then blocks are simply chained together
	naturally. */
	elf_tie_loose_jump_list(fs,s->j);
	elf_xarray_delete(s->j);
	s->j = elNil;
}


void elf_emitter_add_if_clause(elFileState *fs, elf_lineid line, elSelectState *s) {
	/* collect missing else branch */
	if (s->jz != elNil) {
		elf_tie_loose_jump_list(fs,s->jz);
		elf_xarray_delete(s->jz);
		s->jz = elNil;
	}
	/* collect missing then branch */
	if (s->j != elNil) {
		elf_tie_loose_jump_list(fs,s->j);
		elf_xarray_delete(s->j);
		s->j = elNil;
	}
}


void elf_emitter_enter_function(elFileState *fs, elFileFnState *fn, char *line) {
	fn->enclosing = fs->fn;
	fn->entities = fs->nentities;
	fn->bytes = fs->md->nbytes;
	fn->line = line;
	fn->yj = elNil;
	fs->fn = fn;
	/* todo: this other function requires fn to be set
	for xmemory, can xmemory simply be in the file state
	instead? */
	fn->entry_block = elf_emitter_enter_block(fs,0);//&fn->entry,
}


void elf_emitter_leave_function(elFileState *fs) {
	elf_emitter_add_function_epiloge(fs,fs->last_token.line);
	elf_emitter_leave_block(fs);
	elf_ensure(fs->fn->entry_block == fs->level);
	/* ensure all locals were deallocated
	properly */
	elf_ensure(fs->nentities == fs->fn->entities);
	fs->fn = fs->fn->enclosing;
}


elFileBlock *elf_emitter_get_block(elFileState *fs, elBlockId id) {
	return &fs->blocks[id > -1 ? id : fs->nblocks + id];
}


/* todo: the block ended flag could be added automatically when
we add a terminating byte to the current block */
void elf_emitter_add_block_flags(elFileState *fs, int flags) {
	elf_emitter_get_block(fs,-1)->flags |= flags;
}


elBlockId elf_emitter_enter_block(elFileState *fs, elBool flags) {
	elBlockId id = fs->nblocks ++;
	if (elf_xarray_length(fs->blocks) < fs->nblocks) {
		elf_xarray_growby(fs->blocks,1);
	}
	elFileBlock *bl = &fs->blocks[id];
	elf_clear_memory(bl,sizeof(*bl));
	// bl->enclosing = fs->fn->block;
	// fs->fn->block = bl;

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
	return id;
}


void elf_emitter_leave_block(elFileState *fs) {
	elf_ensure(fs->nentities >= fs->fn->entities);
	elFileBlock *bl = elf_emitter_get_block(fs,-1);
	fs->nentities = bl->xentity;
	fs->nnodes = bl->xnode;
	fs->level -= 1;
	// elf_ensure(bl->level == fs->level);
	// fs->fn->block = bl->enclosing;
	fs->fn->xmemory = bl->xmemory;
	if (bl->leavejumps != 0) {
		elf_tie_loose_jump_list(fs,bl->leavejumps);
		elf_xarray_delete(bl->leavejumps);
		bl->leavejumps = 0;
	}
	fs->fn->nloops -= (bl->flags & BLOCK_LOOP) != 0;
}


elFileBlock *elf_emitter_get_loop_block(elFileState *fs) {
	int level;
	for (level = fs->level-1; level > -1; -- level) {
		if (fs->blocks[level].flags & BLOCK_LOOP) {
			return elf_emitter_get_block(fs,level);
		}
	}
	return 0;
}


/* we could merge with adjacent blocks, but this
would change the order of execution, leave as is? */
void elf_emitter_enter_delayed_block(elFileState *fs, elf_lineid line) {//, elFileBlock *bl
	elByteId jo = elf_emitter_add_byteop(fs,line,BC_DELAY,NO_JUMP);
	elBlockId id = elf_emitter_enter_block(fs,BLOCK_DELAYED);//bl,
	fs->blocks[id].jumpover = jo;
}


void elf_emitter_leave_delayed_block(elFileState *fs, elf_lineid line) {//, elFileBlock *bl
	// elf_file_dialog(fs,line,"closed block, %i",langL_getlocallabel(fs));
	elFileBlock *bl = elf_emitter_get_block(fs,-1); // fn = fs->fn;

	elf_emitter_add_byteop(fs,line,BC_LEAVE,0);
	elf_emitter_add_block_flags(fs,BLOCK_ENDED);
	elf_emitter_leave_block(fs);

	elf_ensure(bl->entry != bl->jumpover);
	elf_emitter_patch_jump(fs,bl->jumpover);
}


void elf_emit_continue(elFileState *fs, elf_lineid line) {
	elf_ensure(fs->fn->nloops > 0);
	elFileBlock *bl = elf_emitter_get_loop_block(fs);
	elf_ensure(bl != 0);
	elf_emitter_add_block_flags(fs,BLOCK_ENDED);

	elByteId j = elf_emit_jump(fs,line,elf_get_last_byteid(fs));
	elf_xarray_add(bl->loop.true_jumps,j);
}


void elf_emit_break(elFileState *fs, elf_lineid line) {
	elf_ensure(fs->fn->nloops > 0);
	elFileBlock *bl = elf_emitter_get_loop_block(fs);
	elf_ensure(bl != 0);

	elf_emitter_add_block_flags(fs,BLOCK_ENDED);
	elByteId j = elf_emit_jump(fs,line,elf_get_last_byteid(fs));
	elf_xarray_add(bl->leavejumps,j);
}


void elf_emitter_begin_do_while_loop(elFileState *fs, elf_lineid line) {
	elFileBlock *bl = elf_emitter_get_block(fs,-1); // fs->fn->block
	elf_ensure(bl->flags & BLOCK_LOOP);
	bl->loop.entry = elf_get_last_byteid(fs);
	bl->loop.false_jumps = elNil;
	bl->loop.x = NO_NODE;
	bl->loop.index_register = NO_SLOT;
}


void elf_emitter_close_do_while_loop(elFileState *fs, elf_lineid line, elNodeId x) {
	elFileBlock *bl = elf_emitter_get_block(fs,-1); // fs->fn->block;
	elf_ensure(bl->flags & BLOCK_LOOP);

	elFileJumplist js = {elNil};
	elf_emit_jump_if_true(fs,&js,NO_SLOT,x);

	elf_tie_loose_jump_list_to(fs,js.t,bl->loop.entry);
	elf_xarray_delete(js.t);
	js.t = elNil;

	elf_tie_loose_jump_list_to(fs,bl->loop.true_jumps,bl->loop.entry);
	elf_xarray_delete(bl->loop.true_jumps);
	bl->loop.true_jumps = elNil;
}


void elf_emitter_begin_while_loop(elFileState *fs, elf_lineid line, elNodeId x) {
	elFileBlock *bl = elf_emitter_get_block(fs,-1);
	elf_ensure(bl->flags & BLOCK_LOOP);

	bl->loop.entry = elf_get_last_byteid(fs);
	bl->loop.x = x;

	elf_ensure(bl->loop.false_jumps == 0);

	elFileJumplist js = {elNil};
	bl->loop.false_jumps = elf_emit_jump_if_false(fs,&js,NO_SLOT,x);
}


void elf_emitter_close_while_loop(elFileState *fs, elf_lineid line) {
	elFileBlock *bl = elf_emitter_get_block(fs,-1);
	elf_ensure(bl->flags & BLOCK_LOOP);

	elf_tie_loose_jump_list(fs,bl->loop.true_jumps);
	elf_xarray_delete(bl->loop.true_jumps);
	bl->loop.true_jumps = elNil;
	elf_emit_jump(fs,line,bl->loop.entry);
	elf_tie_loose_jump_list(fs,bl->loop.false_jumps);
	elf_xarray_delete(bl->loop.false_jumps);
	bl->loop.false_jumps = elNil;
}


void elf_emitter_begin_ranged_loop(elFileState *fs, elf_lineid line, elNodeId index_node, elNodeId lo, elNodeId hi) {
	elFileBlock *bl = elf_emitter_get_block(fs,-1);
	elf_ensure(bl->flags & BLOCK_LOOP);

	elf_ensure(index_node != NO_NODE);

	elRegId index_register = elf_emitter_localize(fs,line,index_node);
	index_node = elf_make_node_local_register(fs,line,index_register);

	bl->loop.index_register = index_register;
	bl->loop.x = index_node;
	elf_emitter_local_load(fs,line,elTrue,index_register,1,lo);

	bl->loop.entry = elf_get_last_byteid(fs);

	elRegId hi_register = elf_emitter_localize(fs,line,hi);
	hi = elf_make_node_local_register(fs,line,hi_register);
	elNodeId c = elf_make_node_less_than(fs,line,index_node,hi);

	elf_ensure(bl->loop.false_jumps == elNil);

	elFileJumplist js = {elNil};
	bl->loop.false_jumps = elf_emit_jump_if_false(fs,&js,NO_SLOT,c);
}


void elf_emitter_close_ranged_loop(elFileState *fs, elf_lineid line) {
	elFileBlock *bl = elf_emitter_get_block(fs,-1); // fs->fn->block;
	elf_ensure(bl->flags & BLOCK_LOOP);

	elf_tie_loose_jump_list(fs,bl->loop.true_jumps);
	elf_xarray_delete(bl->loop.true_jumps);
	bl->loop.true_jumps = elNil;
	// elf_ensure(bl->loop.index_register == elf_get_node_register(fs,MAKE_NODE_ID(bl->loop.x)));
	elNodeId index_node = bl->loop.index_node;
	elNodeId k = elf_make_binary_node(fs,NO_LINE,NODE_ADD,NT_INT,index_node,elf_make_node_integer(fs,NO_LINE,1));
	elf_emit_load_to_target(fs,line,index_node,k);
	elf_emit_jump(fs,line,bl->loop.entry);
	elf_tie_loose_jump_list(fs,bl->loop.false_jumps);
	elf_xarray_delete(bl->loop.false_jumps);
	bl->loop.false_jumps = 0;
}


elByteOP elf_node_to_byte(elNodeKi tt) {
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

