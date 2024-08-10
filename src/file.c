/*
** See Copyright Notice In elf.h
** file.c
** Parsing And Code Generation...
*/


/* todo: this function is temporary as I rework
the parser and code generator */
elNodeId elf_Zlocalize(elFileState *fs, elNodeId x) {
	if (x == NO_NODE) return x;

	elFileline line = elf_get_node_line(fs,x);
	return elf_Nlocal(fs,line,elf_Zlocalize2reg(fs,line,x));
}



char *elf_get_file_name(elFileState *fs) {
	return fs->filename;
}


elBool elf_check_expr(elFileState *fs, elFileline line, elNodeId id) {
	if (id != NO_NODE) return 0;
	elf_fdialog(fs,line,"invalid expression");
	return 1;
}


elBool elf_test_token_inline(elFileState *fs, elTokenType k) {
	return fs->this_token.type == k && fs->last_token.eol != 1;
}


elBool elf_ftesttok(elFileState *fs, elTokenType k) {
	return fs->this_token.type == k;
}

elBool elf_test_then_token(elFileState *fs, elTokenType k) {
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
elBool elf_fmaytok(elFileState *fs, elTokenType k) {
	return elf_ftesttok(fs,k) && (elf_flextok(fs), 1);
}


elBool elf_pick_token_inline(elFileState *fs, elTokenType k) {
	return elf_test_token_inline(fs,k) && (elf_flextok(fs), 1);
}


/*
** Same as pick, only this time there
** are two possibilities.
*/
elBool elf_choose_token(elFileState *fs, elTokenType x, elTokenType y) {
	return (elf_ftesttok(fs,x) || elf_ftesttok(fs,y)) && (elf_flextok(fs), 1);
}


/*
** Check whether the current token is a match,
** if so pick it, otherwise error.
*/
elToken elf_fgettok(elFileState *fs, int k) {
	elToken tk = fs->tk;
	if (!elf_fmaytok(fs,k)) {
		elf_fdialog(fs,fs->this_token.line,"expected '%s'\n",elf_tkintel[k].name);
	}
	return tk;
}


elToken elf_take_token_inline(elFileState *fs, int k) {
	elToken tk = fs->tk;
	if (!elf_pick_token_inline(fs,k)) {
		elf_fdialog(fs,fs->this_token.line,"expected '%s'\n",elf_tkintel[k].name);
	}
	return tk;
}


/*
** Looks for an enclosing entity within the function's
** enclosure and returns the index where the entity
** resides.
** The index can then be used to emit instructions
** targeting closure values.
*/
int elf_find_index_of_closure_entity(elFileFnState *fn, elEntityIdTypeGuard id) {
	FOR_ARRAY(i,fn->enclosure) {
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
	/* check whether the entity was already captured */
	FOR_ARRAY(i,fn->enclosure) {
		if (fn->enclosure[i] == id.id) return;
	}
	ARRAY_ADD(fn->enclosure,id.id);
}


/*
** Finds the last declared entity with the given
** name.
**
** The id returned is absolute, but can be made
** relative to the current function.
**
** You can specify whether to add this entity
** to the function's enclosure if it resides
** outside of its scope.
**
** This functions assumes that the entity is
** being referenced, and so it is marked as such.
**
*/
elEntityId elf_Fgetentity(elFileState *fs, elFileline line, char *name, elBool enclose) {
	elFileFnState *fn = fs->fn;
	/* We're using a linear search method here, but I can guarantee
	that this is actually fast enough, introducing table will not
	be profitable for a long while... */
	elEntityId id;
	for (id = fs->nentities-1; id > -1; -- id) {
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


/* Finds the last declared entity with the given register */
elEntityId elf_Fgetentforreg(elFileState *fs, elRegId slot) {
	elFileFnState *fn = fs->fn;
	elEntityId id;
	for (id = fs->nentities-1; id > -1; -- id) {
		if (fs->entities[id].slot == slot) {
			return id;
		}
	}
	return NO_SLOT;
}


/*
** Register a local entity within the current function and level,
** if already declared issues a warning.
** Whether is shadows or redeclares and existing entity is not
** considered to be a fatal error, so it still returns a valid id.
*/
elNodeId elf_fnewlentity(elFileState *F, elFileline line, elFileExpr *expr, char *name, elBool flags) {
	elFileFnState *fn = F->fn;
	elEntityId already = elf_Fgetentity(F,line,name,0);
	if (already != NO_ENTITY) {
		elFileEntity entity = F->entities[already];
		if (entity.kind == ENTITY_DIRECTORY)  {
			elf_fdialog(F,line,"'%s': name is reserved for symbol directory",name);
		}
		if (entity.level == F->level) {
			elf_fdialog(F,line,"'%s': is already declared",name);
		} else {
			/* only issue this warning if the entity we found
			is within this function... */
			if (already >= fn->entities) {
				elf_fdialog(F,line,"'%s': this declaration shadows another one",name);
			}
		}
	}

	elEntityId id = F->nentities ++;
	ARRAY_GROW(F->entities,F->nentities-ARRAY_LENGTH(F->entities));

	elRegId slot = elf_fregalloc2(F,line,NO_SLOT,NO_NODE);
	expr->kind = NODE_LOCAL;
	expr->x    = slot;

	elNodeId node = elf_Nlocal(F,line,slot);

	F->entities[id].kind  = ENTITY_LOCAL;
	F->entities[id].line  = line;
	F->entities[id].name  = name;
	F->entities[id].slot  = slot;
	F->entities[id].level = F->level;
	F->entities[id].flags = flags;
	return node;
}


elNodeId elf_find_entity_node(elFileState *F, elFileline line, elFileExpr *expr, char *name, int flags) {
	elEntityId id = elf_Fgetentity(F,line,name,1);

	if (id == NO_ENTITY) {
		return NO_NODE;
	}

	if (flags) {
		if (~F->entities[id].flags & ENTITY_ASSIGNED) {
			elf_fdialog(F,line,"warning: usage of possibly unassigned variable");
		}
	}

	elFileFnState *fn = F->fn;
	/* to figure out whether this is capture, simply
	check whether the entity id is higher than that
	of the first entity id within this function, in
	other words, if this entity is outside of this
	function's scope */
	if (id < fn->entities) {
		/* todo: implement multilayer caching */
		if (id < fn->enclosing->entities) {
			elf_fdialog(F,line,"too many layers for caching");
		}

		expr->kind = NODE_CLOSURE_VALUE;
		expr->x    = elf_find_index_of_closure_entity(fn,ENTITY_ID(id));

		return elf_Nclsvalue(F,line,expr->x);
	} else {

		expr->kind = NODE_LOCAL;
		expr->x    = F->entities[id].local;

		return elf_Nlocal(F,line,expr->x);
	}
}


elNodeId elf_get_global_entity_node(elFileState *fs, elFileline line, char *name) {
	elSymbolId x = elf_get_global_symbol(fs->M,elf_new_string(fs->R,name));
	elASSERT(x != -1);
	return elf_make_global_value_node(fs,line,x);
}


elNodeId elf_fgetname(elFileState *F, elFileline line, elFileExpr *expr, char *name, int flags) {
	elNodeId v = elf_find_entity_node(F,line,expr,name,flags);

	if (v == NO_NODE) {
		/* todo: instead, check whether we've assigned a value
		to this entity already, otherwise issue a warning that
		we're using something that hasn't got a value yet... */

		// elf_fdialog(F,line,"warning: '%s' implicit global declaration, did you mean this?",name);
		v = elf_get_global_entity_node(F,line,name);
	}


	if (v == NO_NODE) {
		elf_fdialog(F,line,"'%s': undeclared identifier",name);
	}
	return v;
}


void elf_check_assign(elFileState *fs, elFileline line, elNodeId x, elNodeId y) {
	if (y < 0) {
		elf_fdialog(fs,line,"invalid statement, expected a value for assignment");
	}
	elASSERT(x > NO_SLOT);
	elASSERT(y > NO_SLOT);
	elNode node = elf_get_targetable_node(fs,MAKE_NODE_ID(x));

	if (!elf_is_targetable_node(node.kind)) {
		elf_fdialog(fs,line,"invalid assignment to (%s)",node2s[node.kind]);
		elf_Rthrow(fs->R,0,"syntax error: invalid assignment");
	}

	if (node.kind == NODE_LOCAL) {
		elEntityId id = elf_Fgetentforreg(fs,node.x);
		elASSERT(id != -1 && "internal error");

		if (fs->entities[id].flags & ENTITY_CONSTANT) {
			elf_fdialog(fs,line,"invalid assignment to constant entity");
			elf_Rthrow(fs->R,0,"syntax error: invalid assignment to constant entity");
		}

		fs->entities[id].flags |= ENTITY_ASSIGNED;
	}
}

/*
	How code generation works, and why limited
	intermediate representations are... limiting...

	When I first saw lua's code base, I found it
	rather obfuscated.

	Chiefly, I didn't exactly know why it generated
	code directly from source.

	After all, it had been my personal experience
	that it was best to at least generate a simple
	intermediate representation, limited to only
	expressions, and then evaluate those nodes,
	into code.
	And this had been true so far, my code generator
	and parser were drastically simpler (IMHO) than
	lua's, and I was pretty content.

	That was until I got to implement more complex
	expressions, and it was then when I made one
	fundamental realization, and consequently I realized
	why lua did things their way, and why I'd eventually
	switch to doing the same.

	Initially, I was set on the idea of a simple node system,
	this way, the parser would load the source code and
	generate nodes per expression. Then for every statement
	or line, the nodes would get evaluated according to the
	statement, the bytecode was generated from those nodes,
	and then the nodes were freed.

	For instance, the statement:

	A = B + C + D

	Initially, the parser sees A, then it searches for that
	entity, say A was a local variable previously declared
	and it points to register or local index 1, so A
	becomes a "local(1)".

	Then the parser sees '=', so it loads another expression,
	resulting in another node 'add(add(B,C),D)'.

	Now, the parser nows to emit code to assign the right
	side, to the left side.

	Since the left side is a local, it just has to evaluate
	the right side into the local's register.

	This is pretty dang simple, and it works for any expression
	of any length.

	For instance:
		A.B.C = D.E.F

	Is represented as:
		field(field(A,B),C) = field(field(D,E),F)

	Which has the nicety of being symmetrical, now
	the parser calls evaluate on the right side,
	which emits a bunch of "getfield" instructions,
	and it evaluates the left's side left operand
	'field(A,B)', which emits one get field, and since
	the left side of '=' is a field node, it emits
	a "setfield" instruction.

	So nodes made this whole system symmetrical and
	easy to implement and separate.

	This is the primary reason why I wanted some
	sort of intermediate representation, even if
	limited and short lived...

	There is one 'evaluate' function, and one 'assign'
	function.
	'evaluate' simply emits code so that the node yields
	to some register.
	And 'assign' emits code so that the right side's
	result ends up where the left side specifies.

	So a valid target node is a node where something can
	be stored, and the type of node is what determines how
	to, for instance:

	index {x}[y] uses "setindex",
	field {x}.y uses "setfield",
	for local {x} just evaluate the right side into the register of the local

	Correspondingly, these nodes can also be evaluated,

	index {x}[y] uses "getindex",
	field {x}.y uses "getfield",
	local {x} uses "reload"


	When nodes are evaluated, registers are assigned
	symmetrically and optimally (for that expression)
	as well.

	For instance:
		A + B + C + D + E + F + C + ... + N

	No matter how long the expression, it will only use
	2 registers.

	evaluate = fun(node,toreg) {
		save_memory_state() lastly restore_memory_state()
		let xx = localize(node.x)
		let yy = localize(node.y)
		emitxy(node.kind,toreg,xx,yy)
	}

	Notice how we save the current memory state
	before we modify it, and once we exit,
	restore it.
	Within, we could allocate however many
	registers we want, in order to carry out
	the evaluation.
	But, the only register that needs to
	persist is the register that was already
	allocated by the caller.
	The point is, any registers we allocate within
	'evaluate' can fully discarded because by this
	point, once the instruction is emitted that
	stores the result in to the target register,
	our expression has already used them and they
	won't be necessitated by any other expression...






	The only problem with this approach is that for more
	complex expressions that could not be represented with
	nodes, it would totally break.

	Say for instance:

	A + B + default(true) {
		let sum = 0
		for item = items[...] ? {
			sum += item
		}
		leave sum
	}

	Here the default "expression" is a type of expression
	that converts any arbitrary statement into an expression,

	Meaning that it is impossible to represent using our
	simple node system, only capable of expressing actual
	expressions.

	And it was already making range-expressions be hard
	to implement because range expressions are pure
	sugar-coating, which translate to loops:

	For instance:

		array[...100] = 1

	Translates to:

		for ...100 ? array[#index] = 1

	Which is not expressible with nodes, so it took
	the parser to desugar the node and convert it
	to something the evaluate function would understand,
	which was the 'array[#index] = 1' part.
	Then the parser had to revisit the node to close
	the loops.

	The problem really was that the node system really
	showed how limited it was when you'd use range expressions
	on the right-hand side of an assignment.

	So far the parser would just call "desugar" on the left
	side, but the right side couldn't possibly be
	"desugarized" as simply.

	For instance:

		let found = items[...] is 1

	This required the node evaluating code which so far
	had been incredibly simple to now have knowledge of
	what a range expression was and generate loops.
	The loop generation required new registers to be
	allocated and be "leaked" and then the caller would
	have close them in the reverse order.

	This didn't necessarily over complicate things too
	much because range expressions were still fully
	representable with nodes, but things were starting
	to get a bit messy and I wasn't really sure any
	more of how memory state should be managed given
	that now expressions aren't fully closed.

	And then the introduction of "default" expressions
	was what really showed me what the flaw was in the
	systems was...

	Given that nodes weren't really an AST, which could
	represent the entire language, I would have to
	generate code directly for unsupported language
	constructs in the middle of building the node tree.

	It was then that I realized that this system was
	fundamentally incapable of scaling up to more
	complex expressions without expanding the node set
	to fully represent the entire language.
	Something that I did want to do...

	For instance, take the expression:

		x is true and default(false) ? {  leave y == 2 }

	Here 'x is true' would get represented as a node,
	but then the default expression couldn't possibly
	because it can have any generic statement within.

	I tried thinking of a ways to get around this,
	since I wanted to keep the node system in place,
	after all, I did like it, but none of the ideas
	were actually good, at all.
 	I was a fundamental limitation of the system right
 	from the beginning that I did not foresee
 	because of my lack of experience.






	So we'd see parent expressions first, then child
	expressions...


	This made it fairly trivial to allocate registers
	for each expression.
	And it was especially trivial for assignment
	statements, since we started from the top level, we knew
	that memory had to be stored, because memory was shared for both sides.

	(a.b.c.d) = (1 + 2 + 3 + 4)

	The left side would only use two registers,
	and the right side only two as well, but
	they would not intefere with each other...

	For setfield instructions, 3 registers
	are required, the table register, the
	field register, and the value register.

	Meaning the left side expression must "leak"
	two registers...
*/


void elf_fexpr2reg(elFileState *F, elFileExpr *ex, int loc) {
	switch (ex->kind) {
		case NODE_FIELD: case NODE_INDEX: {
			elf_femitxyz(F,ex->line,BC_FIELD,loc,ex->x,ex->y);
		} break;
		case NODE_NUMBER: {
			int yy = ARRAY_GROW(F->M->numbers,1);
			F->M->numbers[yy] = ex->number;
			elf_femitxy(F,ex->line,BC_LOADNUM,loc,yy);
		} break;
		case NODE_INTEGER: {
			int yy = ARRAY_GROW(F->M->integers,1);
			F->M->integers[yy] = ex->integer;
			elf_femitxy(F,ex->line,BC_LOADINT,loc,yy);
		} break;
		default: ;
	}
	ex->kind=NODE_LOCAL;
	ex->x=loc;
}


void elf_flocalize(elFileState *F, elFileExpr *ex) {
	if (ex->kind!=NODE_LOCAL) {
		elf_fexpr2reg(F,ex,elf_fregalloc(F));
	}
}


/* Functions
*/
void elf_fbeginfunction(elFileState *fs, elFileFnState *fn, char *line) {
	fn->enclosing = fs->fn;
	fn->entities = fs->nentities;
	fn->bytes = fs->M->nbytes;
	fn->line = line;
	fn->yj = 0;
	/* todo: "begin_block" requires fn to be set
	for xmemory, can xmemory simply be in the
	file state instead? */
	fs->fn = fn;
	fn->entry_block = elf_Fbeginblock(fs,0);

	/* allocate 'this' register, which is always the
	first local (0) and the first argument.
	* all functions have a 'this' ('context') register...
	REFERENCED: to avoid getting "unreferenced" warnings,
	ASSIGNED: to prevent "unassigned" warnings
	CONSTANT: to prevent reassignment */
	elFileExpr expr = {0};
	elf_fnewlentity(fs, line, &expr, "this"
	, ENTITY_PARAMETER|ENTITY_ASSIGNED|ENTITY_CONSTANT|ENTITY_REFERENCED);
}


void elf_fclosefunction(elFileState *fs) {
	elModule *M = fs->M;
	elFileFnState *fn = fs->fn;
	elFileline line = fs->this_token.line;
	/* patch all the yield jumps to end of
	function (the leave instruction)
	todo: rename 'yj' to be clearer */
	elf_emitter_patch_jumplist(fs,fn->yj);
	ARRAY_DELETE(fn->yj);
	fn->yj = 0;
	/* finally, return control flow... */
	elf_emitter_add_byteop(fs,line,BC_LEAVE,0);
	/* close the block for this function */
	elf_Fcloseblock(fs);
	/* ensure the block was the right one... */
	elASSERT(fn->entry_block == fs->level);
	/* ensure all the entities were closed
	properly */
	elASSERT(fs->nentities == fn->entities);
	/* enclosing function becomes active
	now */
	fs->fn = fn->enclosing;
}


void elf_fcallargs(elFileState *F) {
	elFileExpr expr={0};
	if (elf_ftesttok(F,TK_CURLY_LEFT)) {
	} else if (elf_fmaytok(F,TK_PAREN_LEFT)) {
		if (!elf_ftesttok(F,TK_PAREN_RIGHT)) do {
			if (elf_ftesttok(F,TK_PAREN_LEFT)) {
				elf_fcallargs(F);
			} else {
				elf_fexpr(F,&expr,0,elf_fregalloc(F));
			}
		} while (elf_fmaytok(F,TK_COMMA));
		elf_fgettok(F,TK_PAREN_RIGHT);
	}
}


void elf_funary(elFileState *F, elFileExpr *expr, int flags, int reg) {
	elToken tok = F->this_token;
	expr->line=tok.line;
	switch (tok.type) {
		case TK_WORD: {
			elf_flextok(F);
			elf_fgetname(F,tok.line,expr,tok.text,1);
		} break;
		/* these could never be l-values, but because
		we can't know what the expression is going
		to be and preemptively allocate a register
		we just have to store it... */
		case TK_NUMBER: {
			elf_flextok(F);
			expr->kind=NODE_NUMBER;
			expr->number=tok.number;
		} break;
		case TK_INTEGER: {
			elf_flextok(F);
			expr->kind=NODE_INTEGER;
			expr->integer=tok.integer;
		} break;
	}
	if (reg!=-1) {
		elf_fexpr2reg(F,expr,reg);
	}
}


void elf_fsubexpr(elFileState *F, elFileExpr *x, int flags, int reg, int limit) {
	int mem; char *line; elFileExpr y={0};
	elf_funary(F,x,flags,reg);
	for (mem=elf_fgetmem(F);;elf_fsetmem(F,mem)) {
		if (elf_fmaytok(F,TK_ADD)) { line=F->last_token.line;
			elf_flocalize(F,x);
			elf_funary(F,&y,flags,elf_fregalloc(F));
			if (reg!=-1) {
				elf_femitxyz(F,line,BC_ADD,reg,x->x,y.x);
			} else {
				/* warning, unused expression */
			}
			x->kind=NODE_LOCAL;
			x->x=reg;
		} else break;
	}
}


void elf_fexpr(elFileState *F, elFileExpr *expr, int flags, int loc) {
	elf_fsubexpr(F,expr,flags,loc,-1);
}


void elf_fassign(elFileState *F) {
	elRegId state=elf_fgetmem(F);
	elFileExpr x={0},y={0};
	elFileline line;
	elf_fexpr(F,&x,0,-1);
	if (elf_fmaytok(F,TK_ASSIGN)) { line=F->last_token.line;
		int loc=x.x;
		if (x.kind!=NODE_LOCAL) {
			loc=elf_fregalloc(F);
		}
		elf_fexpr(F,0,0,loc);
		if (x.kind==NODE_LOCAL) {
			/* already handled this */
		} else if (x.kind==NODE_GLOBAL) {
			elf_femitxy(F,line,BC_SETGLOBAL,x.x,loc);
		} else elNOCODE;
	} else {
		elf_flocalize(F,&x);
	}
	elf_fsetmem(F,state);
}


void elf_Fstat(elFileState *F) {
	elFileFnState *fn = F->fn;
	elToken tok = F->this_token;

	elFileBlock *bl = elf_Fgetblock(F,-1);
	if (bl->flags & BLOCK_ENDED) {
		elf_fdialog(F,tok.line,"warning: unreachable statement");
	}
	switch (tok.type) {
		case TK_LET: {
			elf_flextok(F);
			do {
				if (elf_ftesttok(F,TK_LET)) {
					elf_fdialog(F,F->last_token.line,"invalid declaration, expected next declarator's name after ',' instead got 'let'");
					elf_fdialog(F,F->this_token.line,"invalid declaration, 'let' after comma");
					elf_Rthrow(F->R,0,"syntax error: invalid declaration");
				}
				elToken name = elf_fgettok(F,TK_WORD);
				elFileExpr expr = {0};
				elf_fnewlentity(F,name.line,&expr,name.text,0);
				// elf_fassign(F,x);
			} while (elf_fmaytok(F,TK_COMMA));
		} break;
		default: {
			elf_fassign(F);
		} break;
	}
}




#if 0
elNodeId elf_make_call_pf_node(elFileState *fs, elFileline line, elNodeId *args) {
	elNodeId fn = elf_get_global_entity_node(fs,line,"elf.pf");
	return elf_make_call_node(fs,line,fn,args);
}


elNodeId elf_make_set_metatable_node(elFileState *fs, elFileline line, elNodeId table, elNodeId meta_table) {

	elNodeId fn = elf_get_global_entity_node(fs,line,"elf.set_object_metatable");

	elNodeId *z = 0;
	ARRAY_ADD(z,table);
	ARRAY_ADD(z,meta_table);

	return elf_make_call_node(fs,line,fn,z);
}


elNodeId elf_Ftable(elFileState *fs);


/* A call expression is of the form:
	'x()' or 'x{}'

	Where '{x}{}' is translated to 'x({})'
*/
elNodeId *elf_fcallargs(elFileState *fs) {

	elNodeId *z = 0;

	if (elf_ftesttok(fs,TK_CURLY_LEFT)) {
		elNodeId x = elf_Zlocalize(fs,elf_Ftable(fs));
		ARRAY_ADD(z,x);
	} else
	if (elf_fmaytok(fs,TK_PAREN_LEFT)) {
		/* todo: here we should actually ensure we
		load the arguments in the proper call order,
		to avoid having to do double work */
		if (!elf_ftesttok(fs,TK_PAREN_RIGHT)) do {

			elFileExpr expr = {0};
			elNodeId x = elf_fexpr(fs,&expr,0);

			if (x == NO_NODE) break;

			/* Handle multi nodes, simply unpack them
			such that:
				x((1,2,3)) ::= x(1,2,3)
			*/
			if (elf_Ngetkind(fs,x) == NODE_MULTI) {
				elNodeId *n = elf_Nget(fs,x).z;
				FOR_ARRAY(i,n) {
					ARRAY_ADD(z,elf_Zlocalize(fs,n[i]));
				}
			} else {
				ARRAY_ADD(z,elf_Zlocalize(fs,x));
			}
		} while (elf_fmaytok(fs,TK_COMMA));
		elf_fgettok(fs,TK_PAREN_RIGHT);
	}
	return z;
}


elNodeId *elf_Fcallargsorexpr(elFileState *fs) {
	/* x or ( x { , x } ) */
	elNodeId *z = 0;
	if (elf_fmaytok(fs,TK_PAREN_LEFT)) {
		elNOCODE; /* todo: */
	} else {
		elFileExpr expr = {0};
		elNodeId x = elf_fexpr(fs,&expr,0);
		if (x != NO_NODE) ARRAY_ADD(z,x);
	}
	return z;
}


elTokenType elf_tok2binop(elToken tk) {
	/* could be done in the lexer */
	if (tk.type != TK_WORD)  return  tk.type;
	if (!strcmp(tk.text,"and")) return  TK_LOG_AND;
	if (!strcmp(tk.text,"or"))  return  TK_LOG_OR;
	if (!strcmp(tk.text,"is"))  return  TK_EQ;
	return TK_WORD;
}


int elf_gettokprec(elTokenType type) {
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


elNodeId elf_fsubexpr(elFileState *fs, elFileExpr *expr, int rank, int flags) {
	elNodeId x,y;
	x = elf_Fpostfix(fs,expr,flags|EXPR_ALLOW_POSTFIX);
	if (x == NO_NODE) return x;

	elRegId mem;
	for (mem = elf_fgetmem(fs);;elf_fsetmem(fs,mem)) {

		elTokenType op = elf_tok2binop(fs->this_token);
		int prio = elf_gettokprec(op);
		/* also breaks when not a binary operator */
		if (prio <= rank) break;// else rank = prio;

		// *= += ...
		if (elf_test_then_token(fs,TK_ASSIGN))  {
			/* todo: issue a syntax error if expression
			level > 0... For instance, (i += 1), which is
			within parenthesis, is an error... */
			break;
		}
		elToken tk = elf_flextok(fs);

		/* left operand is localized before the right operand
		is loaded, otherwise right operand's possible
		intermediate values overwrite left's... */
		x = elf_Zlocalize(fs,x);

		y = elf_Fpostfix(fs,expr,flags|EXPR_ALLOW_POSTFIX);
		if (y == NO_NODE) return x;

		y = elf_Zlocalize(fs,y);

		expr->kind = elf_token_to_node(op);
		x = elf_binary_node(fs,tk.line,expr->kind,NT_ANY,x,y);
	}
	return x;
}


elNodeId elf_parse_default_expr(elFileState *fs) {
	elToken tk = elf_fgettok(fs,TK_DEFAULT);

	/* First load the default value and localize it, the register
	where this value resides is the result and the new default
	register which could be possibly overwritten. */
	elFileExpr expr = {0};
	elRegId default_register = elf_Zlocalize2reg(fs,tk.line,elf_fexpr(fs,&expr,0));

	elf_fgettok(fs,TK_QMARK);
	if (elf_ftesttok(fs,TK_CURLY_LEFT)) {
		elRegId last_register = fs->default_register;
		fs->default_register = default_register;

		/* any leave instruction we encounter will
		check the default register, if not -1, which is the
		case it will instead write to it instead of actually
		leaving the function */
		elf_Fstat(fs);

		fs->default_register = last_register;
	} else {
		/* if this is not a statement, we just write to
		the default register ourselves */
		elf_Zstore(fs,tk.line,elf_Nlocal(fs,tk.line,default_register),elf_fexpr(fs,&expr,0));
	}

	return elf_Nlocal(fs,tk.line,default_register);
}




/* Named functions aren't a thing for this
language... at least for now... */
elNodeId elf_fsloadfunction(elFileState *fs) {
	elToken tk = elf_fgettok(fs,TK_FUN);

	/* emit a jump instruction to skip this function's
	code, because all code is allocated within the
	same memory region (byte buffer_ we need to skip
	over blocks which are not meant to be executed
	just yet */
	elByteId fj = elf_emit_jump(fs,tk.line,-1);

	/* begin function allocates 'this' parameter and entity */
	elFileFnState fn = {0};
	elf_fbeginfunction(fs,&fn,tk.line);


	int arity = 1;

	/* start loading parameter list, each parameter also
	becomes a local register */
	elf_fgettok(fs,TK_PAREN_LEFT);

	if (!elf_ftesttok(fs,TK_PAREN_RIGHT)) do {
		elToken name = elf_fgettok(fs,TK_WORD);

		elFileExpr expr = {0};
		elf_fnewlentity(fs, name.line, &expr, name.text
		, ENTITY_PARAMETER|ENTITY_ASSIGNED);

		arity += 1;
	} while (elf_fmaytok(fs,TK_COMMA));

	if (!elf_ftesttok(fs,TK_PAREN_RIGHT)) {
		elf_fdialog(fs,0,"did you miss a ','?");
	}
	elf_fgettok(fs,TK_PAREN_RIGHT);

	/* Now this '?' token is optional */
	elf_fmaytok(fs,TK_QMARK);

	/* Now parse the function's body */
	// ? { .. }
	if (elf_ftesttok(fs,TK_CURLY_LEFT)) {
		elf_Fstat(fs);
		/* By default we emit a 'leave this' instruction,
		because this tends to be more convenient... */
		/* todo: only emit the yield if this block
		wasn't terminated by another leave instruction,
		otherwise this is wasteful */
		elf_Zyield(fs,tk.line,elf_Nlocal(fs,tk.line,0));
	} else {
		elFileExpr expr = {0};
		elf_Zyield(fs,tk.line,elf_fexpr(fs,&expr,0));
	}

	elf_fclosefunction(fs);
	/* now finally patch the jump-over jump
	to resume control flow... */
	elf_emitter_patch_jump(fs,fj);
	/* create a new prototype and add this function
	to the list of prototypes */
	elFileProto fp = {0};
	/* todo: implement this */
	// fp.parent  = fn.enclosing
	fp.arity	  = arity;
	fp.y 		  = fn.nyield;
	fp.nlocals = fn.nlocals;
	fp.nvalues = ARRAY_LENGTH(fn.enclosure);
	fp.bytes   = fn.bytes;
	fp.nbytes  = fs->M->nbytes - fn.bytes;

	int f = elf_add_proto(fs->M,fp);

	/* Now iterate over all the enclosed or captured
	entities, these are the locals of this function
	that we accessed from inside the function we
	just loaded.
	Now they are accessible to us directly, so just
	convert them to registers and pass them in
	to the closure instruction...
	* We don't support capturing locals from outside
	of our enclosing function's scope. */
	elNodeId *z = 0;
	FOR_ARRAY(i,fn.enclosure) {
		elFileEntity entity = fs->entities[fn.enclosure[i]];
		ARRAY_ADD(z,elf_Nlocal(fs,entity.line,entity.slot));
	}

	/* the closure node, which translates to a 'new-closure'
	instruction, takes the id of the prototype, and a set
	of closure arguments, which are the locals we captured */
	return elf_Nclosure(fs,tk.line,f,z);
}


/*
** Completes the expression line, possibly a statement.
** Takes the lhs expression, if not followed by a
** statement operator, such as '=' then it simply
** emits code to evaluate the expression.
*/
void elf_fassign(elFileState *fs, elNodeId lhs) {
	if (lhs == NO_NODE) {
		return;
	}

	elToken tk = fs->tk;
	elRegId mem = elf_fgetmem(fs);

	elNode node = elf_Nget(fs,lhs);
	elFileline line = node.line;

	elNodeId x = lhs; // elf_emit_desugar_range_expr(fs,lhs,0);

	if (elf_fmaytok(fs,TK_ASSIGN)) {
		elNodeId y;

		elFileExpr expr = {0};
		y = elf_fexpr(fs,&expr,0);

		elf_check_assign(fs,tk.line,x,y);
		elf_Zstore(fs,tk.line,x,y);

		elf_fsetmem(fs,mem);

	} else if (elf_fmaytok(fs,TK_NIL_ASSIGN)) {
		elFileExpr expr = {0};
		elNodeId y = elf_fexpr(fs,&expr,0);
		elFileExpr js = {0};
		/* todo: todo could we optimize this... */
		elf_emit_jump_if_not_nil(fs,tk.line,&js,x);
		elf_check_assign(fs,tk.line,x,y);
		elf_Zstore(fs,tk.line,x,y);
		elf_emitter_patch_jumplist(fs,js.f);
	} else {
		/* the lhs of an assignment is an expression,
		and it is parsed by the expression parser.
		the expression parser checks whether there's
		an operator to make a binary expression however,
		if the operator is followed by '=' then it quits,
		so if we get here and we see an operator it's
		guaranteed to be a '{x}=' assignment. */
		if (elf_gettokprec(tk.type) > 0) {
			elNodeId x = elf_emit_desugar_range_expr(fs,lhs,0);
			/* op is tk */
			elToken op = elf_flextok(fs);
			elf_fgettok(fs,TK_ASSIGN);
			/* todo: optimization! */
			elFileExpr expr = {0};
			elNodeId y = elf_fexpr(fs,&expr,0);

			y = elf_binary_node(fs,op.line,elf_token_to_node(op.type),elf_get_node_type(fs,x),x,y);

			elf_check_assign(fs,fs->last_token.line,x,y);
			elf_Zstore(fs,op.line,x,y);
		} else {
			// elRegId memory = elf_fgetmem(fs);
			elf_Zlocalize2reg(fs,NO_LINE,lhs);
			// elf_fsetmem(fs,memory);
		}

		elf_fsetmem(fs,mem);
	}

	elf_emit_desugar_range_expr_epilogue(fs,lhs);
	elASSERT(elf_fgetmem(fs) == mem);
}


elNodeId elf_Ftable(elFileState *fs) {
	/* todo: remove the "table-node" this is
	all a bunch of sugar coating */
	elToken tk = fs->tk;
	elf_fgettok(fs,TK_CURLY_LEFT);
	elNodeId table = elf_make_table_node(fs,tk.line,0);

	#if 0
	elNodeId *z = 0;
	int index = 0;
	while (!elf_term_token(fs,TK_CURLY_RIGHT)) {
		tk = fs->tk;
		elNodeId key;
		if (elf_ftesttok(fs,TK_WORD) && elf_test_then_token(fs,TK_ASSIGN)) {
			elf_flextok(fs);
			key = elf_Nstring(fs,tk.line,tk.text);
		} else {
			elFileExpr expr = {0};
			key = elf_fexpr(fs,&expr,0);
		}
		if (elf_check_expr(fs,fs->this_token.line,key)) {
			break;
		}
		if (elf_fmaytok(fs,TK_ASSIGN)) {
			elFileExpr expr = {0};
			elNodeId val = elf_fexpr(fs,&expr,0);
			elNodeId fld = elf_Ngetfield(fs,fs->last_token.line,table,key);
			elNodeId f = elf_make_load_node(fs,fs->last_token.line,fld,val);
			ARRAY_ADD(z,f);
		} else {
			elNodeId val = key;
			elNodeId ii = elf_make_integer_node(fs,fs->last_token.line,index ++);
			elNodeId f = elf_make_load_node(fs,fs->last_token.line,elf_make_index_node(fs,fs->last_token.line,table,ii),val);
			ARRAY_ADD(z,f);
		}
		if (elf_fmaytok(fs,TK_COMMA)) {
			continue;
		}
	}
	fs->nodes[table].z = z;
	#endif

	elf_fgettok(fs,TK_CURLY_RIGHT);
	return table;
}


elNodeId elf_fexpr(elFileState *F, elFileExpr *expr, int flags) {
	/* todo: I think it's easier to check whether the token
	is an expression */
	switch (F->this_token.type) {
		case TK_NONE:
		case TK_FOR: case TK_WHILE: case TK_LASTLY:
		case TK_COMMA:
		case TK_PAREN_RIGHT: case TK_CURLY_RIGHT: case TK_SQUARE_RIGHT: {
			return NO_NODE;
		}
	}
	return elf_fsubexpr(F,expr,0,flags);
}


/*  */
elRegId elf_get_loop_register(elFileState *F, elFileline line, int type) {
	// todo: re-add support for specifying which
	// loop you're referring to
	elFileBlock *bl = elf_emitter_get_loop_block(F,-1);
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
		elf_Rthrow(F->R,0,"syntax error: invalid context for loop register macro");
	}
	return reg;
}


/* todo: add support for:
specifing which for loop you're reffering to. */
#if 0
elRegId target_value_register = NO_SLOT;
if (!elf_term_eol_token(fs)) {
	elNodeId value = elf_fexpr(fs,0);
	if (value != NO_NODE) {
		target_value_register = elf_get_node_register(fs,MAKE_NODE_ID(value));
		if (target_value_register < 0) {
			elf_fdialog(fs,elf_get_node_line(fs,value),"invalid value");
		}
	}
}
#endif



elNodeId elf_funary(elFileState *fs, elFileExpr *expr, elBool flags) {
	elNodeId v = NO_NODE;
	elToken tk = fs->this_token;
	switch (tk.type) {
		case TK_M_INDEX: case TK_M_ARRAY: case TK_M_VALUE: {
			elf_flextok(fs);
			// TODO: instead use regular register node but with
			// negative values?
			elRegId reg =
			tk.type == TK_M_ARRAY ? SPECIAL_REGISTER_ARRAY :
			tk.type == TK_M_VALUE ? SPECIAL_REGISTER_VALUE : SPECIAL_REGISTER_INDEX;
			v = elf_make_special_register_node(fs,tk.line,reg);

			expr->kind = NODE_LOCAL;
			expr->x    = elf_get_loop_register(fs,tk.line,reg);

		} break;
		case TK_M_INT: case TK_M_NUM: { elf_flextok(fs);
			elNodeId x = elf_Fpostfix(fs,expr,flags|EXPR_ALLOW_POSTFIX);
			/* todo: make this an intrinsic instruction! */
			char *name = tk.type == TK_M_INT ? "ntoi" : "iton";
			elNodeId fn = elf_get_global_entity_node(fs,tk.line,name);
			elNodeId *z = {0};
			ARRAY_ADD(z,x);
			v = elf_make_call_node(fs,tk.line,fn,z);
		} break;
		case TK_M_REGISTER: { elf_flextok(fs);
			tk = elf_fgettok(fs,TK_WORD);
			elEntityId entity = elf_Fgetentity(fs,tk.line,tk.s,0);
			if (entity == NO_ENTITY) {
				elf_fdialog(fs,tk.line,"'%s': invalid entity (must be a local)",tk.s);
			}
			v = elf_make_integer_node(fs,tk.line,fs->entities[entity].slot);
		} break;
		//
		// 'load' ( <file-name> )
		//
		//  leave elf.loadfile(<file-name>)
		//
		case TK_LOAD: {
			elf_flextok(fs);
			elNodeId *call_args = elf_Fcallargsorexpr(fs);

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
			elf_flextok(fs);
			elNodeId meta_table = elf_Fpostfix(fs,expr,0);
			elNodeId *call_args = elf_fcallargs(fs);

			elNodeId table;
			/* So if the user does something like new Thing {}
			or new Thing({}) the table that was passed in can
			be used as supposed to creating a new one */
			if ((ARRAY_LENGTH(call_args) == 1) && (elf_Ngetkind(fs,call_args[0]) == NODE_TABLE)) {
				table = call_args[0];
			} else {
				/* If the user however, doesn't do this, then we
				create a new table for him */
				table = elf_make_table_node(fs,tk.line,0);
			}

			table = elf_make_set_metatable_node(fs,tk.line,table,meta_table);

			elNodeId meta_field_name = elf_Nstring(fs,tk.line,"__new");
			elNodeId get_meta_field = elf_make_metafield_node(fs,tk.line,table,meta_field_name);
			elNodeId call_new = elf_make_call_node(fs,tk.line,get_meta_field,call_args);

			v = call_new;
		} break;
		/* Empty ranges '..' are interpreted as 0..limit
		of whatever expression. */
		case TK_DOT_DOT: {
			elf_flextok(fs);
			v = elf_binary_node(fs,tk.line,NODE_RANGE,NT_ANY,NO_NODE,NO_NODE);

			expr->kind = NODE_RANGE;
			expr->x    = NO_NODE;
			expr->y    = NO_NODE;
		} break;
		/* todo: this is temporary */
		case TK_DOT: case TK_ELF: {
			char dir[MAX_PATH] = {};
			if (elf_fmaytok(fs,TK_ELF)) {
				strcat(dir,"elf");
				/* remind the user elf is a reserved keyword */
				if (!elf_test_token_inline(fs,TK_DOT)) {
					elf_fdialog(fs,tk.line,"incomplete symbol, expected '.' on the same line as 'elf'. Did you mean to use 'elf'? This is a reserved keyword and it refers to the elf directory.");
				}
			}
			elf_fgettok(fs,TK_DOT);
			do {
				elf_take_token_inline(fs,TK_WORD);
				strcat(dir,".");
				strcat(dir,fs->last_token.s);
			} while (elf_pick_token_inline(fs,TK_DOT));

			elSymbolId x = elf_get_global_symbol(fs->M,elf_new_string(fs->R,dir));
			v = elf_make_global_value_node(fs,tk.line,x);

			expr->kind = NODE_GLOBAL;
			expr->x = x;
		} break;
		case TK_WORD: {
			elf_flextok(fs);
			v = elf_fgetname(fs,tk.line,expr,tk.text,flags);
		} break;
		case TK_SUB: {
			elf_flextok(fs);
			v = elf_fsubexpr(fs,expr,10000,flags);
			v = elf_binary_node(fs,tk.line,NODE_SUB,NT_INT,elf_make_integer_node(fs,tk.line,0),v);
		} break;
		case TK_ADD: {
			elf_flextok(fs);
			v = elf_fsubexpr(fs,expr,10000,flags);
		} break;
		case TK_CURLY_LEFT: {
			v = elf_Ftable(fs);
		} break;
		case TK_PAREN_LEFT: {
			elf_flextok(fs);
			v = elf_fexpr(fs,expr,EXPR_ALLOW_POSTFIX);
			elf_fgettok(fs,TK_PAREN_RIGHT);
			/* allow for empty () */
			if (v != NO_NODE) {
				v = elf_make_group_node(fs,tk.line,v);
				elASSERT(v != NO_NODE);
			}
		} break;
		case TK_FUN: {
			v = elf_fsloadfunction(fs);
		} break;
		case TK_DEFAULT: {
			elf_fdialog(fs,tk.line,"syntax error: default expressions can only be top level");
			elf_Rthrow(fs->R,0,"syntax error: default expressions can only be top level");
		} break;
		case TK_NIL: {
			elf_flextok(fs);
			expr->kind = NODE_NIL;
			v = elf_make_nil_node(fs,tk.line);
		} break;
		/* todo: maybe use proper boolean node? */
		case TK_TRUE: case TK_FALSE: {
			elf_flextok(fs);
			expr->kind = NODE_INTEGER;
			expr->integer = tk.type == TK_TRUE;
			v = elf_make_integer_node(fs,tk.line,tk.type == TK_TRUE);
		} break;
		case TK_LETTER: case TK_INTEGER: {
			elf_flextok(fs);
			expr->kind = NODE_INTEGER;
			expr->integer = tk.integer;
			v = elf_make_integer_node(fs,tk.line,tk.i);
		} break;
		case TK_NUMBER: { elf_flextok(fs);
			expr->kind = NODE_NUMBER;
			expr->number = tk.number;
			v = elf_make_number_node(fs,tk.line,tk.n);
		} break;
		case TK_STRING: { elf_flextok(fs);
			expr->kind = NODE_STRING;
			expr->string = tk.string;
			v = elf_Nstring(fs,tk.line,tk.s);
		} break;
		default: {
			elf_fdialog(fs,tk.line,"'%s': unexpected token", elf_tkintel[tk.type].name);
			elf_Rthrow(fs->R,0,"syntax error: unexpected token");
		} break;
	}

	return v;
}


elNodeId elf_Fgetfield(elFileState *F, elFileExpr *expr, elNodeId x) {
	elToken tok = elf_fgettok(F,TK_DOT);
	// elRegId mem = elf_fgetmem(F);
	elNodeId y;
	if (elf_fmaytok(F,TK_PAREN_LEFT)) {
		/* desugar {x}.(x,y) -> ({x}.x, {x}.y) */
		elNodeId *z = {0};
		do {
			elToken name = elf_fgettok(F,TK_WORD);
			y = elf_Ngetfield(F,tok.line,x,elf_Nstring(F,name.line,name.text));
			ARRAY_ADD(z,y);
		} while (elf_fmaytok(F,TK_COMMA));
		x = elf_Nmulti(F,tok.line,z);
		elf_fgettok(F,TK_PAREN_RIGHT);
	} else if (elf_fmaytok(F,TK_CURLY_LEFT)) {
		// table.{x,y}
		elNOCODE;
	} else {
		elToken name = elf_fgettok(F,TK_WORD);
		x = elf_Zlocalize(F,x);
		y = elf_Nstring(F,name.line,name.text);
		x = elf_Ngetfield(F,tok.line,x,y);
	}
	// elf_fsetmem(F,mem);
	return x;
}

/*
	Postfix expressions are of the forms:
	x{...}, x(...), x[...], x.(...), x:(...), x.[...], x:[...], x...
*/
elNodeId elf_Fpostfix(elFileState *F, elFileExpr *expr, elBool flags) {
	elASSERT(flags & EXPR_ALLOW_POSTFIX);
	elNodeId x = elf_Funary(F,expr,flags);
	/* ensure we don't parse a postfix out of line */
	// elRegId mem;
	// mem = elf_fgetmem(F); ; elf_fsetmem(F,mem)e
	while (!elf_term_eol_token(F)) {
		switch (F->this_token.type) {
			case TK_DOT: {
				x = elf_Fgetfield(F,expr,x);
			} break;
			default: goto esc;
		}
	}
	// }
	esc:
	return x;
}

#if 0
/* todo: Should we make this nil safe, so [0,0] shouldn't
			fail if item at 0 is nil?  */
			case TK_SQUARE_LEFT: {
				elf_fgettok(fs,TK_SQUARE_LEFT);
				do {
					elFileExpr expr = {0};
					elNodeId index = elf_fexpr(fs,&expr,0);

					if (index == NO_NODE) break;


					/* Unwrap multi-nodes, so that:
					registry[location.(y,x)] -> registry[location.y,location.x] */
					if (elf_Ngetkind(fs,index) == NODE_MULTI) {
						elNodeId *z = elf_Nget(fs,index).z;
						FOR_ARRAY(i,z) {
							v = elf_make_index_node(fs,tk.line,v,z[i]);
						}
					} else
					if (elf_Ngetkind(fs,index) == NODE_RANGE) {
						v = elf_make_ranged_index_node(fs,tk.line,v,index);
					} else v = elf_make_index_node(fs,tk.line,v,index);

					/* todo: this is silly, this is just an
					inner multi expressions, make multi
					expressions be regular 'comma' expressions
					instead */
				} while(elf_fmaytok(fs,TK_COMMA));
				elf_fgettok(fs,TK_SQUARE_RIGHT);
			} break;

			case TK_COLON: {
				elf_flextok(fs);
				elToken n = elf_fgettok(fs,TK_WORD);
				elNodeId y = elf_Nstring(fs,n.line,n.s);
				v = elf_make_metafield_node(fs,tk.line,v,y);
			} break;
			case TK_CURLY_LEFT: case TK_PAREN_LEFT: {
				v = elf_Zlocalize(fs,v);
				elNodeId *z = elf_fcallargs(fs);
				v = elf_make_call_node(fs,tk.line,v,z);
			} break;
#endif
void elf_Fstat(elFileState *fs) {
	elToken tk = fs->tk;
	elFileFnState *fn = fs->fn;

	elFileBlock *bl = elf_Fgetblock(fs,-1);
	if (bl->flags & BLOCK_ENDED) {
		elf_fdialog(fs,tk.line,"warning: unreachable statement");
	}

	elRegId mem = elf_fgetmem(fs);

	switch (tk.type) {
		case TK_THEN: case TK_ELSE: case TK_ELIF: break;
		case TK_LASTLY: case TK_FINALLY: {
			elf_flextok(fs);
			if (tk.type == TK_FINALLY) {
				elf_fdialog(fs,tk.line,"warning: please consider using 'lastly' instead, 'finally' could change semantics in the future");
			}
			elf_Fbegindelayblock(fs,tk.line);
			elf_Fstat(fs);
			elf_Fclosedelayblock(fs,tk.line);
			elASSERT(elf_fgetmem(fs) == mem);
		} break;
		case TK_IF: case TK_IFF: {
			elf_flextok(fs);

			elFileExpr expr = {0};
			elNodeId x = elf_fexpr(fs,&expr,0);

			elf_fgettok(fs,TK_QMARK);
			elf_Fbeginblock(fs,0);

			elSelectState s = {0};
			elf_Fbeginif(fs,tk.line,&s,x,tk.type==TK_IFF?L_IFF:L_IF);

			elf_Fstat(fs);
			while (!elf_ftesttok(fs,TK_NONE)) {
				if (elf_fmaytok(fs,TK_ELIF)) {
					elf_Fbeginblock(fs,0);
					elFileExpr expr = {0};
					x = elf_fexpr(fs,&expr,0);
					elf_fgettok(fs,TK_QMARK);
					elf_Faddelifclause(fs,fs->last_token.line,&s,x);
					elf_Fstat(fs);
					elf_Fcloseblock(fs);
				} else if (elf_fmaytok(fs,TK_THEN)) {
					elf_Fbeginblock(fs,0);
					elf_Faddthenclause(fs,fs->last_token.line,&s);
					elf_Fstat(fs);
					elf_Fcloseblock(fs);
				} else if (elf_fmaytok(fs,TK_ELSE)) {
					elf_Fbeginblock(fs,0);
					elf_Faddelseclause(fs,fs->last_token.line,&s);
					elf_Fstat(fs);
					elf_Fcloseblock(fs);
				} else break;
			}
			elf_Fcloseif(fs,fs->last_token.line,&s);
			elf_Fcloseblock(fs);
			elASSERT(elf_fgetmem(fs) == mem);
		} break;
		case TK_LET: {
			elf_flextok(fs);
			do {
				if (elf_ftesttok(fs,TK_LET)) {
					elf_fdialog(fs,fs->last_token.line,"invalid declaration, expected next declarator's name after ',' instead got 'let'");
					elf_fdialog(fs,fs->this_token.line,"invalid declaration, 'let' after comma");
					elf_Rthrow(fs->R,0,"syntax error: invalid declaration");
				}

				elToken name = elf_fgettok(fs,TK_WORD);

				elFileExpr expr = {0};
				elNodeId x = elf_fnewlentity(fs,name.line,&expr,name.text,0);

				elf_fassign(fs,x);
			} while (elf_fmaytok(fs,TK_COMMA));
		} break;
		case TK_LEAVE: {
			elf_flextok(fs);
			elFileExpr expr = {0};
			elNodeId x = elf_fexpr(fs,&expr,0);
			/* If we are within a default expression then 'leave'
			actually writes to the default register */
			if (fs->default_register != NO_SLOT) {
				elf_Zstore(fs,tk.line,elf_Nlocal(fs,NO_LINE,fs->default_register),x);
			} else {
				elf_Zyield(fs,tk.line,x);
				elASSERT(elf_fgetmem(fs) == mem);
			}
		} break;
		case TK_BREAK: case TK_CONTINUE: { elf_flextok(fs);
			elRegId target_value_register = NO_SLOT;
			if (!elf_term_eol_token(fs)) {

				elFileExpr expr = {0};
				elNodeId value = elf_fexpr(fs,&expr,0);

				if (value != NO_NODE) {
					target_value_register = elf_get_node_register(fs,MAKE_NODE_ID(value));
					if (target_value_register < 0) {
						elf_fdialog(fs,elf_get_node_line(fs,value),"invalid value");
					}
				}
			}
			if (tk.type == TK_CONTINUE) elf_emit_continue(fs,tk.line,target_value_register);
			else elf_emit_break(fs,tk.line,target_value_register);
		} break;
		case TK_WHILE: { elf_flextok(fs);
			elf_Fbeginblock(fs,BLOCK_LOOP); {
				elFileExpr expr = {0};
				elNodeId x = elf_fexpr(fs,&expr,0);
				elf_fgettok(fs,TK_QMARK);
				elf_Fbeginwhileloop(fs,tk.line,x); {
					elf_Fstat(fs);
				} elf_Fclosewhileloop(fs,tk.line);
				/* pedantic error checking */
				elASSERT(elf_fgetmem(fs) == mem);
			} elf_Fcloseblock(fs);
		} break;
		case TK_DO: { elf_flextok(fs);
			elNodeId x;
			elf_Fbeginblock(fs,BLOCK_LOOP); {
				elf_Fbegindowhileloop(fs,tk.line); {
					elf_Fstat(fs);
					elf_fgettok(fs,TK_WHILE);
					elFileExpr expr = {0};
					x = elf_fexpr(fs,&expr,0);
				} elf_Fclosedowhileloop(fs,tk.line,x);
				/* pedantic error checking */
				elASSERT(elf_fgetmem(fs) == mem);
			} elf_Fcloseblock(fs);
		} break;
		case TK_CURLY_LEFT: { elf_flextok(fs);
			elf_Fbeginblock(fs,0); {
				while (!elf_term_token(fs,TK_CURLY_RIGHT)) {
					elf_Fstat(fs);
				}
				elf_fgettok(fs,TK_CURLY_RIGHT);
			} elf_Fcloseblock(fs);
		} break;
		default: {
			elFileExpr expr = {0};
			elNodeId x = elf_fexpr(fs,&expr,0);
			if (x != NO_NODE) {
				elf_fassign(fs,x);
			} else {
				elf_fdialog(fs,tk.line,"invalid statement");
				elf_Rthrow(fs->R,NO_BYTE,"syntax error: invalid statement");
			}
			elf_fsetmem(fs,mem);
			// if (elf_fgetmem(fs) != mem) {
			// 	elf_fdialog(fs,tk.line,"internal error: invalid memory state!");
			// }
			// elASSERT(elf_fgetmem(fs) == mem);
		} break;
	}
}



/*

The for loop syntax justification:

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
void elf_Fforloop(elFileState *fs) {
	elToken tk = elf_fgettok(fs,TK_FOR);

	elf_flextok(fs);
	elToken name = elf_fgettok(fs,TK_WORD);

	elf_fgettok(fs,TK_ASSIGN);

	elNodeId *z = 0;
	do {
		elFileExpr expr = {0};
		elNodeId y = elf_fexpr(fs,&expr,0);


		if (elf_Ngetkind(fs,y) == NODE_MULTI) {
			elNodeId *yz = elf_Nget(fs,y).z;
			for (int i = 0; i < ARRAY_LENGTH(yz); i += 1) {
				ARRAY_ADD(z,yz[i]);
			}
		} else {
			ARRAY_ADD(z,y);
		}
	} while (elf_fmaytok(fs,TK_COMMA));

	elf_fgettok(fs,TK_QMARK);

	elByteId block_head = NO_BYTE;
	elByteId block_tail = NO_BYTE;

			/* todo: why not create this per
			expression instead */
	elBlockId block = elf_Fbeginblock(fs,BLOCK_LOOP);

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
	const elNodeId value = elf_fnewlentity(fs, name.line, &expr, name.text
			/* todo: remove REFERENCED, instead allow the user to not have to
			specify the name */
	,	ENTITY_REFERENCED|ENTITY_ASSIGNED|ENTITY_FORLOOP);


	const elRegId value_register = elf_Nget(fs,value).x;


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

		if (elf_Ngetkind(fs,y) == NODE_RANGE_INDEX) {
			array = elf_Nget(fs,y).x;
			array_register = elf_Zlocalize2reg(fs,NO_LINE,array);

			y = elf_Nget(fs,y).y;
			elASSERT(elf_Ngetkind(fs,y) == NODE_RANGE);
		}
		if (elf_Ngetkind(fs,y) == NODE_RANGE) {
			elNodeId lo = elf_Nget(fs,y).x;
			elNodeId hi = elf_Nget(fs,y).y;

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
				index = elf_Nlocal(fs,tk.line,elf_fregalloc2(fs,tk.line,NO_SLOT,NO_NODE));
			}

			elf_emitter_begin_ranged_loop(fs,tk.line,index,lo,hi);

			elf_Fgetblock(fs,block)->loop.value_register = value_register;
			if (array != NO_NODE) {
				elf_Fgetblock(fs,block)->loop.array_register = array_register;
			}

			if (array != NO_NODE) {
				elNodeId *z = {0};
				ARRAY_ADD(z,index);

						/* todo: make this neater */
				elf_Zstore(fs,name.line,value,
				elf_make_call_metafield_node(fs,tk.line,array,z,"idx"));
						// elf_make_call_node(fs,tk.line,
						// elf_make_metafield_node(fs,tk.line,array,
						// elf_Nstring(fs,tk.line,"idx")),z));
			}

						/* todo: could be neater */
			if (i == 0) {
				block_head = elf_get_last_byteid(fs);
				elf_Fstat(fs);
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
			elf_Zstore(fs,tk.line,value,y);


			if (i == 0) {
				block_head = elf_get_last_byteid(fs);
				elf_Fstat(fs);
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
			elFileBlock *loop = elf_Fgetblock(fs,block);

			elf_emitter_patch_jumplist(fs,loop->loop.true_jumps);
			ARRAY_DELETE(loop->loop.true_jumps);
			loop->loop.true_jumps = 0;
		}
	}
	elf_Fcloseblock(fs);
}

#endif