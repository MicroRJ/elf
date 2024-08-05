/*
** See Copyright Notice In elf.h
** elf-file.c
** Parsing Stuff
*/


char *elf_get_file_name(elFileState *fs) {
	return fs->filename;
}


elBool elf_check_expr(elFileState *fs, elFileLine line, elNodeId id) {
	if (id != NO_NODE) return 0;
	elf_file_dialog(fs,line,"invalid expression");
	return 1;
}


elBool elf_test_token_inline(elFileState *fs, elTokenType k) {
	return fs->tk.type == k && fs->lasttk.eol != 1;
}


elBool elf_test_token(elFileState *fs, elTokenType k) {
	return fs->tk.type == k;
}

elBool elf_test_then_token(elFileState *fs, elTokenType k) {
	return fs->thentk.type == k;
}


/*
** Returns 1 whether there are no more tokens
** or whether the current token is a match.
*/
elBool elf_term_token(elFileState *fs, elTokenType k) {
	return fs->tk.type == TK_NONE || fs->tk.type == k;
}


elBool elf_term_eol_token(elFileState *fs) {
	return fs->tk.type == TK_NONE || fs->last_token.eol == 1;
}


/*
** Consumes the current token only if it is a match,
** returning whether it was a match or not.
*/
elBool elf_pick_token(elFileState *fs, elTokenType k) {
	return elf_test_token(fs,k) && (elf_lexone(fs), 1);
}


elBool elf_pick_token_inline(elFileState *fs, elTokenType k) {
	return elf_test_token_inline(fs,k) && (elf_lexone(fs), 1);
}


/*
** Same as pick, only this time there
** are two possibilities.
*/
elBool elf_choose_token(elFileState *fs, elTokenType x, elTokenType y) {
	return (elf_test_token(fs,x) || elf_test_token(fs,y)) && (elf_lexone(fs), 1);
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


void elf_check_store(elFileState *fs, elFileLine line, elNodeId x, elNodeId y) {
	if (y < 0) {
		elf_file_dialog(fs,line,"invalid statement, expected a value for assignment");
	}
	elASSERT(x > NO_SLOT);
	elASSERT(y > NO_SLOT);
	elNode node = elf_get_targetable_node(fs,MAKE_NODE_ID(x));
	if (elf_is_targetable_node(node.kind)) {
	} else elf_file_dialog(fs,line,"invalid store (%s)",elNodeToStr[node.kind]);

	if (node.kind == NODE_LOCAL) {
		elFileFnState *fn = fs->fn;
		for (elEntityId id = fs->nentities-1; id > -1; -- id) {
			if (fs->entities[id].slot == node.x) {
				fs->entities[id].flags |= ENTITY_ASSIGNED;
				return;
			}
		}
		elf_file_dialog(fs,line,"internal error");
	}
}


/*
** Looks for an enclosing entity within the
** function's enclosure and returns the index
** where the entity resides.
** The index can then be used to emit
** instructions targeting closure values.
*/
int elf_find_index_of_closure_entity(elFileFnState *fn, elEntityIdTypeGuard id) {
	elf_xarray_foreachi(fn->enclosure) {
		if (fn->enclosure[i] == id.id) {
			return i;
		}
	}
	return NO_SLOT;
}


/*
** Encloses an entity within the given function.
*/
void elf_enclose_entity(elFileState *fs, elFileFnState *fn, elEntityIdTypeGuard id) {
	/* ensure the entity should actually be captured */
	elASSERT(id.id < fn->entities);

	for (int i = 0; i < ARRAY_LENGTH(fn->enclosure); i += 1) {
		if (fn->enclosure[i] == id.id) return;
	}

	ARRAY_ADD(fn->enclosure,id.id);
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
elEntityId elf_find_entity(elFileState *fs, elFileLine line, char *name, elBool enclose) {
	elFileFnState *fn = fs->fn;
	for (elEntityId id = fs->nentities-1; id > -1; -- id) {
		if (S_eq(fs->entities[id].name,name)) {
			if ((id < fn->entities) && (enclose)) {
				elf_enclose_entity(fs,fn,(elEntityIdTypeGuard){id});
			}
			fs->entities[id].flags |= ENTITY_REFERENCED;
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
elNodeId elf_new_local_entity(elFileState *fs, elFileLine line, char *name, elBool flags) {
	elFileFnState *fn = fs->fn;
	elEntityId already = elf_find_entity(fs,line,name,0);
	if (already != NO_ENTITY) {
		elFileEntity entity = fs->entities[already];
		if (entity.kind == ENTITY_DIRECTORY)  {
			elf_file_dialog(fs,line,"'%s': name is reserved for symbol directory",name);
		}
		if (entity.level == fs->level) {
			elf_file_dialog(fs,line,"'%s': is already declared",name);
		} else {
			/* only issue this warning if the entity we found
			is within this function... */
			if (already >= fn->entities) {
				elf_file_dialog(fs,line,"'%s': this declaration shadows another one",name);
			}
		}
	}

	elEntityId id = fs->nentities ++;
	elf_xarray_growby(fs->entities,fs->nentities-ARRAY_LENGTH(fs->entities));

	elRegId slot = elf_emitter_local_alloc(fs,line,NO_SLOT,NO_NODE);
	elNodeId node = elf_make_register_node(fs,line,slot);

	fs->entities[id].kind  = ENTITY_LOCAL;
	fs->entities[id].line  = line;
	fs->entities[id].name  = name;
	// fs->entities[id].node  = node;
	fs->entities[id].slot  = slot;
	fs->entities[id].level = fs->level;
	fs->entities[id].flags = flags;
	return node;
}


elNodeId elf_find_entity_node(elFileState *fs, elFileLine line, char *name, int flags) {
	elEntityId id = elf_find_entity(fs,line,name,1);

	if (id == NO_ENTITY) return NO_NODE;

	if (flags) {
		if (~fs->entities[id].flags & ENTITY_ASSIGNED) {
			elf_file_dialog(fs,line,"warning: possibly unassigned variable");
		}
	}

	elFileFnState *fn = fs->fn;
	/* to figure out whether this is capture, simply
	check whether the entity id is higher than that
	of the first entity id within this function, in
	other words, if this entity is outside of this
	function's scope */
	if (id < fn->entities) {
		/* todo: implement multilayer caching */
		if (id < fn->enclosing->entities) {
			elf_file_dialog(fs,line,"too many layers for caching");
		}
		return elf_make_closure_value_node(fs,line,elf_find_index_of_closure_entity(fn,(elEntityIdTypeGuard){id}));
	} else return elf_make_register_node(fs,line,fs->entities[id].slot);
}


elNodeId elf_get_global_entity_node(elFileState *fs, elFileLine line, char *name) {
	elSymbolId x = elf_get_global_symbol(fs->M,elf_new_string(fs->R,name));
	elASSERT(x != -1);
	return elf_make_global_value_node(fs,line,x);
}


elNodeId elf_get_local_or_global_entity_node(elFileState *fs, elFileLine line, char *name, int flags) {
	elNodeId v = elf_find_entity_node(fs,line,name,flags);

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


elNodeId elf_make_call_pf_node(elFileState *fs, elFileLine line, elNodeId *args) {
	elNodeId fn = elf_get_global_entity_node(fs,line,"elf.pf");
	return elf_make_call_node(fs,line,fn,args);
}


elNodeId elf_make_set_metatable_node(elFileState *fs, elFileLine line, elNodeId table, elNodeId meta_table) {

	elNodeId fn = elf_get_global_entity_node(fs,line,"elf.set_object_metatable");

	elNodeId *z = 0;
	ARRAY_ADD(z,table);
	ARRAY_ADD(z,meta_table);

	return elf_make_call_node(fs,line,fn,z);
}


elNodeId elf_load_file_table(elFileState *fs);
elNodeId *elf_load_call_args(elFileState *fs) {
	/* ( x { , x } ) | { <table-initializer-list> } */
	elNodeId *z = 0;
	if (elf_test_token(fs,TK_CURLY_LEFT)) {
		elNodeId x = elf_load_file_table(fs);
		ARRAY_ADD(z,x);
	} else
	if (elf_pick_token(fs,TK_PAREN_LEFT)) {
		if (!elf_test_token(fs,TK_PAREN_RIGHT)) do {
			elNodeId x = elf_load_file_expr(fs,0);
			if (x == NO_NODE) break;
			if (elf_get_node_kind(fs,x) == NODE_MULTI) {
				elNodeId *n = elf_get_node(fs,x).z;
				elf_xarray_foreachi(n) {
					ARRAY_ADD(z,n[i]);
				}
			} else {
				ARRAY_ADD(z,x);
			}
		} while (elf_pick_token(fs,TK_COMMA));
		elf_take_token(fs,TK_PAREN_RIGHT);
	}
	return z;
}


elNodeId *elf_load_call_args_or_expr(elFileState *fs) {
	/* x or ( x { , x } ) */
	elNodeId *z = 0;
	if (elf_pick_token(fs,TK_PAREN_LEFT)) {
		if (!elf_test_token(fs,TK_PAREN_RIGHT)) do {
			elNodeId x = elf_load_file_expr(fs,0);
			if (x != NO_NODE) ARRAY_ADD(z,x);
			else break;
		} while (elf_pick_token(fs,TK_COMMA));
		elf_take_token(fs,TK_PAREN_RIGHT);
	} else {
		elNodeId x = elf_load_file_expr(fs,0);
		if (x != NO_NODE) ARRAY_ADD(z,x);
	}
	return z;
}


elTokenType elf_is_operator_token_contextually(elToken tk) {
	/* could be done in the lexer */
	if (tk.type != TK_WORD) return tk.type;
	if (!strcmp(tk.s,"and")) return TK_LOG_AND;
	if (!strcmp(tk.s,"or")) return TK_LOG_OR;
	if (!strcmp(tk.s,"is")) return TK_EQ;
	return TK_WORD;
}


int elf_get_token_binding_priority(elTokenType type) {
	return elf_tkintel[type].prec;
}


elNodeKi elf_token_to_node(elTokenType tk) {
	switch (tk) {
		case TK_DOT_DOT: return NODE_RANGE;
		case TK_LOG_AND: return NODE_AND;
		case TK_LOG_OR: return NODE_OR;
		case TK_NIL_OR: return NODE_NIL_OR;
		case TK_NIL_AND: return NODE_NIL_AND;
		case TK_ADD: return NODE_ADD;
		case TK_SUB: return NODE_SUB;
		case TK_DIV: return NODE_DIV;
		case TK_MUL: return NODE_MUL;
		case TK_POW: return NODE_POW;
		case TK_MOD: return NODE_MOD;
		case TK_NEQ: return NODE_NEQ;
		case TK_EQ: return NODE_EQ;
		case TK_GT: return NODE_GT;
		case TK_GTEQ: return NODE_GTEQ;
		case TK_LT: return NODE_LT;
		case TK_LTEQ: return NODE_LTEQ;
		case TK_SHL: return NODE_BIT_SHL;
		case TK_SHR: return NODE_BIT_SHR;
		case TK_BIT_XOR: return NODE_BIT_XOR;
		case TK_BIT_OR: return NODE_BIT_OR;
		case TK_BIT_AND: return NODE_BIT_AND;
		default: return NODE_NONE;
	}
}


elNodeId elf_load_file_subexpr(elFileState *fs, int rank, int flags) {
	elNodeId x = elf_load_unary_expr(fs,1,flags);
	if (x == NO_NODE) return x;
	for (;;) {
		elTokenType op = elf_is_operator_token_contextually(fs->this_token);
		int prio = elf_get_token_binding_priority(op);
		/* auto breaks when not a binary operator */
		if (prio <= rank) break;
		// *= += ...
		if (elf_test_then_token(fs,TK_ASSIGN))  {
			/* todo: check subexpression level
			to ensure the user knows this is
			not an expression, for instance,
			'(i = 1)' 'i = 1' is at level 1,
			if we're at level 0, the statement
			parser will handle this and turn it
			into a statement... */
			break;
		}
		elToken tk = elf_lexone(fs);
		elNodeId y = elf_load_file_subexpr(fs,prio,flags);
		if (y == NO_NODE) break;
		x = elf_make_binary_node(fs,tk.line,elf_token_to_node(op),NT_ANY,x,y);
	}
	return x;
}


/* Named functions aren't a thing for this
language... at least for now... */
elNodeId elf_load_file_function(elFileState *fs) {
	elToken tk = elf_take_token(fs,TK_FUN);

	int fj = elf_emit_jump(fs,tk.line,-1);

	elFileFnState fn = {0};
	elf_emitter_begin_function(fs,&fn,tk.line);

	int arity = 0;

	elf_take_token(fs,TK_PAREN_LEFT);
	if (!elf_test_token(fs,TK_PAREN_RIGHT)) do {

		elToken n = elf_take_token(fs,TK_WORD);
		elf_new_local_entity(fs,n.line,n.s,ENTITY_PARAMETER|ENTITY_ASSIGNED);

		arity ++;
	} while (elf_pick_token(fs,TK_COMMA));

	if (!elf_test_token(fs,TK_PAREN_RIGHT)) {
		elf_file_dialog(fs,fs->tk.line,"did you miss a ','?");
	}

	elf_take_token(fs,TK_PAREN_RIGHT);

	elf_take_token(fs,TK_QUESTION_MARK);

	// ? { .. }
	/* todo: couldn't we just use load file statement instead? */
	if (elf_test_token(fs,TK_CURLY_LEFT)) {
		elf_take_token(fs,TK_CURLY_LEFT);
		while (!elf_term_token(fs,TK_CURLY_RIGHT)) {
			elf_load_file_stat(fs);
		}
		elf_take_token(fs,TK_CURLY_RIGHT);
	} else {
		elf_emit_yield(fs,tk.line,elf_load_file_expr(fs,0));
	}

	elf_emitter_close_function(fs);

	/* add this function to the prototype table... */
	elFileProto fp = {0};
	/* todo: implement this */
	// fp.parent  = fn.enclosing
	fp.x 		  = arity;
	fp.y 		  = fn.nyield;
	fp.nlocals = fn.nlocals;
	fp.nvalues = ARRAY_LENGTH(fn.enclosure);
	fp.bytes   = fn.bytes;
	fp.nbytes  = fs->M->nbytes - fn.bytes;
	int f = elf_add_proto(fs->M,fp);

	elf_emitter_patch_jump(fs,fj);

	/* todo: */
	elNodeId *z = 0;
	elf_xarray_foreachi(fn.enclosure) {
		elFileEntity entity = fs->entities[fn.enclosure[i]];
		ARRAY_ADD(z,elf_make_register_node(fs,entity.line,entity.slot));
	}

	return elf_make_closure_node(fs,tk.line,f,z);
}


/*
** Completes the expression line, possibly a statement.
** Takes the lhs expression, if not followed by a
** statement operator, such as '=' then it simply
** emits code to evaluate the expression.
*/
void elf_complete_stat(elFileState *fs, elNodeId lhs) {
	if (lhs == NO_NODE) {
		return;
	}

	elToken tk = fs->tk;
	elRegId mem = elf_get_memory_state(fs);

	elNode node = elf_get_node(fs,lhs);
	elFileLine line = node.line;

	elNodeId x = elf_emit_desugar_range_expr(fs,lhs,0);

	if (elf_pick_token(fs,TK_ASSIGN)) {
		elNodeId y = elf_load_file_expr(fs,0);
		elf_check_store(fs,tk.line,x,y);
		elf_emitter_emit_store(fs,tk.line,x,y);
	} else if (elf_pick_token(fs,TK_NIL_ASSIGN)) {
		elNodeId y = elf_load_file_expr(fs,0);
		elFileBoolExpr js = {0};
		/* todo: todo could we optimize this... */
		elf_emit_jump_if_not_nil(fs,tk.line,&js,x);
		elf_check_store(fs,tk.line,x,y);
		elf_emitter_emit_store(fs,tk.line,x,y);
		elf_emitter_patch_jumplist(fs,js.f);
	} else {
		/* the lhs of an assignment is an expression,
		and it is parsed by the expression parser.
		the expression parser checks whether there's
		an operator to make a binary expression however,
		if the operator is followed by '=' then it quits,
		so if we get here and we see an operator it's
		guaranteed to be a '{x}=' assignment. */
		if (elf_get_token_binding_priority(tk.type) > 0) {
			elNodeId x = elf_emit_desugar_range_expr(fs,lhs,0);
			/* op is tk */
			elToken op = elf_lexone(fs);
			elf_take_token(fs,TK_ASSIGN);
			/* todo: optimization! */
			elNodeId y = elf_load_file_expr(fs,0);
			y = elf_make_binary_node(fs,op.line,elf_token_to_node(op.type),elf_get_node_type(fs,x),x,y);

			elf_check_store(fs,fs->last_token.line,x,y);
			elf_emitter_emit_store(fs,op.line,x,y);
		} else {
			elf_emitter_local_load(fs,NO_LINE,0,NO_SLOT,0,lhs);
		}
		elf_set_memory_state(fs,mem);
	}

	elf_emit_desugar_range_expr_epilogue(fs,lhs);
	elASSERT(elf_get_memory_state(fs) == mem);
}


elNodeId elf_load_file_table(elFileState *fs) {
	elToken tk = fs->tk;
	elf_take_token(fs,TK_CURLY_LEFT);
	elNodeId *z = 0;
	elNodeId table = elf_make_table_node(fs,tk.line,0);
	int index = 0;
	while (!elf_term_token(fs,TK_CURLY_RIGHT)) {
		tk = fs->tk;
		elNodeId key;
		if (elf_test_token(fs,TK_WORD) && elf_test_then_token(fs,TK_ASSIGN)) {
			elf_lexone(fs);
			key = elf_make_string_node(fs,tk.line,tk.s);
		} else {
			key = elf_load_file_expr(fs,0);
		}
		if (elf_check_expr(fs,fs->tk.line,key)) {
			break;
		}
		if (elf_pick_token(fs,TK_ASSIGN)) {
			elNodeId val = elf_load_file_expr(fs,0);
			elNodeId fld = elf_make_field_node(fs,fs->lasttk.line,table,key);
			elNodeId f = elf_make_load_node(fs,fs->lasttk.line,fld,val);
			ARRAY_ADD(z,f);
		} else {
			elNodeId val = key;
			elNodeId ii = elf_make_integer_node(fs,fs->lasttk.line,index ++);
			elNodeId f = elf_make_load_node(fs,fs->lasttk.line,elf_make_index_node(fs,fs->lasttk.line,table,ii),val);
			ARRAY_ADD(z,f);
		}
		if (elf_pick_token(fs,TK_COMMA)) {
			continue;
		}
	}
	elf_take_token(fs,TK_CURLY_RIGHT);
	fs->nodes[table].z = z;
	return table;
}


elNodeId elf_load_file_expr(elFileState *fs, int flags) {
	switch (fs->this_token.type) {
		case TK_NONE:
		case TK_COMMA:
		case TK_PAREN_RIGHT:
		case TK_CURLY_RIGHT:
		case TK_SQUARE_RIGHT: {
			return NO_NODE;
		}
	}
	return elf_load_file_subexpr(fs,0,flags);
}

elNodeId elf_load_unary_expr(elFileState *fs, elBool allow_postfix, elBool flags) {
	elNodeId v = NO_NODE;
	elToken tk = fs->tk;
	switch (tk.type) {
		case TK_M_INDEX: case TK_M_ARRAY: case TK_M_VALUE: {
			elf_lexone(fs);
			/* todo: add support for this again, where you
			can specify which for loop you're reffering to. */
			#if 0
			elRegId target_value_register = NO_SLOT;
			if (!elf_term_eol_token(fs)) {
				elNodeId value = elf_load_file_expr(fs,0);
				if (value != NO_NODE) {
					target_value_register = elf_get_node_register(fs,MAKE_NODE_ID(value));
					if (target_value_register < 0) {
						elf_file_dialog(fs,elf_get_node_line(fs,value),"invalid value");
					}
				}
			}
			elFileBlock *bl = elf_emitter_get_loop_block(fs,target_value_register);
			elASSERT(bl != 0);
			elASSERT(bl->flags & BLOCK_LOOP);
			elRegId reg =
			tk.type == TK_M_ARRAY ? bl->loop.array_register :
			tk.type == TK_M_VALUE ? bl->loop.value_register : bl->loop.index_register;
			if (reg < 0) elf_file_dialog(fs,tk.line,"invalid loop for this");
			v = elf_make_register_node(fs,NO_LINE,reg);
			#endif
			// TODO: instead use regular register node but with
			// negative values?
			elRegId reg =
			tk.type == TK_M_ARRAY ? SPECIAL_REGISTER_ARRAY :
			tk.type == TK_M_VALUE ? SPECIAL_REGISTER_VALUE : SPECIAL_REGISTER_INDEX;
			v = elf_make_special_register_node(fs,tk.line,reg);
		} break;
		case TK_M_INT: case TK_M_NUM: { elf_lexone(fs);
			/* todo: make this an intrinsic instruction! */
			elNodeId x = elf_load_unary_expr(fs,1,0);
			char *name = tk.type == TK_M_INT ? "ntoi" : "iton";
			elNodeId fn = elf_get_global_entity_node(fs,tk.line,name);
			elNodeId *z = {0};
			ARRAY_ADD(z,x);
			v = elf_make_call_node(fs,tk.line,fn,z);
		} break;
		case TK_M_REGISTER: { elf_lexone(fs);
			tk = elf_take_token(fs,TK_WORD);
			elEntityId entity = elf_find_entity(fs,tk.line,tk.s,0);
			if (entity == NO_ENTITY) {
				elf_file_dialog(fs,tk.line,"'%s': invalid entity (must be a local)",tk.s);
			}
			v = elf_make_integer_node(fs,tk.line,fs->entities[entity].slot);
		} break;
		//
		// 'load' ( <file-name> )
		//
		//  leave elf.loadfile(<file-name>)
		//
		case TK_LOAD: {
			elf_lexone(fs);
			elNodeId *call_args = elf_load_call_args_or_expr(fs);

			elNodeId load_file_func = elf_get_global_entity_node(fs,tk.line,"elf.loadfile");
			elNodeId call_load_file = elf_make_call_node(fs,tk.line,load_file_func,call_args);
			v = call_load_file;
		} break;
		//
		// 'new' <meta-table> ( <argument-list> )
		//
		// leave elf.set_object_metatable({},Vector2):__new(x,y)
		//
		case TK_NEW: {
			elf_lexone(fs);
			elNodeId meta_table = elf_load_unary_expr(fs,0,1);
			elNodeId *call_args = elf_load_call_args(fs);

			elNodeId table;
			/* So if the user does something like new Thing {}
			or new Thing({}) the table that was passed in can
			be used as supposed to creating a new one */
			if ((ARRAY_LENGTH(call_args) == 1) && (elf_get_node_kind(fs,call_args[0]) == NODE_TABLE)) {
				table = call_args[0];
			} else {
				/* If the user however, doesn't do this, then we
				create a new table for him */
				table = elf_make_table_node(fs,tk.line,0);
			}

			table = elf_make_set_metatable_node(fs,tk.line,table,meta_table);

			elNodeId meta_field_name = elf_make_string_node(fs,tk.line,"__new");
			elNodeId get_meta_field = elf_make_metafield_node(fs,tk.line,table,meta_field_name);
			elNodeId call_new = elf_make_call_node(fs,tk.line,get_meta_field,call_args);

			v = call_new;
		} break;
		/* Empty ranges '..' are interpreted as 0..limit
		of whatever expression. */
		case TK_DOT_DOT: {
			elf_lexone(fs);
			v = elf_make_binary_node(fs,tk.line,NODE_RANGE,NT_ANY,NO_NODE,NO_NODE);
		} break;
		/* todo: this is temporary */
		case TK_DOT: case TK_ELF: {
			char dir[MAX_PATH] = {};
			if (elf_pick_token(fs,TK_ELF)) {
				strcat(dir,"elf");
				/* remind the user elf is a reserved keyword */
				if (!elf_test_token_inline(fs,TK_DOT)) {
					elf_file_dialog(fs,tk.line,"incomplete symbol, expected '.' on the same line as 'elf'. Did you mean to use 'elf'? This is a reserved keyword and it refers to the elf directory.");
				}
			}
			elf_take_token(fs,TK_DOT);
			do {
				elf_take_token_inline(fs,TK_WORD);
				strcat(dir,".");
				strcat(dir,fs->lasttk.s);
			} while (elf_pick_token_inline(fs,TK_DOT));

			elSymbolId x = elf_get_global_symbol(fs->M,elf_new_string(fs->R,dir));
			v = elf_make_global_value_node(fs,tk.line,x);
		} break;
		case TK_WORD: {
			elf_lexone(fs);
			v = elf_get_local_or_global_entity_node(fs,tk.line,tk.s,flags);
		} break;
		case TK_SUB: {
			elf_lexone(fs);
			v = elf_load_file_subexpr(fs,10000,flags);
			v = elf_make_binary_node(fs,tk.line,NODE_SUB,NT_INT,elf_make_integer_node(fs,tk.line,0),v);
		} break;
		case TK_ADD: {
			elf_lexone(fs);
			v = elf_load_file_subexpr(fs,10000,flags);
		} break;
		case TK_CURLY_LEFT: {
			v = elf_load_file_table(fs);
		} break;
		case TK_PAREN_LEFT: {
			elf_lexone(fs);
			v = elf_load_file_expr(fs,0);
			elf_take_token(fs,TK_PAREN_RIGHT);
			/* allow for empty () */
			if (v != NO_NODE) {
				v = elf_make_group_node(fs,tk.line,v);
			}
		} break;
		case TK_FUN: {
			v = elf_load_file_function(fs);
		} break;
		case TK_THIS: { elf_lexone(fs);
			v = elf_make_node_nullary(fs,tk.line,NODE_THIS,NT_ANY);
		} break;
		case TK_NIL: { elf_lexone(fs);
			v = elf_make_nil_node(fs,tk.line);
		} break;
		/* todo: maybe use proper boolean node? */
		case TK_TRUE: case TK_FALSE: { elf_lexone(fs);
			v = elf_make_integer_node(fs,tk.line,tk.type == TK_TRUE);
		} break;
		case TK_LETTER: case TK_INTEGER: { elf_lexone(fs);
			v = elf_make_integer_node(fs,tk.line,tk.i);
		} break;
		case TK_NUMBER: { elf_lexone(fs);
			v = elf_make_number_node(fs,tk.line,tk.n);
		} break;
		case TK_STRING: { elf_lexone(fs);
			v = elf_make_string_node(fs,tk.line,tk.s);
		} break;
		default: {
			elf_file_dialog(fs,tk.line,"'%s': unexpected token", elf_tkintel[tk.type].name);
			elf_debugger("test-break");
		} break;
	}

	/* termeol: solves some ambiguities,
	for instance:
	.RED
	.DrawCircle(...)
	If it wasn't for this it would think it was:
	.RED.DrawCircle(...) */
	if (!allow_postfix) goto leave;

	while (!elf_term_eol_token(fs)) {
		tk = fs->tk;

		switch (tk.type) {
			case TK_DOT: {
				elf_lexone(fs);
				// table.(x,y) -> (table.x, table.y)
				if (elf_pick_token(fs,TK_PAREN_LEFT)) {
					elNodeId *z = {0};
					do {
						elToken n = elf_take_token(fs,TK_WORD);
						elNodeId y = elf_make_string_node(fs,n.line,n.s);
						elNodeId x = elf_make_field_node(fs,tk.line,v,y);
						ARRAY_ADD(z,x);
					} while (elf_pick_token(fs,TK_COMMA));
					v = elf_make_multi_node(fs,tk.line,z);
					elf_take_token(fs,TK_PAREN_RIGHT);
				} else
				// table.{x,y}
				if (elf_pick_token(fs,TK_CURLY_LEFT)) {
					elNOCODE;
				} else {
					elToken field = elf_take_token(fs,TK_WORD);
					elNodeId field_node = elf_make_string_node(fs,field.line,field.s);
					v = elf_make_field_node(fs,tk.line,v,field_node);
				}
			} break;
			/* todo: make this nil safe, so [0,0] shouldn't
			fail if item at 0 is nil, should this be done
			here or at code generation?  */
			case TK_SQUARE_LEFT: {
				elf_take_token(fs,TK_SQUARE_LEFT);
				do {
					elNodeId index = elf_load_file_expr(fs,0);
					if (index == NO_NODE) break;
					/* registry[location.(y,x)] ->
					registry[location.y,location.x] */
					if (elf_get_node_kind(fs,index) == NODE_MULTI) {
						elNodeId *z = elf_get_node(fs,index).z;
						elf_xarray_foreachi(z) {
							v = elf_make_index_node(fs,tk.line,v,z[i]);
						}
					} else
					if (elf_get_node_kind(fs,index) == NODE_RANGE) {
						v = elf_make_ranged_index_node(fs,tk.line,v,index);
					} else v = elf_make_index_node(fs,tk.line,v,index);

					/* todo: this is silly, this is just an
					inner multi expressions, make multi
					expressions be regular 'comma' expressions
					instead */
				} while(elf_pick_token(fs,TK_COMMA));
				elf_take_token(fs,TK_SQUARE_RIGHT);
			} break;

			case TK_COLON: {
				elf_lexone(fs);
				elToken n = elf_take_token(fs,TK_WORD);
				elNodeId y = elf_make_string_node(fs,n.line,n.s);
				v = elf_make_metafield_node(fs,tk.line,v,y);
			} break;
			case TK_CURLY_LEFT:
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


void elf_load_file_stat(elFileState *fs) {
	elRegId mem = fs->fn->xmemory;
	elToken tk = fs->tk;
	elFileFnState *fn = fs->fn;
	elFileBlock *bl = elf_emitter_get_block(fs,-1); //fn->block;
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
			// elFileBlock bl = {0};
			elf_emitter_enter_delayed_block(fs,tk.line);//,&bl
			elf_load_file_stat(fs);
			elf_emitter_leave_delayed_block(fs,tk.line);//,&bl
			elASSERT(fs->fn->xmemory == mem);
		} break;
		case TK_IF: case TK_IFF: {
			elf_lexone(fs);
			elNodeId x = elf_load_file_expr(fs,0);
			elf_take_token(fs,TK_QUESTION_MARK);
			// elFileBlock block = {0};
			elf_emitter_begin_block(fs,0);//&block,
			elSelectState s = {0};
			elf_emitter_begin_if(fs,tk.line,&s,x,tk.type==TK_IFF?L_IFF:L_IF);
			elf_load_file_stat(fs);
			while (!elf_test_token(fs,TK_NONE)) {
				// elFileBlock block = {0};
				if (elf_pick_token(fs,TK_ELIF)) {
					elf_emitter_begin_block(fs,0);//&block,
					x = elf_load_file_expr(fs,0);
					elf_take_token(fs,TK_QUESTION_MARK);
					elf_emitter_add_elif_clause(fs,fs->lasttk.line,&s,x);
					elf_load_file_stat(fs);
					elf_emitter_close_block(fs);
				} else
				if (elf_pick_token(fs,TK_THEN)) {
					elf_emitter_begin_block(fs,0);//&block,
					elf_emitter_add_then_clause(fs,fs->lasttk.line,&s);
					elf_load_file_stat(fs);
					elf_emitter_close_block(fs);
				} else
				if (elf_pick_token(fs,TK_ELSE)) {
					elf_emitter_begin_block(fs,0);//&block,
					elf_emitter_add_else_clause(fs,fs->lasttk.line,&s);
					elf_load_file_stat(fs);
					elf_emitter_close_block(fs);
				} else break;
			}
			elf_emitter_close_if(fs,fs->lasttk.line,&s);
			elf_emitter_close_block(fs);
			elASSERT(fs->fn->xmemory == mem);
		} break;
		case TK_LET: {
			elf_lexone(fs);
			do {
				if (elf_test_token(fs,TK_LET)) {
					elf_file_dialog(fs,fs->last_token.line,"invalid declaration, expected next declarator's name after ',' instead got 'let'");
					elf_file_dialog(fs,fs->this_token.line,"invalid declaration, 'let' after comma");
					elf_throw(fs->R,0,"syntax error: invalid declaration");
				}
				elToken n = elf_take_token(fs,TK_WORD);
				elNodeId x = elf_new_local_entity(fs,n.line,n.s,0);
				elf_complete_stat(fs,x);
			} while (elf_pick_token(fs,TK_COMMA));
		} break;
		case TK_LEAVE: { elf_lexone(fs);
			elNodeId x = elf_load_file_expr(fs,0);
			elf_emit_yield(fs,tk.line,x);
			elASSERT(fs->fn->xmemory == mem);
		} break;
		case TK_BREAK: case TK_CONTINUE: { elf_lexone(fs);
			elRegId target_value_register = NO_SLOT;
			if (!elf_term_eol_token(fs)) {
				elNodeId value = elf_load_file_expr(fs,0);
				if (value != NO_NODE) {
					target_value_register = elf_get_node_register(fs,MAKE_NODE_ID(value));
					if (target_value_register < 0) {
						elf_file_dialog(fs,elf_get_node_line(fs,value),"invalid value");
					}
				}
			}
			if (tk.type == TK_CONTINUE) elf_emit_continue(fs,tk.line,target_value_register);
			else elf_emit_break(fs,tk.line,target_value_register);
		} break;
		case TK_WHILE: { elf_lexone(fs);
			elf_emitter_begin_block(fs,BLOCK_LOOP);
			elNodeId x = elf_load_file_expr(fs,0);
			elf_take_token(fs,TK_QUESTION_MARK);
			elf_emitter_begin_while_loop(fs,tk.line,x);
			elf_load_file_stat(fs);
			elf_emitter_close_while_loop(fs,tk.line);
			elASSERT(fs->fn->xmemory == mem);
			elf_emitter_close_block(fs);
		} break;
		case TK_DO: { elf_lexone(fs);
			elf_emitter_begin_block(fs,BLOCK_LOOP);
			elf_emitter_begin_do_while_loop(fs,tk.line);
			elf_load_file_stat(fs);
			elf_take_token(fs,TK_WHILE);
			elNodeId x = elf_load_file_expr(fs,0);
			elf_emitter_close_do_while_loop(fs,tk.line,x);
			elASSERT(fs->fn->xmemory == mem);
			elf_emitter_close_block(fs);
		} break;
		/*

		The for loop syntax justification:

		The main idea is variable "projections".

		Not sure if I've coined this term, since
		don't read compiler literature.

		The idea is that "for" is another
		way to "project" a variable.

		"let" "projects" a variable so that
		it is one single value at that point.

		So to derive the "for" loop syntax we
		extend "let", to declare a variable
		that may take on multiple values.

		"for i = 0..1 ? { }".

		"for" "projects" a variable such that
		it takes on multiple values.

		Really the projection part is caused
		by the "0..1" operator.

		for i = 0 is virtually equivalent to
		let i = 0, the only difference is that
		it offers loop semantics.

		Additionally, the variable must be
		projected onto a block, because it
		takes on multiple values, the block
		must be evaluated multiple times.
		This simplifies many things...


		Now in practice we a variable must
		take on one value at any given point,
		hence the term "projection".

		unroll 0..N -> 0,1,2,3,..N
		for i = {x} ?
		for i = .. {x} ?
		for i = {} .. ?
		for i = {x} .. {x} ?
		for i = {x}.({x})
		for i = {x}.({x},{x})
		for i = {x}[{x}..{x}]
		for i = {x} , {x} ?
		for i = 0..1, 1 == for i = 0..,1

		todo: THIS HAS TO BE REWORKED!
		*/
		case TK_FOR: {
			elf_lexone(fs);
			elToken name = elf_take_token(fs,TK_WORD);

			elf_take_token(fs,TK_ASSIGN);

			elNodeId *z = 0;
			do {
				elNodeId y = elf_load_file_expr(fs,0);
				if (elf_get_node_kind(fs,y) == NODE_MULTI) {
					elNodeId *yz = elf_get_node(fs,y).z;
					for (int i = 0; i < ARRAY_LENGTH(yz); i += 1) {
						ARRAY_ADD(z,yz[i]);
					}
				} else {
					ARRAY_ADD(z,y);
				}
			} while (elf_pick_token(fs,TK_COMMA));

			elf_take_token(fs,TK_QUESTION_MARK);

			elByteId block_head = NO_BYTE;
			elByteId block_tail = NO_BYTE;

			/* todo: why not create this per
			expression instead */
			elBlockId block = elf_emitter_begin_block(fs,BLOCK_LOOP);

			/* the value here refers to the thing that
			gets assigned to whatever we're iterating
			over, so for instance, for i = 0..24, here
			'i' is both the value and the index.
			But in principle, index is always the current
			loop iteration we're on.
			However, because loops can expand to multiple
			loops, the value register may alternate between
			index and actual value.
			For instance, for i = array[0..24], here the
			index register is created and the value register
			is used to store the array item. */
			const elNodeId value = elf_new_local_entity(fs,name.line,name.s,ENTITY_REFERENCED|ENTITY_ASSIGNED|ENTITY_FORLOOP);
			const elRegId value_register = elf_get_node(fs,value).x;


			/* todo: we should always have an index
			register, for instance:
			for i = 0..1,24,45 ? {
				// #index Should be 0,1,2
				// #value Should be 0,24,25
			}
			*/
			int i;
			for (i = 0; i < ARRAY_LENGTH(z); i += 1) {
				elNodeId y = z[i];

				elNodeId array = NO_NODE;
				elRegId array_register = NO_SLOT;

				elNodeId index = NO_NODE;
				elRegId index_register = NO_SLOT;

				if (elf_get_node_kind(fs,y) == NODE_RANGE_INDEX) {
					array = elf_get_node(fs,y).x;
					array_register = elf_emitter_localize(fs,NO_LINE,array);

					y = elf_get_node(fs,y).y;
					elASSERT(elf_get_node_kind(fs,y) == NODE_RANGE);
				}
				if (elf_get_node_kind(fs,y) == NODE_RANGE) {
					elNodeId lo = elf_get_node(fs,y).x;
					elNodeId hi = elf_get_node(fs,y).y;

					if (array == NO_NODE) {
						index = value;
						// for .. ? { }
						if (lo == NO_NODE) lo = elf_make_integer_node(fs,tk.line, 0);
						if (hi == NO_NODE) hi = elf_make_integer_node(fs,tk.line,-1);
					} else {
						// for array[..] ? { }
						if (lo == NO_NODE) lo = elf_make_integer_node(fs,tk.line,0);
						if (hi == NO_NODE) hi = elf_make_call_metafield_node(fs,tk.line,array,0,"length");

						/* todo: we're allocating this here, and never freeing it! */
						index = elf_make_register_node(fs,tk.line,elf_emitter_local_alloc(fs,tk.line,NO_SLOT,NO_NODE));
					}

					elf_emitter_begin_ranged_loop(fs,tk.line,index,lo,hi);

					elf_emitter_get_block(fs,block)->loop.value_register = value_register;
					if (array != NO_NODE) {
						elf_emitter_get_block(fs,block)->loop.array_register = array_register;
					}

					if (array != NO_NODE) {
						elNodeId *z = {0};
						ARRAY_ADD(z,index);

						/* todo: make this neater */
						elf_emitter_emit_store(fs,name.line,value,
						elf_make_call_metafield_node(fs,tk.line,array,z,"idx"));
						// elf_make_call_node(fs,tk.line,
						// elf_make_metafield_node(fs,tk.line,array,
						// elf_make_string_node(fs,tk.line,"idx")),z));
					}

						/* todo: could be neater */
					if (i == 0) {
						block_head = elf_get_last_byteid(fs);
						elf_load_file_stat(fs);
						block_tail = elf_get_last_byteid(fs);
					} else {
						for (int j = block_head; j < block_tail; j += 1) {
							elf_emitter_add_byte(fs,
							elf_emitter_get_line(fs,j),
							elf_emitter_get_byte(fs,j));
						}
					}

					elf_emitter_close_ranged_loop(fs,tk.line);
				} else {
					elf_emitter_emit_store(fs,tk.line,value,y);


					if (i == 0) {
						block_head = elf_get_last_byteid(fs);
						elf_load_file_stat(fs);
						block_tail = elf_get_last_byteid(fs);
					} else {
						for (int j = block_head; j < block_tail; j += 1) {
							elf_emitter_add_byte(fs,
							elf_emitter_get_line(fs,j),
							elf_emitter_get_byte(fs,j));
						}
					}

						/* because we are within a loop
						the user can use "continue" and
						"break", continues are the ones
						we need to handle here, which
						mean move on to the next step. */
					elFileBlock *loop = elf_emitter_get_block(fs,block);

					elf_emitter_patch_jumplist(fs,loop->loop.true_jumps);
					elf_xarray_delete(loop->loop.true_jumps);
					loop->loop.true_jumps = 0;
				}
			}
			elf_emitter_close_block(fs);
		} break;
		case TK_CURLY_LEFT: { elf_lexone(fs);
			// elFileBlock block = {0};
			elf_emitter_begin_block(fs,0);//&block,
			while (!elf_term_token(fs,TK_CURLY_RIGHT)) {
				elf_load_file_stat(fs);
			}
			elf_take_token(fs,TK_CURLY_RIGHT);
			elf_emitter_close_block(fs);
		} break;
		default: {

			elNodeId x = elf_load_file_expr(fs,1);
			elf_complete_stat(fs,x);

			elASSERT(fs->fn->xmemory == mem);
			if (x == NO_NODE) {
				elf_file_dialog(fs,tk.line,"invalid statement");
			}
			/* todo: better error handling */
			elASSERT(x != NO_NODE);
		} break;
	}
}

