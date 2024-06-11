/*
** See Copyright Notice In elf.h
** elf-file.c
** Parsing Stuff
*/


elBool elf_fscheckexpr(elFileState *fs, elf_lineid line, elNodeId id) {
	if (id != NO_NODE) return false;
	elf_file_dialog(fs,line,"invalid expression");
	return elTrue;
}


elBool elf_linetesttk(elFileState *fs, ltokentype k) {
	return fs->tk.type == k && fs->lasttk.eol != elTrue;
}


elBool elf_test_token(elFileState *fs, ltokentype k) {
	return fs->tk.type == k;
}

elBool elf_test_then_token(elFileState *fs, ltokentype k) {
	return fs->thentk.type == k;
}


/*
** Returns elTrue whether there are no more tokens
** or whehter the current token is a match.
*/
elBool elf_term_token(elFileState *fs, ltokentype k) {
	return fs->tk.type == TK_NONE || fs->tk.type == k;
}


elBool elf_term_eol_token(elFileState *fs) {
	return fs->tk.type == TK_NONE || fs->lasttk.eol == elTrue;
}


/*
** Consumes the current token only if it is a match,
** returning whether it was a match or not.
*/
elBool elf_pick_token(elFileState *fs, ltokentype k) {
	return elf_test_token(fs,k) && (elf_lexone(fs), elTrue);
}


elBool elf_pick_token_inline(elFileState *fs, ltokentype k) {
	return elf_linetesttk(fs,k) && (elf_lexone(fs), elTrue);
}


/*
** Same as pick, only this time there are two possibilities.
*/
elBool elf_choosetk(elFileState *fs, ltokentype x, ltokentype y) {
	return (elf_test_token(fs,x) || elf_test_token(fs,y)) && (elf_lexone(fs), elTrue);
}


/*
** Check whether the current token is a match,
** if so pick it, otherwise error.
*/
elToken elf_take_token(elFileState *fs, int k) {
	elToken tk = fs->tk;
	if (!elf_pick_token(fs,k)) {
		elf_file_dialog(fs,fs->tk.line,"expected '%s'\n",elf_tkintel[k].name);
	}
	return tk;
}


elToken elf_take_token_inline(elFileState *fs, int k) {
	elToken tk = fs->tk;
	if (!elf_pick_token_inline(fs,k)) {
		elf_file_dialog(fs,fs->tk.line,"expected '%s'\n",elf_tkintel[k].name);
	}
	return tk;
}


elRegId elf_fsnumentinlev(elFileState *fs) {
	elFileEntity *entities = fs->entities;
	int nentities = fs->nentities;
	int n;
	for (n = 0; n < nentities; ++ n) {
		if (entities[nentities-1-n].level < fs->level) {
			break;
		}
	}
	return n;
}


elNodeId elf_fsnumnodesinlev(elFileState *fs) {
	elNode *nodes = fs->nodes;
	elNodeId nnodes = fs->nnodes;
	elNodeId n;
	for (n = 0; n < nnodes; ++n) {
		if (nodes[nnodes-1-n].level < fs->level) {
			break;
		}
	}
	return n;
}


void elf_check_store(elFileState *fs, elf_lineid line, elNodeId x) {
}


/*
** Looks for an enclosing entity within the
** function's enclosure and returns the index
** where the entity resides.
** The index can then be used to emit
** instructions targeting closure values.
*/
int elf_find_index_of_closure_entity(elFileFnState *fn, elEntityIdTypeGuard id) {
	elf_arrfori(fn->enclosure) {
		if (fn->enclosure[i].x == id.x) return i;
	}
	return -1;
}


/*
** Encloses an entity within the given function.
*/
void elf_enclose_entity(elFileState *fs, elFileFnState *fn, elEntityIdTypeGuard id) {
	/* ensure the entity should actually be captured */
	elf_ensure(id.x < fn->entities);
	elf_arrfori(fn->enclosure) {
		if (fn->enclosure[i].x == id.x) return;
	}
	elf_varadd(fn->enclosure,id);
}


/*
** Finds the last declared entity with the given
** name.
** The id returned is absolute, but can be made
** relative to the current function.
** You can specify whether to add this entity
** to the function's enclosure if it resides
** outside of its scope.
*/
elEntityIdTypeGuard elf_find_local_entity(elFileState *fs, elf_lineid line, char *name, elBool enclose) {
	elFileFnState *fn = fs->fn;
	for (int x = fs->nentities-1; x > -1; --x) {
		if (S_eq(fs->entities[x].name,name)) {
			elEntityIdTypeGuard id = {x};
			if ((x < fn->entities) && (enclose)) {
				elf_enclose_entity(fs,fn,id);
			}
			return id;
		}
	}
	return NO_ENTITY;
}


/*
** Register a local entity within the current function and level,
** if already declared issue a warning or error depending on
** whether is shadows or redeclares and existing entity, however,
** still returns a valid id.
*/
elNodeId elf_new_local_entity(elFileState *fs, elf_lineid line, char *name, elBool enm) {
	elFileFnState *fn = fs->fn;
	elEntityIdTypeGuard already = elf_find_local_entity(fs,line,name,false);
	if (already.x != NO_ENTITY.x) {
		elFileEntity entity = fs->entities[already.x];
		if (entity.level == fs->level) {
			elf_file_dialog(fs,line,"'%s': is already declared",name);
		} else {
			/* todo: only issue this warning if the entity is within the
			same function? */
			elf_file_dialog(fs,line,"'%s': this declaration shadows another one",name);
		}
	}

	elEntityIdTypeGuard id = {fs->nentities ++};
	if (elf_varlen(fs->entities) < fs->nentities) {
		elf_varaddi(fs->entities,1);
	}

	fs->entities[id.x].level = fs->level;
	fs->entities[id.x].line  = line;
	fs->entities[id.x].name  = 0;
	fs->entities[id.x].slot  = 0;
	fs->entities[id.x].enm   = false;

	elRegId slot = elf_alloc_register(fs,line,NO_SLOT,NO_NODE);
	elNodeId local = elf_make_local_value_node(fs,line,slot);
	// elf_file_dialog(fs,line,"new local entity, %s, n%i, r%i",name,local,slot);
	fs->entities[id.x].slot = slot;
	fs->entities[id.x].enm  = enm;
	fs->entities[id.x].name = name;
	return local;
}


elNodeId elf_find_entity_node(elFileState *fs, elf_lineid line, char *name) {
	elEntityIdTypeGuard id = elf_find_local_entity(fs,line,name,elTrue);
	if (id.x == NO_ENTITY.x) return NO_NODE;
	elFileFnState *fn = fs->fn;
	/* to figure out whether this is capture, simply
	check whether the entity id is higher than that
	of the first entity id within this function, in
	other words, if this entity is outside of this
	function's scope */
	if (id.x < fn->entities) {
		/* -- todo: implement multilayer caching */
		if (id.x < fn->enclosing->entities) {
			elf_file_dialog(fs,line,"too many layers for caching");
		}
		return elf_make_closure_value_node(fs,line,elf_find_index_of_closure_entity(fn,id));
	} else return elf_make_local_value_node(fs,line,fs->entities[id.x].slot);
}


void elf_enter_file_block(elFileState *fs, elFileBlockState *bl, elBool flags) {
	bl->enclosing = fs->fn->block;
	fs->fn->block = bl;
	bl->xmemory = fs->fn->xmemory;
	bl->xentity = fs->nentities;
	bl->flags = flags;
	bl->level = fs->level ++;
	bl->entry = elf_getlastbyteid(fs);
	bl->jumpover = bl->entry;
	fs->fn->nloops += (flags & BLOCK_LOOP) != 0;
	fs->fn->nblocks += 1;
}


void elf_leave_file_block(elFileState *fs) {
	fs->nentities -= elf_fsnumentinlev(fs); /* close scope */
	fs->nnodes -= elf_fsnumnodesinlev(fs);
	elf_ensure(fs->nentities >= fs->fn->entities);
	fs->level = fs->level - 1;
	elFileBlockState *bl = fs->fn->block;
	/* every expression and statement
	should deallocate whatever registers
	it used, so here we can assert that
	the memory state is identical, otherwise, bug! */
	elf_ensure(bl->level == fs->level);
	fs->fn->block = bl->enclosing;
	fs->fn->xmemory = bl->xmemory;
	if (bl->leavejumps != 0) {
		elf_tie_loose_jump_list(fs,bl->leavejumps);
		elf_delvar(bl->leavejumps);
		bl->leavejumps = 0;
	}
	fs->fn->nloops -= (bl->flags & BLOCK_LOOP) != 0;
}


void elf_begin_file_function(elFileState *fs, elFileFnState *fn, char *line) {
	fn->enclosing = fs->fn;
	fn->entities = fs->nentities;
	fn->bytes = fs->md->nbytes;
	fn->line = line;
	fn->yj = elNil;
	fs->fn = fn;
	elf_enter_file_block(fs,&fn->entry,0);
}


void elf_close_file_function(elFileState *fs) {
	elf_emit_function_epiloge(fs,fs->lasttk.line);
	elf_ensure(fs->fn->block == &fs->fn->entry);
	elf_leave_file_block(fs);
	/* ensure all locals were deallocated
	properly */
	elf_ensure(fs->nentities == fs->fn->entities);
	fs->fn = fs->fn->enclosing;
}


elNodeId *elf_load_call_args(elFileState *fs) {
	/* ( x { , x } ) */
	elNodeId *z = 0;
	if (elf_pick_token(fs,TK_PAREN_LEFT)) {
		if (!elf_test_token(fs,TK_PAREN_RIGHT)) do {
			elNodeId x = elf_load_file_expr(fs);
			if (x == NO_NODE) break;
			elf_varadd(z,x);
		} while (elf_pick_token(fs,TK_COMMA));
		elf_take_token(fs,TK_PAREN_RIGHT);
	}
	return z;
}


elNodeKi tktonode(ltokentype tk) {
	switch (tk) {
		case TK_DOT_DOT:            return NODE_RANGE;
		case TK_LOG_AND:            return NODE_AND;
		case TK_LOG_OR:             return NODE_OR;
		case TK_ADD:                return NODE_ADD;
		case TK_SUB:                return NODE_SUB;
		case TK_DIV:                return NODE_DIV;
		case TK_MUL:                return NODE_MUL;
		case TK_MODULUS:            return NODE_MOD;
		case TK_NOT_EQUALS:         return NODE_NEQ;
		case TK_EQUALS:             return NODE_EQ;
		case TK_GREATER_THAN:       return NODE_GT;
		case TK_GREATER_THAN_EQUAL: return NODE_GTEQ;
		case TK_LESS_THAN:          return NODE_LT;
		case TK_LESS_THAN_EQUAL:    return NODE_LTEQ;
		case TK_LEFT_SHIFT:         return NODE_BITSHL;
		case TK_RIGHT_SHIFT:        return NODE_BITSHR;
		case TK_BIT_XOR:            return NODE_BITXOR;
		case TK_BIT_OR:             return NODE_BITOR;
		default: 						 return NODE_NONE;
	}
}


elNodeId elf_load_file_subexpr(elFileState *fs, int rank) {
	elNodeId x = elf_load_unary_expr(fs,elTrue);
	if (x == NO_NODE) return x;
	for (;;) {
		int thisrank = elf_tkintel[fs->tk.type].prec;
		/* auto breaks when not a binary operator */
		if (thisrank <= rank) break;
		// *= += ...
		if (elf_test_then_token(fs,TK_ASSIGN))  {
			/* todo: check subexpression level
			to ensure the user knows this is
			not an expression, if we're at level
			0, the statement parser will handle
			this and turn it into a statement... */
			break;
		}
		elToken tk = elf_lexone(fs);
		elNodeId y = elf_load_file_subexpr(fs,thisrank);
		if (y == NO_NODE) break;
		x = elf_make_binary_node(fs,tk.line,tktonode(tk.type),NT_ANY,x,y);
	}
	return x;
}


elNodeId elf_load_file_function(elFileState *fs) {

	elToken tk = elf_take_token(fs,TK_FUN);

#if 0
	char *name = elNil;
	if (elf_pick_token(fs,TK_WORD)) {
		name = fs->lasttk.s;
	}
#endif

	int fj = elf_emit_jump(fs,tk.line,-1);

	elFileFnState fn = {0};
	elf_begin_file_function(fs,&fn,tk.line);

	int arity = 0;

	elf_take_token(fs,TK_PAREN_LEFT);
	if (!elf_test_token(fs,TK_PAREN_RIGHT)) do {

		elToken n = elf_take_token(fs,TK_WORD);
		elf_new_local_entity(fs,n.line,n.s,false);

		arity ++;
	} while (elf_pick_token(fs,TK_COMMA));
	if (!elf_test_token(fs,TK_PAREN_RIGHT)) {
		elf_file_dialog(fs,fs->tk.line,"did you miss a ','?");
	}
	elf_take_token(fs,TK_PAREN_RIGHT);

	elf_take_token(fs,TK_QUESTION_MARK);

	// ? { .. }
	if (elf_test_token(fs,TK_CURLY_LEFT)) {
		elf_take_token(fs,TK_CURLY_LEFT);
		while (!elf_term_token(fs,TK_CURLY_RIGHT)) {
			elf_load_file_stat(fs);
		}
		elf_take_token(fs,TK_CURLY_RIGHT);
	} else {
		elf_emit_yield(fs,tk.line,elf_load_file_expr(fs));
	}

	elf_close_file_function(fs);

	/* add this function to the type table */
	elProto p = {0};
	p.x = arity;
	p.y = fn.nyield;
	p.zstack = fn.nlocals;
	p.bytes = fn.bytes;
	p.nbytes  = fs->md->nbytes - fn.bytes;
	p.zcache = elf_varlen(fn.enclosure);
	int f = elf_add_proto(fs->M,p);

	elf_tie_loose_jump(fs,fj);

	/* todo: */
	elNodeId *z = elNil;
	elf_arrfori(fn.enclosure) {
		elf_varadd(z,elf_make_local_value_node(fs,tk.line,fs->entities[fn.enclosure[i].x].slot));
	}

	return elf_make_closure_node(fs,tk.line,f,z);
}


/*
** Completes the expression line, or statement,
** takes the lhs expression,
** if not followed by a statement operator, such
** as '=' then it simply emits code to evaluate
** the expression.
*/
void elf_complete_expr_line(elFileState *fs, elNodeId x) {
	if (x == NO_NODE) {
		return;
	}

	elToken tk = fs->tk;
	elRegId mem = elf_get_memory_state(fs);

	if (elf_pick_token(fs,TK_ASSIGN)) {
		elf_check_store(fs,tk.line,x);
		elNodeId y = elf_load_file_expr(fs);
		elf_gen_store(fs,tk.line,x,y);
	} else if (elf_pick_token(fs,TK_ASSIGN_QUESTION)) {
		elf_check_store(fs,tk.line,x);
		elNodeId y = elf_load_file_expr(fs);
		elFileJumplist js = {0};
		/* todo: this will evaluate the expression twice, we don't
		want that, isntead we can reuse the previous registers by
		deffering deallocation of those registers until statement
		end, for instance:
		x.y ?= 0
		here x.y will be evaluated twice, once to check whether
		it is nil, and once more to actually assign 0 to it.
		to do this we have to split this function into its components,
		allocate a register for getting x.y and letting jumpifxx
		allocate whatever temporary registers it needs and freeing
		them automatically, the only remaining register will be where
		x.y is at, at which point once the assignment is done it can be
		deallocated. */
		elf_emit_jump_if_not_nil(fs,tk.line,&js,x);
		elf_gen_store(fs,tk.line,x,y);
		elf_tie_loose_jump_list(fs,js.f);
	} else {
		/* the lhs of an assignment is an expression,
		and thus it is parsed by the expression parser.
		the expression parsers checks whether there's
		an operator to make a binary expression however,
		if the operator is followed by '=' then it quits,
		so if we get here and we see an operator it's
		guaranteed to be a '{x}=' assignment. */
		if (elf_token_is_operator(tk)) {
			elToken op = elf_lexone(fs);
			elf_take_token(fs,TK_ASSIGN);
			elf_check_store(fs,fs->lasttk.line,x);
			elNodeId y = elf_load_file_expr(fs);
			y = elf_make_binary_node(fs,op.line,tktonode(op.type),fs->nodes[x].t,x,y);
			elf_gen_store(fs,op.line,x,y);
		} else {
			elf_gen_local_load(fs,NO_LINE,0,NO_SLOT,0,x);
		}
		elf_set_memory_state(fs,mem);
	}
	elf_ensure(fs->fn->xmemory == mem);
}


elNodeId elf_load_file_table(elFileState *fs) {
	elToken tk = fs->tk;
	elf_take_token(fs,TK_CURLY_LEFT);
	elNodeId *z = elNil;
	elNodeId table = elf_make_table_node(fs,tk.line,elNil);
	int index = 0;
	while (!elf_term_token(fs,TK_CURLY_RIGHT)) {
		tk = fs->tk;
		elNodeId key;
		if (elf_test_token(fs,TK_WORD) && elf_test_then_token(fs,TK_ASSIGN)) {
			elf_lexone(fs);
			key = elf_make_string_node(fs,tk.line,tk.s);
		} else {
			key = elf_load_file_expr(fs);
		}
		if (elf_pick_token(fs,TK_ASSIGN)) {
			elNodeId val = elf_load_file_expr(fs);
			elNodeId fld = elf_make_field_node(fs,fs->lasttk.line,table,key);
			elNodeId f = elf_make_load_node(fs,fs->lasttk.line,fld,val);
			elf_varadd(z,f);
		} else {
			elNodeId val = key;
			//elf_load_file_expr(fs);
			if (elf_fscheckexpr(fs,fs->tk.line,val)) break;
			elNodeId ii = elf_make_integer_node(fs,fs->lasttk.line,index ++);
			elNodeId f = elf_make_load_node(fs,fs->lasttk.line,elf_nodeindex(fs,fs->lasttk.line,table,ii),val);
			elf_varadd(z,f);
		}
		if (elf_pick_token(fs,TK_COMMA)) {
			continue;
		}
	}
	elf_take_token(fs,TK_CURLY_RIGHT);
	fs->nodes[table].z = z;
	return table;
}


elNodeId elf_load_file_expr(elFileState *fs) {
	/* todo?: do this in else where? */
	switch (fs->tk.type) {
		case TK_NONE:
		case TK_PAREN_RIGHT:
		case TK_CURLY_RIGHT:
		case TK_SQUARE_RIGHT: {
			return NO_NODE;
		}
	}
	return elf_load_file_subexpr(fs,0);
}


elNodeId elf_get_global_entity_node(elFileState *fs, elf_lineid line, char *name) {
	elSymbolID x = elf_get_global_symbol(fs->M,elf_newstr(fs->R,name));
	elf_ensure(x != -1);
	return elf_make_global_value_node(fs,line,x);
}


elNodeId elf_get_local_or_global_entity_node(elFileState *fs, elf_lineid line, char *name) {
	elNodeId v = elf_find_entity_node(fs,line,name);
	if (v == NO_NODE) {
		/* todo: instead, check whether we've assigned a value
		to this entity already, otherwise issue a warning that
		we're using something that hasn't got a value yet... */

		// elf_file_dialog(fs,line,"warning: '%s' implicit global declaration, did you mean this?",name);
		v = elf_get_global_entity_node(fs,line,name);
	}
	if (v == NO_NODE) {
		elf_file_dialog(fs,line,"'%s': undeclared identifier",name);
	}
	return v;
}


elNodeId elf_make_call_pf_node(elFileState *fs, elf_lineid line, elNodeId *args) {
	elNodeId fn = elf_get_global_entity_node(fs,line,"elf.pf");
	return elf_make_call_node(fs,line,fn,args);
}


elNodeId elf_make_set_meta_table_node(elFileState *fs, elf_lineid line, elNodeId table, elNodeId meta_table) {

	elNodeId fn = elf_get_global_entity_node(fs,line,"elf.setmetatable");

	elNodeId *z = elNil;
	elf_varadd(z,table);
	elf_varadd(z,meta_table);

	return elf_make_call_node(fs,line,fn,z);
}


elNodeId elf_load_unary_expr(elFileState *fs, elBool allow_postfix) {
	LDODEBUG(
		if (fs->debuggerflag) {
			elf_debugger("__ELF_FILE_BREAK__");
		}
	);
	elNodeId v = NO_NODE;
	elToken tk = fs->tk;
	switch (tk.type) {
		case TK_M_REGISTER: {
			elf_lexone(fs);
			tk = elf_take_token(fs,TK_WORD);
			elEntityIdTypeGuard entity = elf_find_local_entity(fs,tk.line,tk.s,elFalse);
			if (entity.x == NO_ENTITY.x) {
				elf_file_dialog(fs,tk.line,"'%s': invalid entity (must be a local)",tk.s);
			}
			v = elf_make_integer_node(fs,tk.line,fs->entities[entity.x].slot);
		} break;
		//
		// 'new' <meta-table> ( <argument-list> )
		//
		// leave elf.setmetatable({},Vector2):__new(x,y)
		//
		case TK_NEW: {
			elf_lexone(fs);
			elNodeId *control = elNil;

			elNodeId meta_table = elf_load_unary_expr(fs,elFalse);
			elNodeId table = elf_make_table_node(fs,tk.line,elNil);
			table = elf_make_set_meta_table_node(fs,tk.line,table,meta_table);

			elNodeId meta_field_name = elf_make_string_node(fs,tk.line,"__new");
			elNodeId get_meta_field = elf_make_meta_field_node(fs,tk.line,table,meta_field_name);

			elNodeId *call_args = elf_load_call_args(fs);
			elNodeId call_new = elf_make_call_node(fs,tk.line,get_meta_field,call_args);

			v = call_new;
		} break;
		/* elf is a reserved keyword used
		for the elf symbol tree. */
		case TK_DOT: case TK_ELF: {
			char dir[MAX_PATH] = {};
			if (elf_pick_token(fs,TK_ELF)) {
				strcat(dir,"elf");
				/* remind the user elf is a reserved keyword */
				if (!elf_linetesttk(fs,TK_DOT)) {
					elf_file_dialog(fs,tk.line,"incomplete symbol, expected '.' on the same line as 'elf'. Did you mean to use 'elf'? This is a reserved keyword and it refers to the elf directory.");
				}
			}
			elf_take_token(fs,TK_DOT);
			do {
				elf_take_token_inline(fs,TK_WORD);
				strcat(dir,".");
				strcat(dir,fs->lasttk.s);
			} while (elf_pick_token_inline(fs,TK_DOT));

			elSymbolID x = elf_get_global_symbol(fs->M,elf_newstr(fs->R,dir));
			v = elf_make_global_value_node(fs,tk.line,x);
		} break;
		case TK_WORD: {
			elf_lexone(fs);
			v = elf_get_local_or_global_entity_node(fs,tk.line,tk.s);
		} break;
		case TK_SUB: {
			elf_lexone(fs);
			v = elf_load_file_subexpr(fs,10000);
			v = elf_make_binary_node(fs,tk.line,NODE_SUB,NT_INT,elf_make_integer_node(fs,tk.line,0),v);
		} break;
		case TK_ADD: {
			elf_lexone(fs);
			v = elf_load_file_subexpr(fs,10000);
		} break;
		case TK_CURLY_LEFT: {
			v = elf_load_file_table(fs);
		} break;
		case TK_PAREN_LEFT: {
			elf_lexone(fs);
			v = elf_load_file_expr(fs);
			elf_take_token(fs,TK_PAREN_RIGHT);
			/* allow for empty () */
			if (v != NO_NODE) {
				v = elf_make_group_node(fs,tk.line,v);
			}
		} break;
		case TK_FUN: {
			v = elf_load_file_function(fs);
		} break;
		case TK_LOAD: {
			elf_lexone(fs);
			v = elf_load_unary_expr(fs,elTrue);
			v = elf_nodeloadfile(fs,tk.line,v);
		} break;
		case TK_THIS: { elf_lexone(fs);
			v = elf_nodenullary(fs,tk.line,NODE_THIS,NT_ANY);
		} break;
		case TK_NIL: { elf_lexone(fs);
			v = elf_nodenil(fs,tk.line);
		} break;
		/* todo: maybe use proper boolean node? */
		case TK_TRUE: case TK_FALSE: { elf_lexone(fs);
			v = elf_make_integer_node(fs,tk.line,tk.type == TK_TRUE);
		} break;
		case TK_LETTER: case TK_INTEGER: { elf_lexone(fs);
			v = elf_make_integer_node(fs,tk.line,tk.i);
		} break;
		case TK_NUMBER: { elf_lexone(fs);
			v = elf_nodenum(fs,tk.line,tk.n);
		} break;
		case TK_STRING: { elf_lexone(fs);
			v = elf_make_string_node(fs,tk.line,tk.s);
		} break;
		default: {
			elf_file_dialog(fs,tk.line,"'%s': unexpected token", elf_tkintel[tk.type].name);
			elf_debugger("test-break");
		} break;
	}

	LDODEBUG(
		if (fs->debuggerflag) {
			elf_debugger("__ELF_FILE_BREAK__");
		}
	);


	/* termeol: solves some ambiguities,
	for instance:
	.RED
	.DrawCircle(...)
	If it wasn't for this it would think it was:
	.RED.DrawCircle(...) */
	if (allow_postfix) while (!elf_term_eol_token(fs)) {
		tk = fs->tk;

		switch (tk.type) {
			case TK_DOT: { elf_lexone(fs);
				elToken n = elf_take_token(fs,TK_WORD);
				elNodeId i = elf_make_string_node(fs,n.line,n.s);
				v = elf_make_field_node(fs,tk.line,v,i);
			} break;
			case TK_SQUARE_LEFT: {
				elf_take_token(fs,TK_SQUARE_LEFT);
				elNodeId i = elf_load_file_expr(fs);
				elf_take_token(fs,TK_SQUARE_RIGHT);
				if (fs->nodes[i].k == NODE_RANGE_INDEX) {
					elf_unreachable;
				} else
				if (fs->nodes[i].k == NODE_RANGE) {
					v = elf_noderangedindex(fs,tk.line,v,i);
				} else {
					v = elf_nodeindex(fs,tk.line,v,i);
				}
			} break;
			case TK_COLON: { elf_lexone(fs);
				elToken n = elf_take_token(fs,TK_WORD);
				elNodeId y = elf_make_string_node(fs,n.line,n.s);
				v = elf_make_meta_field_node(fs,tk.line,v,y);
			} break;
			case TK_PAREN_LEFT: {
				elNodeId *z = elf_load_call_args(fs);
				v = elf_make_call_node(fs,tk.line,v,z);
			} break;
			default: goto leave;
		}
	}

	leave:
	return v;
}


/* todo: make this legit */
void elfY_loadenumlist(elFileState *fs) {
	if (!elf_test_token(fs,TK_CURLY_RIGHT)) {
		do {
			/* , } */
			if (elf_test_token(fs,TK_CURLY_RIGHT)) {
				break;
			}
			elToken tk = fs->tk;
			elToken n = elf_take_token(fs,TK_WORD);
			elNodeId x = elf_new_local_entity(fs,n.line,n.s,elTrue);
			elf_take_token(fs,TK_ASSIGN);
			elNodeId y = elf_load_file_expr(fs);
			elf_gen_store(fs,tk.line,x,y);
		} while(elf_pick_token(fs,TK_COMMA));
	}
}


void elf_load_file_stat(elFileState *fs) {
	elRegId mem = fs->fn->xmemory;
	elToken tk = fs->tk;
	elFileFnState *fn = fs->fn;
	elFileBlockState *bl = fn->block;
	if (bl->flags & BLOCK_ENDED) {
		elf_file_dialog(fs,tk.line,"warning: unreachable statement");
	}
	switch (tk.type) {
		case TK_THEN: case TK_ELSE: case TK_ELIF: {
		} break;
		case TK_LASTLY: case TK_FINALLY: {
			elf_lexone(fs);
			if (tk.type == TK_FINALLY) {
				elf_file_dialog(fs,tk.line,"warning: please consider using 'lastly' instead, 'finally' could change semantics in the future");
			}
			elFileBlockState bl = {0};
			elf_enter_delayed_block(fs,tk.line,&bl);
			elf_load_file_stat(fs);
			elf_leave_delayed_block(fs,tk.line,&bl);
			elf_ensure(fs->fn->xmemory == mem);
		} break;
		case TK_IF: case TK_IFF: {
			elf_lexone(fs);
			elNodeId x = elf_load_file_expr(fs);
			elf_take_token(fs,TK_QUESTION_MARK);
			elFileBlockState block = {0};
			elf_enter_file_block(fs,&block,0);
			elSelectState s = {0};
			elf_gen_begin_if(fs,tk.line,&s,x,tk.type==TK_IFF?L_IFF:L_IF);
			elf_load_file_stat(fs);
			while (!elf_test_token(fs,TK_NONE)) {
				elFileBlockState block = {0};
				if (elf_pick_token(fs,TK_ELIF)) {
					elf_enter_file_block(fs,&block,0);
					x = elf_load_file_expr(fs);
					elf_take_token(fs,TK_QUESTION_MARK);
					elf_gen_add_elif(fs,fs->lasttk.line,&s,x);
					elf_load_file_stat(fs);
					elf_leave_file_block(fs);
				} else
				if (elf_pick_token(fs,TK_THEN)) {
					elf_enter_file_block(fs,&block,0);
					elf_gen_add_then(fs,fs->lasttk.line,&s);
					elf_load_file_stat(fs);
					elf_leave_file_block(fs);
				} else
				if (elf_pick_token(fs,TK_ELSE)) {
					elf_enter_file_block(fs,&block,0);
					elf_gen_add_else(fs,fs->lasttk.line,&s);
					elf_load_file_stat(fs);
					elf_leave_file_block(fs);
				} else break;
			}
			elf_close_if(fs,fs->lasttk.line,&s);
			elf_leave_file_block(fs);
			elf_ensure(fs->fn->xmemory == mem);
		} break;
		case TK_LET: { elf_lexone(fs);
			if (tk.type == TK_ENUM) {
				elf_file_dialog(fs,tk.line,"global enums are not supported yet, this enum will be made local");
			} else
			/* allows for 'let enum' */
			if (tk.type == TK_LET) {
				if (elf_test_token(fs,TK_ENUM)) {
					tk = elf_lexone(fs);
				}
			}
			elBool enm = tk.type == TK_ENUM;
			if (elf_pick_token(fs,TK_CURLY_LEFT)) {
				elfY_loadenumlist(fs);
				elf_take_token(fs,TK_CURLY_RIGHT);
			} else {
				do {
					elToken n = elf_take_token(fs,TK_WORD);
					elNodeId x = elf_new_local_entity(fs,n.line,n.s,enm);
					elf_complete_expr_line(fs,x);
				} while (elf_pick_token(fs,TK_COMMA));
			}
		} break;
		case TK_LEAVE: { elf_lexone(fs);
			bl->flags |= BLOCK_ENDED;
			elNodeId x = elf_load_file_expr(fs);
			elf_emit_yield(fs,tk.line,x);
			elf_ensure(fs->fn->xmemory == mem);
		} break;
		case TK_CONTINUE: { elf_lexone(fs);
			elf_emitcontinue(fs,tk.line);
		} break;
		case TK_BREAK: { elf_lexone(fs);
			elf_emitbreak(fs,tk.line);
		} break;
		case TK_WHILE: { elf_lexone(fs);
			elFileBlockState block = {0};
			elf_enter_file_block(fs,&block,BLOCK_LOOP);
			elNodeId x = elf_load_file_expr(fs);
			elf_take_token(fs,TK_QUESTION_MARK);
			elf_begin_while_loop(fs,tk.line,x);
			elf_load_file_stat(fs);
			elf_close_while_loop(fs,tk.line);
			elf_ensure(fs->fn->xmemory == mem);
			elf_leave_file_block(fs);
		} break;
		case TK_DO: { elf_lexone(fs);
			elFileBlockState block = {0};
			elf_enter_file_block(fs,&block,BLOCK_LOOP);
			elf_begin_do_while_loop(fs,tk.line);
			elf_load_file_stat(fs);
			elf_take_token(fs,TK_WHILE);
			elNodeId x = elf_load_file_expr(fs);
			elf_close_do_while_loop(fs,tk.line,x);
			elf_ensure(fs->fn->xmemory == mem);
			elf_leave_file_block(fs);
		} break;
		case TK_FOR: {
			elf_lexone(fs);
			elFileBlockState block = {0};
			elf_enter_file_block(fs,&block,BLOCK_LOOP);
			elToken n = elf_take_token(fs,TK_WORD);
			elNodeId x = elf_new_local_entity(fs,n.line,n.s,false);
			// elf_take_token(fs,TK_IN);
			if (elf_pick_token(fs,TK_IN)) {
				elNodeId y = elf_load_file_expr(fs);
				elf_take_token(fs,TK_QUESTION_MARK);
				elNodeId lo = NO_NODE;
				elNodeId hi = NO_NODE;
				if (fs->nodes[y].k == NODE_RANGE) {
					lo = fs->nodes[y].x;
					hi = fs->nodes[y].y;
				} else {
					lo = elf_make_integer_node(fs,tk.line,0);
					hi = y;
				}
				elf_begin_ranged_loop(fs,tk.line,x,lo,hi);
				elf_load_file_stat(fs);
				elf_close_ranged_loop(fs,tk.line);
			} else elf_unreachable;
			elf_leave_file_block(fs);
			// if (tk.type == TK_FOR) {
			// } else {
			// 	elf_unreachable;/* todo: emit code to initialize x to i'th item of y */
			// 	if (fs->nodes[y].k == NODE_RANGE) {
			// 		lo = fs->nodes[y].x;
			// 		hi = fs->nodes[y].y;
			// 	} else {
			// 		lo = elf_make_integer_node(fs,tk.line,0);
			// 		/* todo: add type guard */
			// 		hi = elf_make_meta_field_node(fs,tk.line,y,elf_make_string_node(fs,tk.line,"length"));
			// 		i = elf_make_local_value_node(fs,tk.line,elf_reserve_register(fs,1));
			// 	}
			// }
		} break;
		case TK_CURLY_LEFT: { elf_lexone(fs);
			elFileBlockState block = {0};
			elf_enter_file_block(fs,&block,0);
			while (!elf_term_token(fs,TK_CURLY_RIGHT)) {
				elf_load_file_stat(fs);
			}
			elf_take_token(fs,TK_CURLY_RIGHT);
			elf_leave_file_block(fs);
		} break;
		default: {
			elNodeId x = elf_load_file_expr(fs);
			elf_complete_expr_line(fs,x);
			elf_ensure(fs->fn->xmemory == mem);
			elf_ensure(x != NO_NODE);
		} break;
	}
}


