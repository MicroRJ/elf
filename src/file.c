/*
** See Copyright Notice In elf.h
** file.c
** Parsing And Code Generation...
*/



void elf_fclose_function(elFileState *fs);


void elf_fs_close_file(elFileState *fs) {
	elf_fclose_function(fs);
}


int elf_fs_begin_file(elFileState *fs, char *filename, char *filetext) {
	if ((filename == 0) || (filetext == 0)) {
		return -1;
	}
	fs->filename   = filename;
	fs->filetext   = filetext;
	fs->thischar   = filetext;
	fs->linechar   = filetext;
	fs->linenumber = 1;
	fs->default_register = NO_SLOT;

	/* kick start by lexing the first two tokens */
	elf_poll_token(fs);
	elf_poll_token(fs);

	elf_begin_function(fs,&fs->state,fs->this_token.line);
	return 1;
}



char *elf_fget_name(elFileState *fs) {
	return fs->filename;
}


elBool elf_check_expr(elFileState *fs, elFileline line, elNodeId id) {
	if (id != NO_NODE) return 0;
	elf_fdialog(fs,line,"invalid expression");
	return 1;
}


/* test the current token if on the same line as the previous
token */
elBool elf_test_token_inline(elFileState *fs, elTokenType k) {
	return fs->this_token.type == k && fs->last_token.eol != 1;
}


elBool elf_test_token(elFileState *fs, elTokenType k) {
	return fs->this_token.type == k;
}

elBool elf_ftestthentok(elFileState *fs, elTokenType k) {
	return fs->thentk.type == k;
}


/*
** Returns 1 whether there are no more tokens
** or whether the current token is a match.
*/
elBool elf_term_token(elFileState *fs, elTokenType k) {
	return fs->this_token.type == TK_NONE || fs->this_token.type == k;
}


elBool elf_term_eol_token(elFileState *fs) {
	return fs->this_token.type == TK_NONE || fs->last_token.eol == 1;
}


/*
** Consumes the current token only if it is a match,
** returning whether it was a match or not.
*/
elBool elf_pick_token(elFileState *fs, elTokenType k) {
	return elf_test_token(fs,k) && (elf_poll_token(fs), 1);
}


elBool elf_fpicktokinl(elFileState *fs, elTokenType k) {
	return elf_test_token_inline(fs,k) && (elf_poll_token(fs), 1);
}


elBool elf_choose_token(elFileState *fs, elTokenType x, elTokenType y) {
	return (elf_test_token(fs,x) || elf_test_token(fs,y)) && (elf_poll_token(fs), 1);
}


elToken elf_take_token(elFileState *fs, int k) {
	elToken tk = fs->tk;
	if (!elf_pick_token(fs,k)) {
		elf_fdialog(fs,fs->this_token.line,"expected '%s'\n",elf_token_intel[k].name);
	}
	return tk;
}


elToken elf_fgettokinl(elFileState *fs, int k) {
	elToken tk = fs->tk;
	if (!elf_fpicktokinl(fs,k)) {
		elf_fdialog(fs,fs->this_token.line,"expected '%s'\n",elf_token_intel[k].name);
	}
	return tk;
}


/* looks for an enclosed entity within the function,
and returns the index where the entity, the index
can then be used to emit instructions.
*/
int elf_fgetclosevalue(elFileFnState *fn, elEntityIdTypeGuard id) {
	FOR_ARRAY(i,fn->enclosure) {
		if (fn->enclosure[i] == id.id) {
			return i;
		}
	}
	return NO_SLOT;
}


/* encloses an entity within the given function. */
void elf_fencloseentity(elFileState *fs, elFileFnState *fn, elEntityIdTypeGuard id) {
	/* ensure the entity should actually be captured */
	elASSERT(id.id < fn->entities);
	/* check whether the entity was already captured */
	FOR_ARRAY(i,fn->enclosure) {
		if (fn->enclosure[i] == id.id) return;
	}
	ARRAY_ADD(fn->enclosure,id.id);
}


/* find the last declared entity for the given register
within the current function only! */
int elf_fgetentityforreg(elFileState *fs, int reg) {
	int id;
	for (id = fs->nentities-1; id >= fs->fn->entities; -- id) {
		if (fs->entities[id].reg == reg) {
			return id;
		}
	}
	return -1;
}


/* finds the last declared entity with the given
name. the id is absolute. */
elEntityId elf_fgetentity(elFileState *fs, elFileline line, char *name, elBool enclose) {
	elFileFnState *fn = fs->fn;
	/* we're using a linear search method here, but I can guarantee
	that this is actually fast enough, introducing table will not
	be profitable for a long while... */
	elEntityId id;
	for (id = fs->nentities-1; id > -1; -- id) {
		if (elf_texteq(fs->entities[id].name,name)) {
			if ((id < fn->entities) && (enclose)) {
				elf_fencloseentity(fs,fn,(elEntityIdTypeGuard){id});
			}
			fs->entities[id].flags |= ENTITY_REFERENCED;
			return id;
		}
	}
	return NO_ENTITY;
}


/* register a local entity within the current function and level,
if it shadows or redeclared some other entity, issue warnings,
a valid entity id is always returned regardless. Pass in the
register... */
elNodeId elf_new_entity(elFileState *fs, elFileline line, elBool flags, char *name, int reg) {
	elFileFnState *fn = fs->fn;
	elEntityId already;
	elFileEntity entity;

	already = elf_fgetentity(fs,line,name,0);
	if (already != NO_ENTITY) {
		entity = fs->entities[already];
		if (entity.kind == ENTITY_DIRECTORY)  {
			elf_fdialog(fs,line,"'%s': name is reserved for symbol directory",name);
		}
		if (entity.level == fs->nblocks) {
			elf_fdialog(fs,line,"'%s': is already declared",name);
		} else {
			/* only issue this warning if the entity we found
			is within this function... */
			if (already >= fn->entities) {
				elf_fdialog(fs,line,"'%s': this declaration shadows another one",name);
			}
		}
	}

	elEntityId id = fs->nentities ++;
	ARRAY_GROW(fs->entities,fs->nentities-ARRAY_LENGTH(fs->entities));

	fs->entities[id].kind  = ENTITY_LOCAL;
	fs->entities[id].line  = line;
	fs->entities[id].name  = name;
	fs->entities[id].level = fs->nblocks; // this should be nblocks-1, it doesn't really matter...
	fs->entities[id].flags = flags;
	fs->entities[id].reg   = reg;
	return id;
}


elNodeId elf_fs_get_global(elFileState *fs, elFileline line, char *name) {
	elSymbolId x = elf_get_global_symbol(fs->M,elf_new_string(fs->R,name));
	elASSERT(x != -1);
	return elf_fnodeglobal(fs,line,x);
}


elNodeId elf_fgetname(elFileState *F, elFileline line, int flags, char *name) {
	elEntityId id = elf_fgetentity(F,line,name,1);
	if (id != NO_ENTITY) {
		if (flags) {
			if (~F->entities[id].flags & ENTITY_ASSIGNED) {
				elf_fdialog(F,line,"warning: usage of possibly unassigned variable");
			}
		}
		elFileFnState *fn = F->fn;
		if (id < fn->entities) {
			if (id < fn->enclosing->entities) {
				elf_fdialog(F,line,"too many layers for caching");
			}
			return elf_nclosevalue(F,line,elf_fgetclosevalue(fn,ENTITY_ID(id)));
		} else {
			return elf_node_local(F,line,F->entities[id].reg);
		}
	} else {
		/* todo: check whether we've assigned a value
		to this entity already, otherwise issue a warning that
		we're using something that hasn't got a value yet... */
		return elf_fs_get_global(F,line,name);
	}
}


void elf_begin_function(elFileState *fs, elFileFnState *fn, char *line) {
	fn->enclosing = fs->fn;
	fn->entities = fs->nentities;
	fn->bytes = fs->M->nbytes;
	fn->line = line;
	fn->yj = 0;
	/* todo: "begin_block" requires fn to be set
	for xmemory, can xmemory simply be in the
	file state instead? */
	fs->fn = fn;
	fn->entry_block = elf_begin_block(fs,0);

	int flags=ENTITY_PARAMETER|ENTITY_ASSIGNED|ENTITY_CONSTANT|ENTITY_REFERENCED;
	elf_new_entity(fs, line, flags, "this", elf_fs_local_alloc(fs));
}


void elf_fclose_function(elFileState *fs) {
	elFileFnState *fn;
	char *line;

	fn = fs->fn;
	line = fs->this_token.line;
	elf_emit_patch_jumplist(fs,fn->yj);
	ARRAY_DELETE(fn->yj);
	fn->yj = 0;
	elf_emit_bytex(fs,line,BC_LEAVE,0);
	elf_close_block(fs);
	elASSERT(fn->entry_block == fs->nblocks);
	elASSERT(fs->nentities == fn->entities);
	fs->fn = fn->enclosing;
}


void elf_fcheckassign(elFileState *fs, elFileline line, elNodeId x, elNodeId y) {
	if (y < 0) {
		elf_fdialog(fs,line,"invalid statement, expected a value for assignment");
	}
	elASSERT(x > NO_SLOT);
	elASSERT(y > NO_SLOT);
	elNode node = elf_get_target_node(fs,MAKE_NODE_ID(x));

	if (!elf_is_node_lvalue(node.kind)) {
		elf_fdialog(fs,line,"invalid assignment to (%s)",node2s[node.kind]);
		elf_fail(fs->R,0,"syntax error: invalid assignment");
	}

	if (node.kind == NODE_LOCAL) {
		elEntityId id = elf_fgetentityforreg(fs,node.x);
		elASSERT(id != -1 && "internal error");

		if (fs->entities[id].flags & ENTITY_CONSTANT) {
			elf_fdialog(fs,line,"invalid assignment to constant entity");
			elf_fail(fs->R,0,"syntax error: invalid assignment to constant entity");
		}

		fs->entities[id].flags |= ENTITY_ASSIGNED;
	}
}


elNodeId elf_fnodecallpf(elFileState *fs, elFileline line, elNodeId *args) {
	elNodeId fn = elf_fs_get_global(fs,line,"elf.pf");
	return elf_node_call(fs,line,fn,args);
}


elNodeId elf_node_call_set_metatable(elFileState *fs, elFileline line, elNodeId object, elNodeId metatable) {
	elNodeId fn = elf_fs_get_global(fs,line,"elf.set_object_metatable");
	elNodeId *z = 0;
	ARRAY_ADD(z,object);
	ARRAY_ADD(z,metatable);
	return elf_node_call(fs,line,fn,z);
}


elNodeId elf_parse_table(elFileState *fs);

void elf_ffreenode(elFileState *fs, int id) {
	if (id==fs->nnodes-1) {
		fs->nnodes-=1;
		elf_fdialog(fs,0,"freed node");
	}
}

/* {x} | ( x { ... } ) | { <table-initializer-list> } */
elNodeId *elf_parse_cargs(elFileState *fs) {
	elNodeId x;
	elNodeId *z,*n;
	z=0;
	if (elf_test_token(fs,TK_CURLY_LEFT)) {
		x=elf_parse_table(fs);
		ARRAY_ADD(z,x);
	} else if (elf_pick_token(fs,TK_PAREN_LEFT)) {
		if (!elf_test_token(fs,TK_PAREN_RIGHT)) do {
			x=elf_parse_expr(fs,0,0);
			if (x!=NO_NODE) {
				if (elf_get_node_kind(fs,x)==NODE_MULTI) {
					n=elf_get_node(fs,x).z;
					FOR_ARRAY(i,n)ARRAY_ADD(z,n[i]);
				} else ARRAY_ADD(z,x);
			} else break;
		} while (elf_pick_token(fs,TK_COMMA));
		elf_take_token(fs,TK_PAREN_RIGHT);
	} else {
		x=elf_parse_expr(fs,0,0);
		if (x!=NO_NODE)ARRAY_ADD(z,x);
	}
	return z;
}


elTokenType elf_is_operator_token_contextually(elToken tk) {
	/* could be done in the lexer */
	if (tk.type != TK_WORD) return tk.type;
	if (!strcmp(tk.text,"and")) return TK_LOG_AND;
	if (!strcmp(tk.text,"or")) return TK_LOG_OR;
	if (!strcmp(tk.text,"is")) return TK_EQ;
	return TK_WORD;
}


int elf_get_token_prec(elTokenType type) {
	return elf_token_intel[type].prec;
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


elNodeId elf_parse_subexpr(elFileState *fs, elFileExpr *expr, int rank, int flags) {
	elNodeId x = elf_parse_unary(fs,expr,flags|EXPR_ALLOW_POSTFIX);

	if (x == NO_NODE) {
		return x;
	}
	for (;;) {
		elTokenType op = elf_is_operator_token_contextually(fs->this_token);
		int prio = elf_get_token_prec(op);
		/* auto breaks when not a binary operator */
		if (prio <= rank) break;
		// *= += ...
		if (fs->then_token.type==TK_ASSIGN)  {
			/* todo: check subexpression level
			to ensure the user knows this is
			not an expression, for instance,
			'(i = 1)' 'i = 1' is at level 1,
			if we're at level 0, the statement
			parser will handle this and turn it
			into a statement... */
			break;
		}

		elToken tk = elf_poll_token(fs);
		elNodeId y = elf_parse_subexpr(fs,0,prio,flags);
		if (y == NO_NODE) break;
		x = elf_nodexy(fs,tk.line,elf_token_to_node(op),NT_ANY,x,y);
	}
	return x;
}


/* Named functions aren't a thing for this
language... at least for now... */
elNodeId elf_parse_function(elFileState *fs) {
	elToken tk;
	int fj,arity;
	elFileFnState fn = {0};

	tk=elf_take_token(fs,TK_FUN);
	/* emit a jump instruction to skip this function's code */
	fj=elf_emit_jump(fs,tk.line,-1);
	elf_begin_function(fs,&fn,tk.line);
	arity=1;

	/* start loading parameter list, each parameter also
	becomes a local register */
	elf_take_token(fs,TK_PAREN_LEFT);
	if (!elf_test_token(fs,TK_PAREN_RIGHT)) do {
		elToken name;

		name=elf_take_token(fs,TK_WORD);
		elf_new_entity(fs, name.line, ENTITY_PARAMETER|ENTITY_ASSIGNED
		, name.text, elf_fs_local_alloc(fs));

		arity += 1;
	} while (elf_pick_token(fs,TK_COMMA));

	if (!elf_test_token(fs,TK_PAREN_RIGHT)) {
		elf_fdialog(fs,0,"did you miss a ','?");
	}
	elf_take_token(fs,TK_PAREN_RIGHT);

	/* '?' are now optional */
	elf_pick_token(fs,TK_QMARK);

	if (elf_test_token(fs,TK_CURLY_LEFT)) {
		elf_take_token(fs,TK_CURLY_LEFT);
		while (elf_parse_stat(fs)) {
			tk=fs->this_token;
		}
		/* By default we emit a 'leave this' instruction,
		because this tends to be more convenient... */
		/* todo: only emit the yield if this block
		wasn't terminated by another leave instruction,
		otherwise this is wasteful */
		elf_emit_yield(fs,tk.line,elf_node_local(fs,tk.line,0));
		elf_take_token(fs,TK_CURLY_RIGHT);
	} else {
		elf_emit_yield(fs,tk.line,elf_parse_expr(fs,0,0));
	}

	elf_fclose_function(fs);

	/* now finally patch the jump-over jump
	to resume control flow... */
	elf_patch_jump(fs,fj);


	/* create a new prototype and add this function
	to the list of prototypes */
	elFileProto fp = {0};
	/* todo: implement this */
	// fp.parent  = fn.enclosing
	fp.arity	  = arity;
	fp.nlocals = fn.nlocals;
	fp.nvalues = ARRAY_LENGTH(fn.enclosure);
	fp.bytes   = fn.bytes;
	fp.nbytes  = fs->M->nbytes - fn.bytes;

	int f = elf_add_proto(fs->M,fp);

	/* Now iterate over all the enclosed or captured
	entities, this are the locals that we accessed
	from outside our function, these are registers
	so we can create register nodes directly. */
	elNodeId *z = 0;
	FOR_ARRAY(i,fn.enclosure) {
		elFileEntity entity = fs->entities[fn.enclosure[i]];
		ARRAY_ADD(z,elf_node_local(fs,entity.line,entity.reg));
	}

	return elf_make_closure_node(fs,tk.line,f,z);
}


/*
** Completes the expression line, possibly a statement.
** Takes the lhs expression, if not followed by a
** statement operator, such as '=' then it simply
** emits code to evaluate the expression.
*/
void elf_fassignmaybe(elFileState *fs, elNodeId lhs) {
	if (lhs == NO_NODE) {
		return;
	}

	elToken tk = fs->tk;
	elRegId mem = elf_get_mem_state(fs);

	elNode node = elf_get_node(fs,lhs);
	elFileline line = node.line;

	elNodeId x = elf_fdesugarrangeexpr(fs,lhs,0);

	if (elf_pick_token(fs,TK_ASSIGN)) {
		elNodeId y;
		// elRegId xmemory = elf_get_mem_state(fs);
		// if (elf_test_token(fs,TK_DEFAULT)) {
		// 	y = elf_parse_default_expr(fs);
		// } else {
		elFileExpr expr = {0};
		y = elf_parse_expr(fs,&expr,0);
		// }
		elf_fcheckassign(fs,tk.line,x,y);
		elf_emitstore(fs,tk.line,x,y);
		// elf_set_mem_state(fs,xmemory);
	} else if (elf_pick_token(fs,TK_NIL_ASSIGN)) {
		elFileExpr expr = {0};
		elNodeId y = elf_parse_expr(fs,&expr,0);
		elFileExpr js = {0};
		/* todo: todo could we optimize this... */
		elf_emitjumpifnonil(fs,tk.line,&js,x);
		elf_fcheckassign(fs,tk.line,x,y);
		elf_emitstore(fs,tk.line,x,y);
		elf_emit_patch_jumplist(fs,js.f);
	} else {
		/* the lhs of an assignment is an expression,
		and it is parsed by the expression parser.
		the expression parser checks whether there's
		an operator to make a binary expression however,
		if the operator is followed by '=' then it quits,
		so if we get here and we see an operator it's
		guaranteed to be a '{x}=' assignment. */
		if (elf_get_token_prec(tk.type) > 0) {
			elNodeId x = elf_fdesugarrangeexpr(fs,lhs,0);
			/* op is tk */
			elToken op = elf_poll_token(fs);
			elf_take_token(fs,TK_ASSIGN);
			/* todo: optimization! */
			elFileExpr expr = {0};
			elNodeId y = elf_parse_expr(fs,&expr,0);

			y = elf_nodexy(fs,op.line,elf_token_to_node(op.type),elf_get_node_type(fs,x),x,y);

			elf_fcheckassign(fs,fs->last_token.line,x,y);
			elf_emitstore(fs,op.line,x,y);
		} else {
			elf_emittereval(fs,0,-1,0,lhs);
		}
		elf_set_mem_state(fs,mem);
	}

	elf_fdesugarrangeexprepilogue(fs,lhs);
	elASSERT(elf_get_mem_state(fs) == mem);
}


elNodeId elf_parse_table(elFileState *fs) {
	elNodeId *args,table,key,field,store,value;
	elToken token;
	int index;

	token=elf_take_token(fs,TK_CURLY_LEFT);
	table=elf_node_newtable(fs,token.line,0);
	args=0;
	index=0;
	token=fs->this_token;

	for (;(token.type!=TK_NONE)&&(token.type!=TK_CURLY_RIGHT);token=fs->this_token) {
		value=NO_NODE;
		if ((token.type==TK_WORD)&&(fs->then_token.type==TK_ASSIGN)) {
			token=elf_poll_token(fs);
			key=elf_node_string(fs,token.line,token.text);
		} else {
			key=value=elf_parse_expr(fs,0,0);
		}
		token=fs->this_token;
		if (elf_pick_token(fs,TK_ASSIGN)) {
			value=elf_parse_expr(fs,0,0);
		} else {
			key=elf_node_integer(fs,token.line,index++);
		}

		elf_check_expr(fs,token.line,key);
		elf_check_expr(fs,token.line,value);
		token=fs->this_token;
		field=elf_node_field(fs,token.line,table,key);
		store=elf_node_store(fs,token.line,field,value);
		ARRAY_ADD(args,store);
		if (elf_pick_token(fs,TK_COMMA)) {
			continue;
		}
	}
	elf_take_token(fs,TK_CURLY_RIGHT);
	fs->nodes[table].z = args;
	return table;
}


elNodeId elf_parse_expr(elFileState *F, elFileExpr *expr, int flags) {
	/* todo: I think it's easier to check whether the token is an expression */
	switch (F->this_token.type) {
		case TK_NONE:
		case TK_FOR: case TK_WHILE: case TK_LASTLY:
		case TK_COMMA:
		case TK_PAREN_RIGHT: case TK_CURLY_RIGHT: case TK_SQUARE_RIGHT: {
			return NO_NODE;
		}
	}
	return elf_parse_subexpr(F,expr,0,flags);
}


/*  */
elRegId elf_get_loop_register(elFileState *F, elFileline line, int type) {
	// todo: re-add support for specifying which
	// loop you're referring to
	elFileBlock *bl = elf_get_loop_block(F,-1);
	elASSERT(bl != 0);
	elASSERT(bl->flags & BLOCK_LOOP);
	elRegId reg = NO_SLOT;
	switch (type) {
		case SPECIAL_REGISTER_ARRAY: {
			reg = bl->loop.array_register;
		} break;
		case SPECIAL_REGISTER_VALUE: {
			reg = bl->loop.value_register;
		} break;
		case SPECIAL_REGISTER_INDEX: {
			reg = bl->loop.index_register;
		} break;
		default: elNOCODE;
	}
	if (reg == NO_SLOT) {
		elf_fdialog(F,line,"invalid context for loop register macro");
		elf_fail(F->R,0,"syntax error: invalid context for loop register macro");
	}
	return reg;
}


/* todo: add support for:
specifing which for loop you're reffering to. */
#if 0
elRegId target_value_register = NO_SLOT;
if (!elf_term_eol_token(fs)) {
	elNodeId value = elf_parse_expr(fs,0);
	if (value != NO_NODE) {
		target_value_register = elf_get_node_register(fs,MAKE_NODE_ID(value));
		if (target_value_register < 0) {
			elf_fdialog(fs,elf_get_node_line(fs,value),"invalid value");
		}
	}
}
#endif


elNodeId elf_parse_unary(elFileState *fs, elFileExpr *expr, elBool flags) {
	elNodeId v = NO_NODE;
	elToken tk = fs->this_token;
	elNodeId x;
	switch (tk.type) {
		case TK_M_INDEX: case TK_M_ARRAY: case TK_M_VALUE: {
			elf_poll_token(fs);
			elRegId reg =
			tk.type == TK_M_ARRAY ? SPECIAL_REGISTER_ARRAY :
			tk.type == TK_M_VALUE ? SPECIAL_REGISTER_VALUE : SPECIAL_REGISTER_INDEX;
			v = elf_node_local(fs,tk.line,reg);
		} break;
		/* todo: make this an intrinsic instruction! */
		case TK_M_INT: case TK_M_NUM: { elf_poll_token(fs);
			char *name;
			elNodeId fn, *z=0;
			x=elf_parse_unary(fs,0,flags|EXPR_ALLOW_POSTFIX);
			name=tk.type==TK_M_INT?"ntoi":"iton";
			fn=elf_fs_get_global(fs,tk.line,name);
			ARRAY_ADD(z,x);
			v=elf_node_call(fs,tk.line,fn,z);
		} break;
		case TK_M_REGISTER: {
			elf_poll_token(fs);
			tk=elf_take_token(fs,TK_WORD);
			x=elf_fgetentity(fs,tk.line,tk.text,0);
			if (x!=NO_ENTITY) {
				x=elf_node_integer(fs,tk.line,fs->entities[x].reg);
			} else elf_fdialog(fs,tk.line,"'%s': invalid entity (must be a local)",tk.text);
		} break;
		//
		// 'load' ( <file-name> )
		//
		//  elf.loadfile(<file-name>)
		//
		case TK_LOAD: {
			elf_poll_token(fs);
			elNodeId *call_args = elf_parse_cargs(fs);

			elNodeId load_file_func = elf_fs_get_global(fs,tk.line,"elf.loadfile");
			elNodeId call_load_file = elf_node_call(fs,tk.line,load_file_func,call_args);
			v = call_load_file;
		} break;
		//
		// 'new' <meta-table> ( <argument-list> )
		//
		// leave elf.set_object_metatable({},Vector2):__new(x,y)
		//
		case TK_NEW: {
			elf_poll_token(fs);
			elNodeId meta_field_name, get_meta_field, call_new
			, meta_table, *call_args, table;

			meta_table=elf_parse_unary(fs,0,0);
			call_args=elf_parse_cargs(fs);

			/* so if the user does something like new Thing {}
			or new Thing({}) the table that was passed in can
			be used as supposed to creating a new one */
			if ((ARRAY_LENGTH(call_args) == 1) && (elf_get_node_kind(fs,call_args[0]) == NODE_TABLE)) {
				table = call_args[0];
			} else {
				/* If the user however, doesn't do this, then we
				create a new table for him */
				table = elf_node_newtable(fs,tk.line,0);
			}

			table=elf_node_call_set_metatable(fs,tk.line,table,meta_table);

			meta_field_name=elf_node_string(fs,tk.line,"__new");
			get_meta_field=elf_node_metafield(fs,tk.line,table,meta_field_name);
			call_new=elf_node_call(fs,tk.line,get_meta_field,call_args);

			v = call_new;
		} break;
		/* empty ranges '..' are interpreted as 0..limit
		of whatever expression. */
		case TK_DOT_DOT: {
			elf_poll_token(fs);
			v = elf_nodexy(fs,tk.line,NODE_RANGE,NT_ANY,NO_NODE,NO_NODE);
		} break;
		/* todo: this is temporary */
		case TK_DOT: case TK_ELF: {
			char dir[MAX_PATH] = {};
			if (elf_pick_token(fs,TK_ELF)) {
				strcat(dir,"elf");
				/* remind the user elf is a reserved keyword */
				if (!elf_test_token_inline(fs,TK_DOT)) {
					elf_fdialog(fs,tk.line,"incomplete symbol, expected '.' on the same line as 'elf'. Did you mean to use 'elf'? This is a reserved keyword and it refers to the elf directory.");
				}
			}

			elf_take_token(fs,TK_DOT);
			do {
				elf_fgettokinl(fs,TK_WORD);
				strcat(dir,".");
				strcat(dir,fs->last_token.text);
			} while (elf_fpicktokinl(fs,TK_DOT));
			int x;
			x=elf_get_global_symbol(fs->M,elf_new_string(fs->R,dir));
			v=elf_fnodeglobal(fs,tk.line,x);
		} break;
		case TK_WORD: {
			elf_poll_token(fs);
			v = elf_fgetname(fs,tk.line,flags,tk.text);
		} break;
		case TK_SUB: {
			elf_poll_token(fs);
			v = elf_parse_subexpr(fs,0,10000,flags);
			v = elf_nodexy(fs,tk.line,NODE_SUB,NT_INT,elf_node_integer(fs,tk.line,0),v);
		} break;
		case TK_ADD: {
			elf_poll_token(fs);
			v = elf_parse_subexpr(fs,0,10000,flags);
		} break;
		case TK_CURLY_LEFT: {
			v = elf_parse_table(fs);
		} break;
		case TK_PAREN_LEFT: {
			elf_poll_token(fs);
			v = elf_parse_expr(fs,0,EXPR_ALLOW_POSTFIX);
			elf_take_token(fs,TK_PAREN_RIGHT);
			/* allow for empty () */
			if (v != NO_NODE) {
				v = elf_make_group_node(fs,tk.line,v);
				elASSERT(v != NO_NODE);
			}
		} break;
		case TK_FUN: {
			v = elf_parse_function(fs);
		} break;
		case TK_DEFAULT: {
			elf_fdialog(fs,tk.line,"syntax error: default expressions can only be top level");
			elf_fail(fs->R,0,"syntax error: default expressions can only be top level");
		} break;
		case TK_NIL: {
			elf_poll_token(fs);
			v = elf_nnil(fs,tk.line);
		} break;
		case TK_TRUE: case TK_FALSE: {
			elf_poll_token(fs);
			v=elf_node_integer(fs,tk.line,tk.type==TK_TRUE);
		} break;
		case TK_LETTER: case TK_INTEGER: {
			elf_poll_token(fs);
			v=elf_node_integer(fs,tk.line,tk.integer);
		} break;
		case TK_NUMBER: {
			elf_poll_token(fs);
			v=elf_nnumber(fs,tk.line,tk.number);
		} break;
		case TK_STRING: {
			elf_poll_token(fs);
			v=elf_node_string(fs,tk.line,tk.text);
		} break;
		default: {
			elf_fdialog(fs,tk.line,"'%s': unexpected token", elf_token_intel[tk.type].name);
			elf_fail(fs->R,0,"syntax error: unexpected token");
		} break;
	}

	if (~flags & EXPR_ALLOW_POSTFIX) {
		goto esc;
	}

	/* ensure we don't parse a postfix past a line */
	while (!elf_term_eol_token(fs)) {
		tk = fs->tk;

		switch (tk.type) {
			case TK_DOT: {
				elf_poll_token(fs);
				// table.(x,y) -> (table.x, table.y)
				if (elf_pick_token(fs,TK_PAREN_LEFT)) {
					elNodeId *z = {0};
					do {
						elToken n = elf_take_token(fs,TK_WORD);
						elNodeId x,y;
						y=elf_node_string(fs,n.line,n.text);
						x=elf_node_field(fs,tk.line,v,y);
						ARRAY_ADD(z,x);
					} while (elf_pick_token(fs,TK_COMMA));
					v = elf_nmulti(fs,tk.line,z);
					elf_take_token(fs,TK_PAREN_RIGHT);
				} else
				// table.{x,y}
				if (elf_pick_token(fs,TK_CURLY_LEFT)) {
					elNOCODE;
				} else {
					elToken name;
					elNodeId field;
					name=elf_take_token(fs,TK_WORD);
					field=elf_node_string(fs,name.line,name.text);
					v=elf_node_field(fs,tk.line,v,field);
				}
			} break;
			/* todo: make this nil safe, so [0,0] shouldn't
			fail if item at 0 is nil, should this be done
			here or at code generation?  */
			case TK_SQUARE_LEFT: {
				elf_take_token(fs,TK_SQUARE_LEFT);
				do {
					elFileExpr expr = {0};
					elNodeId index = elf_parse_expr(fs,&expr,0);

					if (index == NO_NODE) break;
					/* registry[location.(y,x)] ->
					registry[location.y,location.x] */
					if (elf_get_node_kind(fs,index) == NODE_MULTI) {
						elNodeId *z = elf_get_node(fs,index).z;
						FOR_ARRAY(i,z) {
							v = elf_node_index(fs,tk.line,v,z[i]);
						}
					} else
					if (elf_get_node_kind(fs,index) == NODE_RANGE) {
						v = elf_make_ranged_index_node(fs,tk.line,v,index);
					} else v = elf_node_index(fs,tk.line,v,index);

					/* todo: this is silly, this is just an
					inner multi expressions, make multi
					expressions be regular 'comma' expressions
					instead */
				} while(elf_pick_token(fs,TK_COMMA));
				elf_take_token(fs,TK_SQUARE_RIGHT);
			} break;

			case TK_COLON: {
				elToken n;
				elNodeId y;
				elf_poll_token(fs);
				n=elf_take_token(fs,TK_WORD);
				y=elf_node_string(fs,n.line,n.text);
				v=elf_node_metafield(fs,tk.line,v,y);
			} break;
			case TK_CURLY_LEFT:
			case TK_PAREN_LEFT: {
				elNodeId *z = elf_parse_cargs(fs);
				v = elf_node_call(fs,tk.line,v,z);
			} break;
			default: goto esc;
		}
	}

	esc:
	return v;
}


int elf_parse_stat(elFileState *fs) {
	int mem;
	elToken tk;
	elFileBlock *bl;
	elNodeId x;


	bl=elf_get_block(fs,-1);
	mem=elf_get_mem_state(fs);
	tk=fs->tk;

	switch (tk.type) {
		case TK_NONE: case TK_CURLY_RIGHT:
		case TK_THEN: case TK_ELSE: case TK_ELIF: {
			return 0;
		}
	}
	if (bl->flags & BLOCK_ENDED) {
		elf_fdialog(fs,tk.line,"warning: unreachable statement");
	}
	switch (tk.type) {
		case TK_LASTLY: case TK_FINALLY: {
			elf_poll_token(fs);
			if (tk.type == TK_FINALLY) {
				elf_fdialog(fs,tk.line,"warning: please consider using 'lastly' instead, 'finally' could change semantics in the future");
			}
			elf_begin_lastly_block(fs,tk.line);
			elf_parse_stat(fs);
			elf_close_lastly_block(fs,tk.line);
			elASSERT(elf_get_mem_state(fs)==mem);
		} break;
		case TK_IF: case TK_IFF: {
			elSelectState s = {0};
			elf_poll_token(fs);
			x=elf_parse_expr(fs,0,0);
			elf_take_token(fs,TK_QMARK);
			elf_begin_block(fs,0);
			elf_fbeginif(fs,tk.line,&s,x,tk.type==TK_IFF?L_IFF:L_IF);
			elf_parse_stat(fs);
			while (!elf_test_token(fs,TK_NONE)) {
				if (elf_pick_token(fs,TK_ELIF)) {
					elf_begin_block(fs,0);
					x=elf_parse_expr(fs,0,0);
					elf_take_token(fs,TK_QMARK);
					elf_fifaddelifclause(fs,fs->last_token.line,&s,x);
					elf_parse_stat(fs);
					elf_close_block(fs);
				} else if (elf_pick_token(fs,TK_THEN)) {
					elf_begin_block(fs,0);
					elf_fifaddthenclause(fs,fs->last_token.line,&s);
					elf_parse_stat(fs);
					elf_close_block(fs);
				} else if (elf_pick_token(fs,TK_ELSE)) {
					elf_begin_block(fs,0);
					elf_fifaddelseclause(fs,fs->last_token.line,&s);
					elf_parse_stat(fs);
					elf_close_block(fs);
				} else break;
			}
			elf_fcloseif(fs,fs->last_token.line,&s);
			elf_close_block(fs);
			elASSERT(elf_get_mem_state(fs)==mem);
		} break;
		case TK_LET: {
			elf_poll_token(fs);
			do {
				if (elf_test_token(fs,TK_LET)) {
					elf_fdialog(fs,fs->last_token.line,"invalid declaration, expected next declarator's name after ',' instead got 'let'");
					elf_fdialog(fs,fs->this_token.line,"invalid declaration, 'let' after comma");
					elf_fail(fs->R,0,"syntax error: invalid declaration");
				}

				elToken name;
				int reg;

				name=elf_take_token(fs,TK_WORD);
				reg=elf_fs_local_alloc(fs);
				/* this could be better, like instead we should
				evaluate the rhs and then point the entity
				to the register of where the result is, otherwise
				if we allocate a register now, it won't be
				available for the code generator to use */
				elf_new_entity(fs,name.line,0,name.text,reg);
				elf_fassignmaybe(fs,elf_node_local(fs,name.line,reg));
			} while (elf_pick_token(fs,TK_COMMA));
		} break;
		case TK_LEAVE: {
			elf_poll_token(fs);
			x=elf_parse_expr(fs,0,0);
			if (fs->default_register!=NO_SLOT) {
				elf_emitstore(fs,tk.line,elf_node_local(fs,NO_LINE,fs->default_register),x);
			} else {
				elf_emit_yield(fs,tk.line,x);
			}
			elASSERT(elf_get_mem_state(fs)==mem);
		} break;
		case TK_BREAK: case TK_CONTINUE: {
			elNodeId value;
			int reg;

			reg=NO_NODE;
			elf_poll_token(fs);
			if (!elf_term_eol_token(fs)) {
				if ((value=elf_parse_unary(fs,0,0))!=NO_NODE) {
					if ((reg=elf_get_node_register(fs,MAKE_NODE_ID(value)))<0) {
						elf_fdialog(fs,elf_get_node_line(fs,value),"invalid value");
					}
				}
			}
			if (tk.type==TK_CONTINUE) elf_emitcontinue(fs,tk.line,reg);
			else elf_emitbreak(fs,tk.line,reg);
		} break;
		case TK_WHILE: {
			elf_poll_token(fs);
			elf_begin_block(fs,BLOCK_LOOP);
			x=elf_parse_expr(fs,0,0);
			elf_take_token(fs,TK_QMARK);
			elf_emit_begin_while_loop(fs,x);
			elf_parse_stat(fs);
			elf_emit_close_while_loop(fs);
			elASSERT(elf_get_mem_state(fs)==mem);
			elf_close_block(fs);
		} break;
		case TK_DO: {
			elf_poll_token(fs);
			elf_begin_block(fs,BLOCK_LOOP);
			elf_fbegindowhileloop(fs,tk.line);
			elf_parse_stat(fs);
			elf_take_token(fs,TK_WHILE);
			x=elf_parse_expr(fs,0,0);
			elf_fclosedowhileloop(fs,tk.line,x);
			elASSERT(elf_get_mem_state(fs)==mem);
			elf_close_block(fs);
		} break;
		case TK_CURLY_LEFT: {
			elf_begin_block(fs,0);
			elf_poll_token(fs);
			while (!elf_term_token(fs,TK_CURLY_RIGHT)) {
				elf_parse_stat(fs);
			}
			elf_take_token(fs,TK_CURLY_RIGHT);
			elf_close_block(fs);
		} break;
		case TK_FOR: {
			elf_fforloop(fs);
		} break;
		default: {
			elNodeId x;
			x=elf_parse_expr(fs,0,0);
			elf_fassignmaybe(fs,x);
			if (x==NO_NODE) {
				elf_fdialog(fs,tk.line,"invalid statement");
			}
			elASSERT(elf_get_mem_state(fs)==mem);
		} break;
	}
	// elASSERT(fs->nnodes==0);
	return 1;
}

/*
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
	*/
void elf_fforloop(elFileState *fs) {
	elToken tk = elf_take_token(fs,TK_FOR);
	elToken name = elf_take_token(fs,TK_WORD);

	elf_take_token(fs,TK_ASSIGN);

	elNodeId *z = 0;
	do {
		elFileExpr expr = {0};
		elNodeId y = elf_parse_expr(fs,&expr,0);

		if (elf_get_node_kind(fs,y) == NODE_MULTI) {
			elNodeId *yz = elf_get_node(fs,y).z;
			for (int i = 0; i < ARRAY_LENGTH(yz); i += 1) {
				ARRAY_ADD(z,yz[i]);
			}
		} else {
			ARRAY_ADD(z,y);
		}
	} while (elf_pick_token(fs,TK_COMMA));

	elf_take_token(fs,TK_QMARK);

	elByteId block_head = NO_BYTE;
	elByteId block_tail = NO_BYTE;

			/* todo: why not create this per
			expression instead */
	elBlockId block = elf_begin_block(fs,BLOCK_LOOP);

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
	elFileExpr expr = {0};
	elRegId value_register = elf_fs_local_alloc(fs);
	elNodeId value = elf_node_local(fs,name.line,value_register);
			/* todo: remove REFERENCED, instead allow the user to not have to
			specify the name */
	elf_new_entity(fs, name.line
	,	ENTITY_REFERENCED|ENTITY_ASSIGNED|ENTITY_FORLOOP,name.text,value_register);


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
			array_register = elf_emitter_load(fs,array);

			y = elf_get_node(fs,y).y;
			elASSERT(elf_get_node_kind(fs,y) == NODE_RANGE);
		}
		if (elf_get_node_kind(fs,y) == NODE_RANGE) {
			elNodeId lo = elf_get_node(fs,y).x;
			elNodeId hi = elf_get_node(fs,y).y;

			if (array == NO_NODE) {
				index = value;
						// for .. ? { }
				if (lo == NO_NODE) lo = elf_node_integer(fs,tk.line, 0);
				if (hi == NO_NODE) hi = elf_node_integer(fs,tk.line,-1);
			} else {
						// for array[..] ? { }
				if (lo == NO_NODE) lo = elf_node_integer(fs,tk.line,0);
				if (hi == NO_NODE) hi = elf_ncallmetafield(fs,tk.line,array,0,"length");

						/* todo: we're allocating this here, and never freeing it! */
				index = elf_node_local(fs,tk.line,elf_fs_local_alloc(fs));
			}

			elf_fbeginrangeloop(fs,tk.line,index,lo,hi);

			elf_get_block(fs,block)->loop.value_register = value_register;
			if (array != NO_NODE) {
				elf_get_block(fs,block)->loop.array_register = array_register;
			}

			if (array != NO_NODE) {
				elNodeId *z = {0};
				ARRAY_ADD(z,index);

						/* todo: make this neater */
				elf_emitstore(fs,name.line,value,
				elf_ncallmetafield(fs,tk.line,array,z,"idx"));
						// elf_node_call(fs,tk.line,
						// elf_node_metafield(fs,tk.line,array,
						// elf_node_string(fs,tk.line,"idx")),z));
			}

						/* todo: could be neater */
			if (i == 0) {
				block_head = elf_get_instr_cursor(fs);
				elf_parse_stat(fs);
				block_tail = elf_get_instr_cursor(fs);
			} else {
				for (int j = block_head; j < block_tail; j += 1) {
					elf_emit_byte(fs,
					elf_get_instr_line(fs,j),
					elf_get_byte(fs,j));
				}
			}

			elf_fcloserangeloop(fs,tk.line);
		} else {
			elf_emitstore(fs,tk.line,value,y);


			if (i == 0) {
				block_head = elf_get_instr_cursor(fs);
				elf_parse_stat(fs);
				block_tail = elf_get_instr_cursor(fs);
			} else {
				for (int j = block_head; j < block_tail; j += 1) {
					elf_emit_byte(fs,
					elf_get_instr_line(fs,j),
					elf_get_byte(fs,j));
				}
			}

						/* because we are within a loop
						the user can use "continue" and
						"break", continues are the ones
						we need to handle here, which
						mean move on to the next step. */
			elFileBlock *loop = elf_get_block(fs,block);

			elf_emit_patch_jumplist(fs,loop->loop.true_jumps);
			ARRAY_DELETE(loop->loop.true_jumps);
			loop->loop.true_jumps = 0;
		}
	}
	elf_close_block(fs);
}