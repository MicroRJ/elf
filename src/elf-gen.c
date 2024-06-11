/*
** See Copyright Notice In elf.h
** elf-gen.c
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


elf_byteop nodetobyte(elNodeKi tt);


elByteId elf_getlastbyteid(elFileState *fs) {
	return fs->md->nbytes;
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


elBool elf_is_pseudo_node(elNodeKi kind) {
	switch (kind) {
		case NODE_TYPEGUARD:
		case NODE_GROUP: case NODE_REGION: return elTrue;
		default: return elFalse;
	}
}


elRegId elf_get_node_register(elFileState *fs, elNodeIdTypeGuard id) {
	if (id.id == NO_NODE) return NO_SLOT;
	// elf_ensure(id.id != NO_NODE);
	elNode node = elf_get_node(fs,id.id);
	while (elf_is_pseudo_node(node.kind)) {
		node = elf_get_node(fs,node.x);
	}

	/* unaffected by memory regions */
	if (node.kind == NODE_LOCAL) return node.x;

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


elRegId elf_alloc_register(elFileState *fs, elf_lineid line, elRegId target, elNodeId id) {
	elFileFnState *fn = fs->fn;
	elRegId reg;
#if 0
	if (id != NO_NODE) {
		line = fs->nodes[id].line;
		elf_ensure(!elf_is_pseudo_node(elf_get_node_kind(fs,id)));
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
elByteId elf_emit_byte(elFileState *fs, elf_lineid line, elf_Bytecode byte) {
	elFileFnState *fn = fs->fn;
	elModule *md = fs->md;
	elf_varadd(md->lines,line);
	elf_varadd(md->bytes,byte);
	elf_varadd(md->track,0);
	// elf_bytefpf(stdout,md,-1,md->nbytes-fn->bytes,byte);
	return md->nbytes ++;
}


elByteId elf_emit_byteop(elFileState *fs, elf_lineid line, elf_byteop k, elInteger i) {
	return elf_emit_byte(fs,line,(elf_Bytecode){k,i});
}


elByteId elf_emit_bytexy(elFileState *fs, elf_lineid line, elf_byteop k, int x, int y) {
	elf_Bytecode b = (elf_Bytecode){k};
	b.x = x, b.y = y;
	return elf_emit_byte(fs,line,b);
}


elByteId elf_emit_bytexyz(elFileState *fs, elf_lineid line, elf_byteop k, int x, int y, int z) {
	elf_Bytecode b = (elf_Bytecode){k};
	b.x = x, b.y = y, b.z = z;
	return elf_emit_byte(fs,line,b);
}


void elf_tie_loose_jump_to(elFileState *fs, elByteId to, elByteId j) {
	elModule *M = fs->M;
	elf_Bytecode *bytes = M->bytes;
	elf_Bytecode b = bytes[to];

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
	elf_arrfori(js) {
		elf_tie_loose_jump_to(fs,js[i],j);
	}
}


void elf_tie_loose_jump(elFileState *fs, elByteId i) {
	elf_tie_loose_jump_to(fs,i,elf_getlastbyteid(fs));
}


void elf_tie_loose_jump_list(elFileState *fs, elByteId *js) {
	elf_arrfori(js) {
		elf_tie_loose_jump(fs,js[i]);
	}
}


elByteId elf_emit_jump(elFileState *fs, elf_lineid line, elByteId j) {
	return elf_emit_byteop(fs,line,BC_J,j - fs->md->nbytes);
}


void elf_emit_function_epiloge(elFileState *fs, elf_lineid line) {
	elModule *md = fs->md;
	elFileFnState *fn = fs->fn;

	/* todo: this is unnecessary, we know were
	the return instruction is at. */
	elf_tie_loose_jump_list(fs,fn->yj);
	elf_delvar(fn->yj);
	fn->yj = elNil;

	/* finally, return control flow... */
	elf_emit_byteop(fs,line,BC_LEAVE,0);
}


/*
** Evaluates the given boolean or logical expression using short
** circuit evaluation, creating branches as it evaluates each
** expression, only one register is necessary, if no registers
** are given one is allocated and deallocated automatically.
*/
elByteId elf_emit_branch_if(elFileState *fs, elFileJumplist *js, elBool z, elRegId x, elNodeId id) {
	elNode v = fs->nodes[id];
	elByteId j = NO_BYTE;

	elRegId mem = elf_get_memory_state(fs);
	/*
	todo: updated comment, this is b.s!
	------------------------------------
	note: old comment
	if not provided one, allocate temporary
	register here, notice how this is done
	before we keep splitting, this will make
	it so subsequent recursive calls to this
	function won't keep allocating registers,
	we get away with using only one register
	due to the transitive nature of short
	circuiting, expressible in the bytecode. */
	if (x == NO_SLOT) x = elf_alloc_register(fs,NO_LINE,NO_SLOT,id);

	switch (v.k) {
		case NODE_GROUP: {
			j = elf_emit_branch_if(fs,js,z,x,v.x);
		} break;
		case NODE_AND: {
			elf_emit_jump_if_false(fs,js,x,v.x);
			j = elf_emit_branch_if(fs,js,z,x,v.y);
		} break;
		case NODE_OR: {
			elf_emit_jump_if_true(fs,js,x,v.x);
			j = elf_emit_branch_if(fs,js,z,x,v.y);
		} break;
		default: {
			/* todo: hmmm..., this is a bit sketch for
			something like foo(1 && 2), '1 && 2'
			should be reloaded to 'x'. */
			elf_gen_local_load(fs,NO_LINE,false,x,1,id);

			if (z != 0) {
				j = elf_emit_bytexy(fs,v.line,BC_JNZ,NO_JUMP,x);
				elf_varadd(js->t,j);
			} else {
				j = elf_emit_bytexy(fs,v.line,BC_JZ,NO_JUMP,x);
				elf_varadd(js->f,j);
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
** branches converge here. all true branches are returned.
*/
elByteId *elf_emit_jump_if_true(elFileState *fs, elFileJumplist *js, elRegId x, elNodeId id) {
	elf_branch_if_true(fs,js,x,id);
	elf_tie_loose_jump_list(fs,js->f);
	elf_delvar(js->f);
	js->f = elNil;
	return js->t;
}


elByteId *elf_emit_jump_if_false(elFileState *fs, elFileJumplist *js, elRegId x, elNodeId id) {
	elf_branch_if_false(fs,js,x,id);
	elf_tie_loose_jump_list(fs,js->t);
	elf_delvar(js->t);
	js->t = elNil;
	return js->f;
}


elByteId *elf_emit_jump_if_not_nil(elFileState *fs, elf_lineid line, elFileJumplist *js, elNodeId id) {
	return elf_emit_jump_if_false(fs,js,NO_SLOT,elf_make_binary_node(fs,line,NODE_EQ,NT_BOL,id,elf_nodenil(fs,line)));
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
		elRegId x = elf_gen_relocalize(fs,line,NO_SLOT,MAKE_NODE_ID(id));
		if (fs->fn->nyield < n) fs->fn->nyield = n;
		elByteId j = elf_emit_bytexyz(fs,line,BC_YIELD,NO_JUMP,x,n);
		elf_varadd(fs->fn->yj,j);
		/* if there are no results then simply leave directly */
	} else elf_emit_byteop(fs,line,BC_LEAVE,0);
	elf_set_memory_state(fs,regress);
}


void elf_emit(elFileState *fs, elf_lineid line, elRegId target_register, elNodeId id) {
	elRegId mem = elf_get_memory_state(fs);
	elNode v = fs->nodes[id];
	switch (v.k) {
		case NODE_LOAD: {
			elNode x = fs->nodes[v.x];
			if ((x.k == NODE_LOCAL)) {
				elf_unreachable;
			} else
			if ((x.k == NODE_FIELD) || (x.k == NODE_INDEX)) {
				// elRegId xx = elf_gen_localize(fs,line,x.x);
				elRegId xx = target_register;
				elRegId xy = elf_gen_localize(fs,line,x.y);
				elRegId yy = elf_gen_localize(fs,line,v.y);
				if (x.k == NODE_FIELD) {
					elf_emit_bytexyz(fs,line,BC_SETFIELD,xx,xy,yy);
				} else elf_emit_bytexyz(fs,line,BC_SETINDEX,xx,xy,yy);
			} else elf_unreachable;
		} break;
		default: elf_unreachable;
	}
	elf_set_memory_state(fs,mem);
}



/*
** Use this function when you just want
** to make sure the expression is in a
** register.
*/
elRegId elf_gen_localize(elFileState *fs, elf_lineid line, elNodeId id) {
	elRegId target_register = elf_get_node_register(fs,MAKE_NODE_ID(id));
	if (target_register == NO_SLOT) {
		target_register = elf_alloc_register(fs,line,NO_SLOT,id);
		elRegId final_register = elf_gen_local_load(fs,line,0,target_register,1,id);
		elf_ensure(final_register == target_register);
	}
	return target_register;
}


/*
** Reloads the expression a to newly allocated register or
** the given register.
*/
elRegId elf_gen_relocalize(elFileState *fs, elf_lineid line, elRegId target_register, elNodeIdTypeGuard id) {
	elRegId source_register = elf_get_node_register(fs,id);
	if (target_register == NO_SLOT) {
		target_register = elf_alloc_register(fs,line,NO_SLOT,id.id);
	}
	if (target_register != source_register) {
		elRegId final_register = elf_gen_local_load(fs,line,LOAD_RELOAD,target_register,1,id.id);
		elf_ensure(final_register == target_register);
	}
	return target_register;
}



/*
** emits bytecode to load id into local x,
** if y is 0 the instruction is omitted if
** no side effects.
*/
elRegId elf_gen_local_load(elFileState *fs, elf_lineid line
, 	elBool flags, elRegId target_register
, 	elRegId y, elNodeId id) {

	elModule *md = fs->md;
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
			target_register = elf_alloc_register(fs,line,NO_SLOT,id);
		}
	}

	/* memory state is tracked after possible target
	register allocation */
	elRegId mem = elf_get_memory_state(fs);

	elRegId source_register = elf_get_node_register(fs,MAKE_NODE_ID(id));
	/* if node is already local and no reloading is necessary */
	if ((source_register != NO_SLOT) && (source_register != target_register)) {
		if (~flags & LOAD_RELOAD) {
			__debugbreak();
			// elf_logdebug("node is already localized and no reloading is necessary, node: %i, target_register: %i, source_register: %i, (reload: %s)"
			// , id, target_register, source_register, (flags & LOAD_RELOAD) ? "true" : "false");
			target_register = source_register;
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
			elf_emit_bytexy(fs,line,BC_RELOAD,target_register,v.x);
		} break;
		case NODE_THIS: {
			UNUSED_CHECK;
			elf_emit_byteop(fs,line,BC_LOADTHIS,target_register);
		} break;
		case NODE_CLSVAL: {
			UNUSED_CHECK;
			elf_emit_bytexy(fs,line,BC_LOADCACHED,target_register,v.x);
		} break;
		case NODE_GLOBAL: {
			UNUSED_CHECK;
			elf_emit_bytexy(fs,line,BC_LOADGLOBAL,target_register,v.x);
		} break;
		case NODE_NIL: {
			UNUSED_CHECK;
			/* -- todo: coalesce */
			elf_emit_bytexy(fs,line,BC_LOADNIL,target_register,y);
		} break;
		case NODE_INTEGER: {
			UNUSED_CHECK;
			/* todo: interning */
			int yy = elf_varaddi(fs->md->ki,1);
			fs->md->ki[yy] = v.lit.i;
			elf_emit_bytexy(fs,line,BC_LOADINT,target_register,yy);
		} break;
		case NODE_NUMBER: {
			UNUSED_CHECK;
			/* todo: interning */
			int yy = elf_varaddi(fs->md->kn,1);
			fs->md->kn[yy] = v.lit.n;
			elf_emit_bytexy(fs,line,BC_LOADNUM,target_register,yy);
		} break;
		case NODE_STRING: {
			UNUSED_CHECK;
			/* -- todo: allocate this in constant pool */
			int g = lang_addglobal(fs->md,0,elf_valstr(elf_newstr(fs->rt,v.lit.s)));
			elf_emit_bytexy(fs,line,BC_LOADGLOBAL,target_register,g);
		} break;
		case NODE_TABLE: {
			UNUSED_CHECK;
			elf_emit_bytexy(fs,line,BC_TABLE,target_register,0);
			elf_arrfori(v.z) elf_emit(fs,line,target_register,v.z[i]);
		} break;
		case NODE_FIELD: case NODE_INDEX: {
			UNUSED_CHECK;
			elRegId xx = elf_gen_localize(fs,line,v.x);
			elRegId yy = elf_gen_localize(fs,line,v.y);
			elf_emit_bytexyz(fs,line,nodetobyte(v.k),target_register,xx,yy);
		} break;
		case NODE_CLOSURE: {
			UNUSED_CHECK;
			elRegId head = fn->xmemory;
			elRegId tail = head;
			elRegId last = tail;
			elf_arrfori(v.z) {
				last = elf_gen_relocalize(fs,line,NO_SLOT,MAKE_NODE_ID(v.z[i]));
				elf_ensure(last == tail ++);
			}
			elf_emit_bytexy(fs,line,BC_CLOSURE,head,v.x);
			if (head != target_register) {
				elf_emit_bytexy(fs,line,BC_RELOAD,target_register,head);
			}
		} break;
		case NODE_TYPEGUARD: {
			UNUSED_CHECK;
			elf_gen_local_load(fs,line,flags,target_register,y,v.x);
			elf_emit_bytexy(fs,v.line,BC_TYPEGUARD,target_register,elf_nodettotag(v.y));
		} break;
		case NODE_GROUP: {
			UNUSED_CHECK;
			elf_gen_local_load(fs,line,flags,target_register,y,v.x);
		} break;
		case NODE_REGION: {
			elMemoryRegion restore = elf_enter_memory_region(fs);
			elf_arrfori(v.z) {
				elf_gen_localize(fs,line,v.z[i]);
			}
			elf_ensure((y == 0) || (elf_get_node_kind(fs,v.x) != NODE_NONE));
			elf_gen_local_load(fs,line,flags,target_register,1,v.x);
			elf_leave_memory_region(fs,restore);
		} break;
		case NODE_FILE: {
			elf_gen_local_load(fs,line,elTrue,target_register,1,v.x);
			elf_emit_bytexy(fs,line,BC_LOADFILE,target_register,y);
		} break;
		case NODE_METAFIELD: {
			UNUSED_CHECK;
			elRegId rx = elf_gen_localize(fs,line,v.x);
			elRegId ry = elf_gen_localize(fs,line,v.y);
			elf_emit_bytexyz(fs,line,nodetobyte(v.k),target_register,rx,ry);
		} break;
		case NODE_CALL: {
			elNode vx = fs->nodes[v.x];
			elRegId head = fn->xmemory;
			elRegId tail = head;
			elRegId last = tail;
			if (vx.k == NODE_METAFIELD) {
				elRegId rx = last = elf_gen_relocalize(fs,line,NO_SLOT,MAKE_NODE_ID(vx.x));
				// elRegId rx = last = elf_gen_local_load(fs,line,LOAD_RELOAD|LOAD_ALLOCATE,NO_SLOT,1,vx.x);
				elf_ensure(last == tail ++);
				elRegId ry = last = elf_gen_relocalize(fs,line,NO_SLOT,MAKE_NODE_ID(vx.y));
				elf_ensure(last == tail ++);
				elf_emit_bytexyz(fs,line,nodetobyte(vx.k),ry,rx,ry);
			} else {
				last = elf_gen_relocalize(fs,line,NO_SLOT,MAKE_NODE_ID(v.x));
				elf_ensure(last == tail ++);
			}
			elf_arrfori(v.z) {
				last = elf_gen_relocalize(fs,line,NO_SLOT,MAKE_NODE_ID(v.z[i]));
				elf_ensure(last == tail ++);
			}
			int n = elf_varlen(v.z);
			elf_emit_bytexyz(fs,line,vx.k == NODE_METAFIELD ? BC_METACALL : BC_CALL,head,n,y);
			if (target_register != NO_SLOT && y != 0) {
				if (y > 1) elf_file_dialog(fs,line,"unsupported");
				elf_emit_bytexy(fs,line,BC_RELOAD,target_register,head);
			}
		} break;
		case NODE_AND: case NODE_OR: {
			if (y == 0) goto leave;
			/* todo: we need to optimize this, better support for
			boolean expressions */
			elFileJumplist js = {0};
			elf_emit_jump_if_false(fs,&js,NO_SLOT,id);
			elf_gen_local_load(fs,line,LOAD_RELOAD,target_register,1,elf_make_integer_node(fs,line,elTrue));
			int j = elf_emit_jump(fs,line,-1);
			elf_tie_loose_jump_list(fs,js.f);
			elf_delvar(js.f);
			js.f = elNil;
			elf_gen_local_load(fs,line,LOAD_RELOAD,target_register,1,elf_make_integer_node(fs,line,false));
			elf_tie_loose_jump(fs,j);
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
				elRegId xx = elf_gen_localize(fs,line,v.y);
				elRegId yy = elf_gen_localize(fs,line,v.x);
				elf_emit_bytexyz(fs,line,nodetobyte(v.k^1),target_register,xx,yy);
			} else
			if (v.k == NODE_EQ) {
				/* todo: enable this */
				if(1) goto _else;
				if (elf_get_node_kind(fs,v.y) == NODE_NIL) {
					elRegId xx = elf_gen_localize(fs,line,v.y);
					elf_emit_bytexy(fs,line,BC_ISNIL,target_register,xx);
				} else goto _else;
			} else { _else:
				elRegId rx = elf_gen_localize(fs,line,v.x);
				elRegId ry = elf_gen_localize(fs,line,v.y);
				// elf_file_dialog(fs,line,"%s(r%i, r%i)",elNodeToStr[v.kind],rx,ry);
				// elf_ensure(rx != NO_SLOT);
				// elf_ensure(ry != NO_SLOT);
				elf_emit_bytexyz(fs,line,nodetobyte(v.k),target_register,rx,ry);
			}
		} break;
		default: elf_unreachable;
	}

	leave:
	// if (~flags & LOAD_KEEPALIVE) {
		elf_set_memory_state(fs,mem);
	// }
	return target_register;
}


void elf_gen_store(elFileState *fs, elf_lineid line, elNodeId x, elNodeId y) {
	elNode v = fs->nodes[x];
	elf_ensure(v.level <= fs->level);
	elf_ensure(x >= 0);
	elf_ensure(y >= 0);
	if (v.line != 0) line = v.line;

	/* keep local state, finally free any temporary locals */
	elRegId mem = fs->fn->xmemory;
	switch (v.k) {
		case NODE_GLOBAL: {
			elRegId yy = elf_gen_localize(fs,line,y);
			elf_emit_bytexy(fs,line,BC_SETGLOBAL,v.x,yy);
		} break;
		case NODE_CLSVAL: {
			elf_file_dialog(fs,line,"assignment to cache value is not supported yet");
		} break;
		case NODE_LOCAL: {
			elf_gen_local_load(fs,line,LOAD_RELOAD,v.x,1,y);
		} break;
		case NODE_INDEX: case NODE_FIELD: {
			elRegId xx = elf_gen_localize(fs,line,v.x);
			elRegId ii = elf_gen_localize(fs,line,v.y);
			elRegId yy = elf_gen_localize(fs,line,y);
			elf_byteop op = v.k == NODE_INDEX ? BC_SETINDEX : BC_SETFIELD;
			elf_emit_bytexyz(fs,line,op,xx,ii,yy);
		} break;
		case NODE_METAFIELD: {
			elRegId xx = elf_gen_localize(fs,line,v.x);
			elRegId ii = elf_gen_localize(fs,line,v.y);
			elRegId yy = elf_gen_localize(fs,line,y);
			elf_emit_bytexyz(fs,line,BC_SETMETAFIELD,xx,ii,yy);
		} break;
		// {x}[{x}..{x}] = {y}
		case NODE_RANGE_INDEX: {
			elNodeId lo = fs->nodes[v.y].x;
			elNodeId hi = fs->nodes[v.y].y;

			elNodeId ii = elf_make_local_value_node(fs,line,elf_alloc_register(fs,line,NO_SLOT,NO_NODE));

			elRegId xx = elf_gen_localize(fs,line,v.x);
			elRegId yy = elf_gen_localize(fs,line,y);
			elFileBlockState block = {0};
			elf_enter_file_block(fs,&block,BLOCK_LOOP);
			elf_begin_ranged_loop(fs,line,ii,lo,hi);
			elf_emit_bytexyz(fs,line,BC_SETINDEX,xx,block.loop.r,yy);
			elf_close_ranged_loop(fs,line);
			elf_leave_file_block(fs);
		} break;
		default: {
			elf_unreachable;
		} break;
	}


	fs->fn->xmemory = mem;
}


void elf_gen_begin_if(elFileState *fs, elf_lineid line, elSelectState *s, elNodeId x, int z) {
	elFileJumplist js = {0};
	elf_emit_branch_if(fs,&js,z,NO_SLOT,x);
	// if  0 = jz
	// iff 1 = jnz
	if (z == L_IF) {
		elf_ensure(js.f != 0);
		elf_tie_loose_jump_list(fs,js.t);
		elf_delvar(js.t);
		js.t = 0;
		s->jz = js.f;
	} else {
		elf_ensure(js.t != 0);
		elf_tie_loose_jump_list(fs,js.f);
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
void elf_gen_add_else(elFileState *fs, elf_lineid line, elSelectState *s) {
	elf_ensure(s->jz != 0);
	int j = elf_emit_jump(fs,line,-1);
	elf_varadd(s->j,j);

	elf_tie_loose_jump_list(fs,s->jz);
	elf_delvar(s->jz);
	s->jz = elNil;
}


void elf_gen_add_elif(elFileState *fs, elf_lineid line, elSelectState *s, int x) {
	elf_gen_add_else(fs,line,s);
	elf_gen_begin_if(fs,line,s,x,L_IF);
}


void elf_gen_add_then(elFileState *fs, elf_lineid line, elSelectState *s) {
	/* we don't need to close the previous block, it can just fall
	through to our branch, do collect all the other exit jumps and
	tie them to this branch block, naturally we don't need to add
	an exit jump since else and elif or closeif will terminate
	this block, multiple then blocks are simply chained together
	naturally. */
	elf_tie_loose_jump_list(fs,s->j);
	elf_delvar(s->j);
	s->j = elNil;
}


void elf_close_if(elFileState *fs, elf_lineid line, elSelectState *s) {
	/* collect missing else branch */
	if (s->jz != elNil) {
		elf_tie_loose_jump_list(fs,s->jz);
		elf_delvar(s->jz);
		s->jz = elNil;
	}
	/* collect missing then branch */
	if (s->j != elNil) {
		elf_tie_loose_jump_list(fs,s->j);
		elf_delvar(s->j);
		s->j = elNil;
	}
}


elFileBlockState *elf_getloopblock(elFileState *fs) {
	elFileBlockState *bl = fs->fn->block;
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
	elFileBlockState *bl = elf_getloopblock(fs);
	elf_thisblockended(fs);

	elByteId j = elf_emit_jump(fs,line,elf_getlastbyteid(fs));
	elf_varadd(bl->loop.truejumps,j);
}


void elf_emitbreak(elFileState *fs, elf_lineid line) {
	elf_ensure(fs->fn->nloops > 0);
	elFileBlockState *bl = elf_getloopblock(fs);
	elf_thisblockended(fs);
	elByteId j = elf_emit_jump(fs,line,elf_getlastbyteid(fs));
	elf_varadd(bl->leavejumps,j);
}


void elf_begin_do_while_loop(elFileState *fs, elf_lineid line) {
	elFileBlockState *bl = fs->fn->block;
	elf_ensure(bl->flags & BLOCK_LOOP);
	bl->loop.entry = elf_getlastbyteid(fs);
	bl->loop.falsejumps = elNil;
	bl->loop.x = NO_NODE;
	bl->loop.r = NO_SLOT;
}


void elf_close_do_while_loop(elFileState *fs, elf_lineid line, elNodeId x) {
	elFileBlockState *bl = fs->fn->block;
	elf_ensure(bl->flags & BLOCK_LOOP);

	elFileJumplist js = {elNil};
	elf_emit_jump_if_true(fs,&js,NO_SLOT,x);

	elf_tie_loose_jump_list_to(fs,js.t,bl->loop.entry);
	elf_delvar(js.t);
	js.t = elNil;

	elf_tie_loose_jump_list_to(fs,bl->loop.truejumps,bl->loop.entry);
	elf_delvar(bl->loop.truejumps);
	bl->loop.truejumps = elNil;
}


void elf_begin_while_loop(elFileState *fs, elf_lineid line, elNodeId x) {
	elFileBlockState *bl = fs->fn->block;
	elf_ensure(bl->flags & BLOCK_LOOP);

	bl->loop.entry = elf_getlastbyteid(fs);
	bl->loop.x = x;

	elf_ensure(bl->loop.falsejumps == 0);

	elFileJumplist js = {elNil};
	bl->loop.falsejumps = elf_emit_jump_if_false(fs,&js,NO_SLOT,x);
}


void elf_close_while_loop(elFileState *fs, elf_lineid line) {
	elFileBlockState *bl = fs->fn->block;
	elf_ensure(bl->flags & BLOCK_LOOP);

	elf_tie_loose_jump_list(fs,bl->loop.truejumps);
	elf_delvar(bl->loop.truejumps);
	bl->loop.truejumps = elNil;
	elf_emit_jump(fs,line,bl->loop.entry);
	elf_tie_loose_jump_list(fs,bl->loop.falsejumps);
	elf_delvar(bl->loop.falsejumps);
	bl->loop.falsejumps = elNil;
}


elNodeId elf_nodeilessthan(elFileState *fs, elf_lineid line, elNodeId x, elNodeId y) {
	x = elf_nodetypeguard(fs,fs->nodes[x].line,x,NT_INT);
	y = elf_nodetypeguard(fs,fs->nodes[y].line,y,NT_INT);
	return elf_make_binary_node(fs,line,NODE_LT,NT_BOL,x,y);
}


void elf_begin_ranged_loop(elFileState *fs, elf_lineid line, elNodeId x, elNodeId lo, elNodeId hi) {
	elFileBlockState *bl = fs->fn->block;
	elf_ensure(bl->flags & BLOCK_LOOP);

	elf_ensure(x != NO_NODE);
	bl->loop.x = x;
	bl->loop.r = elf_gen_localize(fs,line,x);
	elf_gen_local_load(fs,line,elTrue,bl->loop.r,1,lo);

	bl->loop.entry = elf_getlastbyteid(fs);

	elf_gen_localize(fs,line,hi);
	elNodeId c = elf_nodeilessthan(fs,line,bl->loop.x,hi);

	elf_ensure(bl->loop.falsejumps == 0);

	elFileJumplist js = {elNil};
	bl->loop.falsejumps = elf_emit_jump_if_false(fs,&js,NO_SLOT,c);
}


void elf_close_ranged_loop(elFileState *fs, elf_lineid line) {
	elFileBlockState *bl = fs->fn->block;
	elf_ensure(bl->flags & BLOCK_LOOP);

	elf_tie_loose_jump_list(fs,bl->loop.truejumps);
	elf_delvar(bl->loop.truejumps);
	bl->loop.truejumps = elNil;
	elf_ensure(bl->loop.r == elf_get_node_register(fs,MAKE_NODE_ID(bl->loop.x)));
	int x = bl->loop.x;
	int k = elf_make_binary_node(fs,NO_LINE,NODE_ADD,NT_INT,x,elf_make_integer_node(fs,NO_LINE,1));
	elf_gen_store(fs,line,x,k);
	elf_emit_jump(fs,line,bl->loop.entry);
	elf_tie_loose_jump_list(fs,bl->loop.falsejumps);
	elf_delvar(bl->loop.falsejumps);
	bl->loop.falsejumps = 0;
}


/* -- todo? we could merge with adjacent blocks, but this
would change the order of execution, should leave as is? */
void elf_enter_delayed_block(elFileState *fs, elf_lineid line, elFileBlockState *bl) {
	elByteId jo = elf_emit_byteop(fs,line,BC_DELAY,NO_JUMP);
	elf_enter_file_block(fs,bl,BLOCK_DELAYED);
	bl->jumpover = jo;
}


void elf_leave_delayed_block(elFileState *fs, elf_lineid line, elFileBlockState *bl) {
	// elf_file_dialog(fs,line,"closed block, %i",langL_getlocallabel(fs));
	elFileFnState *fn = fs->fn;
	elf_thisblockended(fs);
	elf_emit_byteop(fs,line,BC_LEAVE,0);
	elf_leave_file_block(fs);
	elf_ensure(bl->entry != bl->jumpover);
	elf_tie_loose_jump(fs,bl->jumpover);
}


elf_byteop nodetobyte(elNodeKi tt) {
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

