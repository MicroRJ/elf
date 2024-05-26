/*
** See Copyright Notice In elf.h
** elf-file.c
** Parsing Stuff
*/


elBool elf_fscheckexpr(elFileState *fs, elf_lineid line, elf_nodeid id) {
	if (id != NO_NODE) return lfalse;
	elf_filediag(fs,line,"invalid expression");
	return ltrue;
}


elBool elf_linetesttk(elFileState *fs, ltokentype k) {
	return fs->tk.type == k && fs->lasttk.eol != ltrue;
}


elBool elf_testtk(elFileState *fs, ltokentype k) {
	return fs->tk.type == k;
}

elBool elf_testthentk(elFileState *fs, ltokentype k) {
	return fs->thentk.type == k;
}


/*
** Returns true whether there are no more tokens
** or whehter the current token is a match.
*/
elBool elf_termtk(elFileState *fs, ltokentype k) {
	return fs->tk.type == TK_NONE || fs->tk.type == k;
}


elBool elf_iseolalready(elFileState *fs) {
	return fs->lasttk.eol;
}


elBool elf_termeoltk(elFileState *fs) {
	return fs->tk.type == TK_NONE || fs->lasttk.eol == ltrue;
}


/*
** Consumes the current token only if it is a match,
** returning whether it was a match or not.
*/
elBool elf_picktk(elFileState *fs, ltokentype k) {
	return elf_testtk(fs,k) && (elf_lexone(fs), ltrue);
}


elBool elf_linepicktk(elFileState *fs, ltokentype k) {
	return elf_linetesttk(fs,k) && (elf_lexone(fs), ltrue);
}


/*
** Same as pick, only this time there are two possibilities.
*/
elBool elf_choosetk(elFileState *fs, ltokentype x, ltokentype y) {
	return (elf_testtk(fs,x) || elf_testtk(fs,y)) && (elf_lexone(fs), ltrue);
}


/*
** Check whether the current token is a match,
** if so pick it, otherwise error.
*/
elToken elf_taketk(elFileState *fs, int k) {
	elToken tk = fs->tk;
	if (!elf_picktk(fs,k)) {
		elf_filediag(fs,fs->tk.line,"expected '%s'\n",elf_tkintel[k].name);
	}
	return tk;
}


elToken elf_linetaketk(elFileState *fs, int k) {
	elToken tk = fs->tk;
	if (!elf_linepicktk(fs,k)) {
		elf_filediag(fs,fs->tk.line,"expected '%s'\n",elf_tkintel[k].name);
	}
	return tk;
}


lentityid elfY_allocentity(elFileState *fs, elf_lineid line) {
	elFileFnState *ff = fs->fn;
	lentityid id = {fs->nentities ++};
	if (elf_varlen(fs->entities) < fs->nentities) {
		elf_varaddi(fs->entities,1);
	}

	for (elf_localid i = id.x; i < fs->nentities; ++i) {
		fs->entities[i].level = fs->level;
		fs->entities[i].line  = line;
		fs->entities[i].name  = 0;
		fs->entities[i].slot  = 0;
		fs->entities[i].enm   = lfalse;
	}
	// elf_filediag(fs,line,"%i, fs=%i, ff=%i",id,fs->nentities,nentities);
	return id;
}


elf_localid elf_fsnumentinlev(elFileState *fs) {
	elf_fileentry *entities = fs->entities;
	int nentities = fs->nentities;
	int n;
	for (n = 0; n < nentities; ++ n) {
		if (entities[nentities-1-n].level < fs->level) {
			break;
		}
	}
	return n;
}


elf_nodeid elf_fsnumnodesinlev(elFileState *fs) {
	elf_Node *nodes = fs->nodes;
	elf_nodeid nnodes = fs->nnodes;
	elf_nodeid n;
	for (n = 0; n < nnodes; ++n) {
		if (nodes[nnodes-1-n].level < fs->level) {
			break;
		}
	}
	return n;
}


void elf_fscheckassign(elFileState *fs, elf_lineid line, elf_nodeid x) {
}


/*
** Looks for a captured entity within the function's captures
** and returns the index where the captured entity resides.
** the index is then used to emit instructions targeting
** captures, the first capture corresponds to index 0.
*/
int elfY_indexofentityincache(elFileFnState *fn, lentityid id) {
	elf_arrfori(fn->captures) {
		if (fn->captures[i].x == id.x) return i;
	}
	return -1;
}


/*
** Captures or traps an entitity (if not already)
** from the enclosing function, storing
** a copy of the entity id in the
** function's captures.
*/
void elf_fstrapent(elFileState *fs, elFileFnState *fn, lentityid id) {
	/* ensure the entity should actually be captured */
	elf_ensure(id.x < fn->entities);
	elf_arrfori(fn->captures) {
		if (fn->captures[i].x == id.x) return;
	}
	elf_varadd(fn->captures,id);
}


/*
** Find the closest entity in order of lexical
** relevance, starting from the last declared entity.
** The id returned is an absolute index into fs->entities,
** you should make it relative to the current function.
** If the entity is outside of this function, then it caches it.
*/
lentityid elf_fsfndent(elFileState *fs, elf_lineid line, char *name) {
	elFileFnState *fn = fs->fn;
	for (int x = fs->nentities-1; x >= 0; --x) {
		if (S_eq(fs->entities[x].name,name)) {
			lentityid id = {x};
	if (x < fn->entities) {
		elf_fstrapent(fs,fn,id);
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
elf_nodeid elf_fsnewlocalentity(elFileState *fs, elf_lineid line, char *name, elBool enm) {
	elFileFnState *fn = fs->fn;
	lentityid id = elf_fsfndent(fs,line,name);
	if (id.x == NO_ENTITY.x) {
		id = elfY_allocentity(fs,line);

		/* -- todo: for compile time constants,
		no slot allocation required */
		elf_localid slot = elf_genlocalalloc(fs,1);
		fs->entities[id.x].slot = slot;
		fs->entities[id.x].enm  = enm;
		fs->entities[id.x].name = name;

		return elf_nodelocal(fs,line,slot);
	} else {
		elf_fileentry entity = fs->entities[id.x];
		/* is this variable name already present in this level? */
		if (entity.level == fs->level) {
			elf_filediag(fs,line,"'%s': already declared",name);
		} else {
			elf_filediag(fs,line,"'%s': this declaration shadows another one",name);
		}
		return elf_nodelocal(fs,line,entity.slot);
	}
}


elf_nodeid elf_fsfndentitynode(elFileState *fs, elf_lineid line, char *name) {
	lentityid id = elf_fsfndent(fs,line,name);
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
			elf_filediag(fs,line,"too many layers for caching");
		}
		return elf_nodeclsval(fs,line,elfY_indexofentityincache(fn,id));
	} else return elf_nodelocal(fs,line,fs->entities[id.x].slot);
}


void elf_enterblock(elFileState *fs, elf_fileblock *bl, elBool flags) {
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


void elf_leaveblock(elFileState *fs) {
	fs->nentities -= elf_fsnumentinlev(fs); /* close scope */
	fs->nnodes -= elf_fsnumnodesinlev(fs);
	elf_ensure(fs->nentities >= fs->fn->entities);
	fs->level = fs->level - 1;
	elf_fileblock *bl = fs->fn->block;
	/* every expression and statement
	should deallocate whatever registers
	it used, so here we can assert that
	the memory state is identical, otherwise, bug! */
	elf_ensure(bl->level == fs->level);
	fs->fn->block = bl->enclosing;
	fs->fn->xmemory = bl->xmemory;
	if (bl->leavejumps != 0) {
		langL_tieloosejs(fs,bl->leavejumps);
		elf_delvar(bl->leavejumps);
		bl->leavejumps = 0;
	}
	fs->fn->nloops -= (bl->flags & BLOCK_LOOP) != 0;
}


void elf_beginfsfn(elFileState *fs, elFileFnState *fn, char *line) {
	fn->enclosing = fs->fn;
	fn->entities = fs->nentities;
	fn->bytes = fs->md->nbytes;
	fn->line = line;
	fn->yj = elNIL;
	fs->fn = fn;
	elf_enterblock(fs,&fn->entry,0);
}


void elf_closefsfn(elFileState *fs) {
	langL_fnepiloge(fs,fs->lasttk.line);
	elf_ensure(fs->fn->block == &fs->fn->entry);
	elf_leaveblock(fs);
	/* ensure all locals were deallocated
	properly */
	elf_ensure(fs->nentities == fs->fn->entities);
	fs->fn = fs->fn->enclosing;
}


elf_nodeid *elf_fsloadcallargs(elFileState *fs) {
	/* ( x { , x } ) */
	elf_nodeid *z = 0;
	if (elf_picktk(fs,TK_PAREN_LEFT)) {
		if (!elf_testtk(fs,TK_PAREN_RIGHT)) do {
			elf_nodeid x = elf_fsloadexpr(fs);
			if (x == NO_NODE) break;
			elf_varadd(z,x);
		} while (elf_picktk(fs,TK_COMMA));
		elf_taketk(fs,TK_PAREN_RIGHT);
	}
	return z;
}


/* pretty self explanatory function,
could have used the token itself, but
that's for smart people to do. */
elf_nodeop tktonode(ltokentype tk) {
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


elf_nodeid elfY_loadsubexpr(elFileState *fs, int rank) {
	elf_nodeid x = elf_fsloadunary(fs);
	if (x == NO_NODE) return x;
	for (;;) {
		int thisrank = elf_tkintel[fs->tk.type].prec;
		/* auto breaks when not a binary operator */
		if (thisrank <= rank) break;
		// *= += ...
		if (elf_testthentk(fs,TK_ASSIGN))  {
			/* todo: check subexpression level
			to ensure the user knows this is
			not an expression, if we're at level
			0, the statement parser will handle
			this and turn it into a statement... */
			break;
		}
		elToken tk = elf_lexone(fs);
		elf_nodeid y = elfY_loadsubexpr(fs,thisrank);
		if (y == NO_NODE) break;
		x = elf_nodebinary(fs,tk.line,tktonode(tk.type),NT_ANY,x,y);
	}
	return x;
}


elf_nodeid elf_fsloadfun(elFileState *fs) {

	elToken tk = elf_taketk(fs,TK_FUN);

	/* All bytecode is outputted to the same
	module, the way this language works is
	that we just run the entire module,
	for that reason add a jump byte to
	skip this function's code. */
	int fj = elf_emitjump(fs,tk.line,-1);

	elFileFnState fn = {0};
	elf_beginfsfn(fs,&fn,tk.line);

	int arity = 0;

	elf_taketk(fs,TK_PAREN_LEFT);
	if (!elf_testtk(fs,TK_PAREN_RIGHT)) do {

		elToken n = elf_taketk(fs,TK_WORD);
		elf_fsnewlocalentity(fs,n.line,n.s,lfalse);

		arity ++;
	} while (elf_picktk(fs,TK_COMMA));
	elf_taketk(fs,TK_PAREN_RIGHT);

	elf_taketk(fs,TK_QUESTION_MARK);

	// ? { .. }
	if (elf_testtk(fs,TK_CURLY_LEFT)) {
		elf_taketk(fs,TK_CURLY_LEFT);
		while (!elf_termtk(fs,TK_CURLY_RIGHT)) {
			elf_fsloadstat(fs);
		}
		elf_taketk(fs,TK_CURLY_RIGHT);
	} else {
		elf_emityield(fs,tk.line,elf_fsloadexpr(fs));
	}

	elf_closefsfn(fs);

	/* add this function to the type table */
	elProto p = {0};
	p.x = arity;
	p.y = fn.nyield;
	p.nlocals = fn.nlocals;
	p.bytes = fn.bytes;
	p.nbytes  = fs->md->nbytes - fn.bytes;
	p.ncaches = elf_varlen(fn.captures);
	int f = lang_addproto(fs->md,p);

	/* patch jump */
	langL_tieloosej(fs,fj);

	/* todo: */
	elf_nodeid *z = elNIL;
	elf_arrfori(fn.captures) {
		elf_varadd(z,elf_nodelocal(fs,tk.line,fs->entities[fn.captures[i].x].slot));
	}

	return elf_nodecls(fs,tk.line,f,z);
}


void elf_fsmayassign(elFileState *fs, elf_nodeid x) {
	elToken tk = fs->tk;
	elf_localid mem = fs->fn->xmemory;
	if (elf_picktk(fs,TK_ASSIGN)) {
		elf_fscheckassign(fs,tk.line,x);
		elf_nodeid y = elf_fsloadexpr(fs);
		langL_moveto(fs,tk.line,x,y);
	} else if (elf_picktk(fs,TK_ASSIGN_QUESTION)) {
		elf_fscheckassign(fs,tk.line,x);
		elf_nodeid y = elf_fsloadexpr(fs);
		ljlist js = {0};
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
		langL_jumpifnotnil(fs,tk.line,&js,x);
		langL_moveto(fs,tk.line,x,y);
		langL_tieloosejs(fs,js.f);
	} else {
		/* the lhs of an assignment is an expression,
		and thus it is parsed by the expression parser.
		the expression parsers checks whether there's
		an operator to make a binary expression however,
		if the operator is followed by '=' then it quits,
		so if we get here and we see an operator it's
		guaranteed to be a '{x}=' assignment. */
		if (elf_tkintel[tk.type].prec > 0) {
			elToken op = elf_lexone(fs);
			elf_taketk(fs,TK_ASSIGN);
			elf_fscheckassign(fs,fs->lasttk.line,x);
			elf_nodeid y = elf_fsloadexpr(fs);
			y = elf_nodebinary(fs,op.line,tktonode(op.type),fs->nodes[x].t,x,y);
			langL_moveto(fs,op.line,x,y);
		} else {
			elf_localid r = elf_genlocalalloc(fs,1);
			langL_localload(fs,NO_LINE,lfalse,r,0,x);
		}
		fs->fn->xmemory = mem;
	}
	elf_ensure(fs->fn->xmemory == mem);
}


elf_nodeid elf_fsloadtable(elFileState *fs) {
	elToken tk = fs->tk;
	elf_taketk(fs,TK_CURLY_LEFT);
	elf_nodeid *z = elNIL;
	elf_nodeid table = elf_nodetab(fs,tk.line,elNIL);
	int index = 0;
	while (!elf_termtk(fs,TK_CURLY_RIGHT)) {
		if (elf_testthentk(fs,TK_ASSIGN)) {
			tk = fs->lasttk;
			fs->flags |= NOTANENTITY;
			elf_nodeid key = elf_fsloadexpr(fs);
			fs->flags &= ~NOTANENTITY;
			elf_taketk(fs,TK_ASSIGN);
			elf_nodeid val = elf_fsloadexpr(fs);
			if (elf_fscheckexpr(fs,fs->tk.line,val)) {
				break;
			}
			elf_nodeid fld = elf_nodefield(fs,fs->lasttk.line,table,key);
			elf_nodeid f = elf_nodeload(fs,fs->lasttk.line,fld,val);
			elf_varadd(z,f);
		} else {
			elf_nodeid val = elf_fsloadexpr(fs);
			if (elf_fscheckexpr(fs,fs->tk.line,val)) break;
			elf_nodeid ii = elf_nodeint(fs,fs->lasttk.line,index ++);
			elf_nodeid f = elf_nodeload(fs,fs->lasttk.line,elf_nodeindex(fs,fs->lasttk.line,table,ii),val);
			elf_varadd(z,f);
		}
		if (elf_picktk(fs,TK_COMMA)) {
			continue;
		}
	}
	elf_taketk(fs,TK_CURLY_RIGHT);
	fs->nodes[table].z = z;
	return table;
}


elf_nodeid elf_fsloadexpr(elFileState *fs) {
	/* todo?: do this in else where? */
	switch (fs->tk.type) {
		case TK_NONE:
		case TK_PAREN_RIGHT:
		case TK_CURLY_RIGHT:
		case TK_SQUARE_RIGHT: {
			return NO_NODE;
		}
	}
	return elfY_loadsubexpr(fs,0);
}


elf_nodeid elf_fsloadunary(elFileState *fs) {
	LDODEBUG(
		if (fs->debuggerflag) {
			elf_debugger("__ELF_FILE_BREAK__");
		}
	);
	elf_nodeid v = NO_NODE;
	elToken tk = fs->tk;
	switch (tk.type) {
		/* elf is a reserved keyword used
		for the elf directory. */
		case TK_DOT: case TK_ELF: {
			char dir[MAX_PATH] = {};
			if (elf_picktk(fs,TK_ELF)) {
				strcat(dir,"elf");
		/* remind the user elf is a reserved keyword */
		if (!elf_linetesttk(fs,TK_DOT)) {
			elf_filediag(fs,tk.line,"incomplete symbol, expected '.' on the same line as 'elf'. Did you mean to use 'elf'? This is a reserved keyword and it refers to the elf directory.");
		}
			}
			elf_taketk(fs,TK_DOT);
			do {
				elf_linetaketk(fs,TK_WORD);
				strcat(dir,".");
				strcat(dir,fs->lasttk.s);
			} while (elf_linepicktk(fs,TK_DOT));

			elf_globalid x = elf_getsymbol(fs->M,elf_newstr(fs->R,dir));
			v = elf_nodeglobal(fs,tk.line,x);
		} break;
		case TK_WORD: {
			elf_lexone(fs);
	if (~fs->flags & NOTANENTITY) {
		v = elf_fsfndentitynode(fs,tk.line,tk.s);
		if (v == NO_NODE) {
			// elf_filediag(fs,tk.line,"warning: '%s' implicit global declaration, did you mean this?",tk.s);
			elf_globalid x = elf_getsymbol(fs->M,elf_newstr(fs->R,tk.s));
			elf_ensure(x != -1);
			v = elf_nodeglobal(fs,tk.line,x);
		}
		if (v == NO_NODE) {
			elf_filediag(fs,tk.line,"'%s': undeclared identifier",tk.s);
		}
	/* otherwise this is a string */
	} else v = elf_nodestr(fs,tk.line,tk.s);
		} break;
		case TK_SUB: {
			elf_lexone(fs);
			v = elfY_loadsubexpr(fs,10000);
			v = elf_nodebinary(fs,tk.line,NODE_SUB,NT_INT,elf_nodeint(fs,tk.line,0),v);
		} break;
		case TK_ADD: {
			elf_lexone(fs);
			v = elfY_loadsubexpr(fs,10000);
		} break;
		case TK_CURLY_LEFT: {
			v = elf_fsloadtable(fs);
		} break;
		case TK_PAREN_LEFT: {
			elf_lexone(fs);
			v = elf_fsloadexpr(fs);
			elf_taketk(fs,TK_PAREN_RIGHT);
			/* allow for empty () */
			if (v != NO_NODE) {
				v = elf_nodegroup(fs,tk.line,v);
			}
		} break;
		case TK_FUN: {
			v = elf_fsloadfun(fs);
		} break;
		case TK_LOAD: {
			elf_lexone(fs);
			v = elf_fsloadunary(fs);
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
			v = elf_nodeint(fs,tk.line,tk.type == TK_TRUE);
		} break;
		case TK_LETTER: case TK_INTEGER: { elf_lexone(fs);
			v = elf_nodeint(fs,tk.line,tk.i);
		} break;
		case TK_NUMBER: { elf_lexone(fs);
			v = elf_nodenum(fs,tk.line,tk.n);
		} break;
		case TK_STRING: { elf_lexone(fs);
			v = elf_nodestr(fs,tk.line,tk.s);
		} break;
		default: {
			elf_filediag(fs,tk.line,"'%s': unexpected token", elf_tkintel[tk.type].name);
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
	while (!elf_termeoltk(fs)) {
		tk = fs->tk;

		switch (tk.type) {
			case TK_DOT: { elf_lexone(fs);
				elToken n = elf_taketk(fs,TK_WORD);
				elf_nodeid i = elf_nodestr(fs,n.line,n.s);
				v = elf_nodefield(fs,tk.line,v,i);
			} break;
			case TK_SQUARE_LEFT: {
				elf_taketk(fs,TK_SQUARE_LEFT);
				elf_nodeid i = elf_fsloadexpr(fs);
				elf_taketk(fs,TK_SQUARE_RIGHT);
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
				elToken n = elf_taketk(fs,TK_WORD);
				elf_nodeid y = elf_nodestr(fs,n.line,n.s);
				v = elf_nodemetafield(fs,tk.line,v,y);
			} break;
			case TK_PAREN_LEFT: {
				elf_nodeid *z = elf_fsloadcallargs(fs);
				v = elf_nodecall(fs,tk.line,v,z);
			} break;
			default: goto leave;
		}
	}

	leave:
	return v;
}


/* todo: make this legit */
void elfY_loadenumlist(elFileState *fs) {
	if (!elf_testtk(fs,TK_CURLY_RIGHT)) {
		do {
			/* , } */
			if (elf_testtk(fs,TK_CURLY_RIGHT)) {
				break;
			}
			elToken tk = fs->tk;
			elToken n = elf_taketk(fs,TK_WORD);
			elf_nodeid x = elf_fsnewlocalentity(fs,n.line,n.s,ltrue);
			elf_taketk(fs,TK_ASSIGN);
			elf_nodeid y = elf_fsloadexpr(fs);
			langL_moveto(fs,tk.line,x,y);
		} while(elf_picktk(fs,TK_COMMA));
	}
}


void elf_fsloadstat(elFileState *fs) {
	elf_localid mem = fs->fn->xmemory;
	elToken tk = fs->tk;
	elFileFnState *fn = fs->fn;
	elf_fileblock *bl = fn->block;
	if (bl->flags & BLOCK_ENDED) {
		elf_filediag(fs,tk.line,"warning: unreachable statement");
	}
	switch (tk.type) {
		case TK_THEN: case TK_ELSE: case TK_ELIF: {
		} break;
		case TK_LASTLY: case TK_FINALLY: {
			elf_lexone(fs);
			if (tk.type == TK_FINALLY) {
				elf_filediag(fs,tk.line,"warning: please consider using 'lastly' instead, 'finally' could change semantics in the future");
			}
			elf_fileblock bl = {0};
			elf_enterlastlyblock(fs,tk.line,&bl);
			elf_fsloadstat(fs);
			elf_closelastlyblock(fs,tk.line,&bl);
			elf_ensure(fs->fn->xmemory == mem);
		} break;
		case TK_IF: case TK_IFF: {
			elf_lexone(fs);
			elf_nodeid x = elf_fsloadexpr(fs);
			elf_taketk(fs,TK_QUESTION_MARK);
			elf_fileblock block = {0};
			elf_enterblock(fs,&block,0);
			Select s = {0};
			elf_genbeginif(fs,tk.line,&s,x,tk.type==TK_IFF?L_IFF:L_IF);
			elf_fsloadstat(fs);
			while (!elf_testtk(fs,TK_NONE)) {
				elf_fileblock block = {0};
				if (elf_picktk(fs,TK_ELIF)) {
					elf_enterblock(fs,&block,0);
					x = elf_fsloadexpr(fs);
					elf_taketk(fs,TK_QUESTION_MARK);
					langL_addelif(fs,fs->lasttk.line,&s,x);
					elf_fsloadstat(fs);
					elf_leaveblock(fs);
				} else
				if (elf_picktk(fs,TK_THEN)) {
					elf_enterblock(fs,&block,0);
					langL_addthen(fs,fs->lasttk.line,&s);
					elf_fsloadstat(fs);
					elf_leaveblock(fs);
				} else
				if (elf_picktk(fs,TK_ELSE)) {
					elf_enterblock(fs,&block,0);
					langL_addelse(fs,fs->lasttk.line,&s);
					elf_fsloadstat(fs);
					elf_leaveblock(fs);
				} else break;
			}
			langL_closeif(fs,fs->lasttk.line,&s);
			elf_leaveblock(fs);
			elf_ensure(fs->fn->xmemory == mem);
		} break;
		case TK_LET: { elf_lexone(fs);
			if (tk.type == TK_ENUM) {
				elf_filediag(fs,tk.line,"global enums are not supported yet, this enum will be made local");
			} else
			/* allows for 'let enum' */
			if (tk.type == TK_LET) {
				if (elf_testtk(fs,TK_ENUM)) {
					tk = elf_lexone(fs);
				}
			}
			elBool enm = tk.type == TK_ENUM;
			if (elf_picktk(fs,TK_CURLY_LEFT)) {
				elfY_loadenumlist(fs);
				elf_taketk(fs,TK_CURLY_RIGHT);
			} else {
				do {
					elToken n = elf_taketk(fs,TK_WORD);
					elf_nodeid x = elf_fsnewlocalentity(fs,n.line,n.s,enm);
					elf_fsmayassign(fs,x);
				} while (elf_picktk(fs,TK_COMMA));
			}
		} break;
		case TK_LEAVE: { elf_lexone(fs);
			bl->flags |= BLOCK_ENDED;
			elf_nodeid x = elf_fsloadexpr(fs);
			elf_emityield(fs,tk.line,x);
			elf_ensure(fs->fn->xmemory == mem);
		} break;
		case TK_CONTINUE: { elf_lexone(fs);
			elf_emitcontinue(fs,tk.line);
		} break;
		case TK_BREAK: { elf_lexone(fs);
			elf_emitbreak(fs,tk.line);
		} break;
		case TK_WHILE: { elf_lexone(fs);
			elf_fileblock block = {0};
			elf_enterblock(fs,&block,BLOCK_LOOP);
			elf_nodeid x = elf_fsloadexpr(fs);
			elf_taketk(fs,TK_QUESTION_MARK);
			langL_beginwhile(fs,tk.line,x);
			elf_fsloadstat(fs);
			langL_closewhile(fs,tk.line);
			elf_ensure(fs->fn->xmemory == mem);
			elf_leaveblock(fs);
		} break;
		case TK_DO: { elf_lexone(fs);
			elf_fileblock block = {0};
			elf_enterblock(fs,&block,BLOCK_LOOP);
			langL_begindowhile(fs,tk.line);
			elf_fsloadstat(fs);
			elf_taketk(fs,TK_WHILE);
			elf_nodeid x = elf_fsloadexpr(fs);
			langL_closedowhile(fs,tk.line,x);
			elf_ensure(fs->fn->xmemory == mem);
			elf_leaveblock(fs);
		} break;
		case TK_FOREACH: case TK_FOR: {
			elf_lexone(fs);
			elf_fileblock block = {0};
			elf_enterblock(fs,&block,BLOCK_LOOP);
			elToken n = elf_taketk(fs,TK_WORD);
			elf_nodeid i, x, y, lo, hi;
			x = elf_fsnewlocalentity(fs,n.line,n.s,lfalse);
			elf_taketk(fs,TK_IN);
			y = elf_fsloadexpr(fs);
			elf_taketk(fs,TK_QUESTION_MARK);
			i = x; lo = NO_NODE; hi = NO_NODE;
			if (tk.type == TK_FOR) {
				if (fs->nodes[y].k == NODE_RANGE) {
					lo = fs->nodes[y].x;
					hi = fs->nodes[y].y;
				} else {
					lo = elf_nodeint(fs,tk.line,0);
					hi = y;
				}
			} else {
				elf_unreachable;/* todo: emit code to initialize x to i'th item of y */
				if (fs->nodes[y].k == NODE_RANGE) {
					lo = fs->nodes[y].x;
					hi = fs->nodes[y].y;
				} else {
					lo = elf_nodeint(fs,tk.line,0);
					/* todo: add type guard */
					hi = elf_nodemetafield(fs,tk.line,y,elf_nodestr(fs,tk.line,"length"));
					i = elf_nodelocal(fs,tk.line,elf_genlocalalloc(fs,1));
				}
			}
			elf_beginrangedloop(fs,tk.line,i,lo,hi);
			elf_fsloadstat(fs);
			elf_closerangedloop(fs,tk.line);
			elf_leaveblock(fs);
		} break;
		case TK_CURLY_LEFT: { elf_lexone(fs);
			elf_fileblock block = {0};
			elf_enterblock(fs,&block,0);
			while (!elf_termtk(fs,TK_CURLY_RIGHT)) {
				elf_fsloadstat(fs);
			}
			elf_taketk(fs,TK_CURLY_RIGHT);
			elf_leaveblock(fs);
		} break;
		default: {
			elf_nodeid x = elf_fsloadexpr(fs);
			elf_fsmayassign(fs,x);
			elf_ensure(fs->fn->xmemory == mem);
			elf_ensure(x != NO_NODE);
		} break;
	}
}


