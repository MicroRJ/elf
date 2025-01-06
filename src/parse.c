/*
** parse.c
** See Copyright Notice In elf.h
*/

#include "tree.c"


static treeID parse_unary(Parser *parser, int flags);
static treeID parse_expr(Parser *parser, int flags);
static treeID parse_table(Parser *parser);
static treeID parse_subexpr(Parser *parser, int flags, int rank);
static treeID parse_postfix(Parser *parser, int flags);
static treeID parse_block(Parser *parser);
static int parse_stat(Parser *parser);
static int tok2tree(int tok);
static int tok2prec(int tok);

//todo:who cares bout the parser,
//don't pass it in as a pointer....
static treeID parse(Parser *parser, elf_State *R, char *name, char *text) {
	parser->R = R;
	parser->name=name;
	parser->text=text;
	parser->line_pos=text;
	parser->line_num=1;
	parser->pos=text;
	get_tok(parser);
	get_tok(parser);

	treeID dummy;

	dummy=new_tree(parser,TREE_FUNCTION,parser->tok.line);
	parser->enclosing=dummy;

	while (parser->tok.type!=TK_NONE) {
		if(!parse_stat(parser))break;
	}
	return tree_block(parser,parser->tok.line,parser->block.body);
}

static void check_tree(Parser *parser, Source line, treeID id) {
	if (id == NO_TREE){
		parser_dialog(parser,line,"invalid expression");
	}
}

//todo:make end of line be a new token?
static bool eof_or_tok(Parser *parser, int type) {
	return parser->tok.type == TK_NONE || parser->tok.type == type;
}
static bool eof_or_eol_tok(Parser *parser) {
	return parser->tok.type == TK_NONE || parser->tok_prev.eol == 1;
}


static bool peek_tok(Parser *parser, int type) {
	return parser->tok.type == type;
}
static bool pick_tok(Parser *parser, int k) {
	return peek_tok(parser,k) && (get_tok(parser), 1);
}
static bool peek_tok_inl(Parser *parser, int k) {
	return peek_tok(parser,k) && parser->tok_prev.eol != 1;
}
static bool pick_tok_inl(Parser *parser, int k) {
	return peek_tok_inl(parser,k) && (get_tok(parser), 1);
}

static tokenT get_token_inline(Parser *parser, int k) {
	tokenT tok = parser->tok;
	if (!pick_tok_inl(parser,k)) {
		parser_dialog(parser,tok.line,"expected '%s'\n",tok2inf[k].name);
	}
	return tok;
}

static tokenT take_tok(Parser *parser, int k) {
	tokenT tok = parser->tok;
	if (!pick_tok(parser,k)) {
		parser_dialog(parser,parser->tok.line,"expected '%s'\n",tok2inf[k].name);
	}
	return tok;
}

static void begin_scope(Parser *parser) {
	parser->scope_stack[parser->scope_index ++] = parser->entity_index;
	parser->scope ++;
}
static void close_scope(Parser *parser) {
	parser->entity_index = parser->scope_stack[-- parser->scope_index];
	parser->scope --;
}
static void block_add(Parser *parser, treeID id){
	ARRAY_ADD(parser->block.body,id);
}
static void begin_block(Parser *parser) {
	parser->block_stack[parser->block_index ++] = parser->block;
	parser->block.body=0;
	begin_scope(parser);
}
static treeID close_block(Parser *parser) {
	close_scope(parser);
	Block block;
	block=parser->block;
	parser->block = parser->block_stack[-- parser->block_index];
	return tree_block(parser,parser->tok.line,block.body);
}

static entityID identify(Parser *parser, char *name, bool enclose) {
	// IR_Function *fn = parser->fn;
	entityID id;
	for (id = parser->entity_index-1; id > -1; -- id) {
		if (text_eq(parser->entities[id].name,name)) {
			// if ((id < fn->entities) && (enclose)) {
			// 	enclose_entity(parser,fn,(entityID2){id});
			// }
			parser->entities[id].status |= ENTITY_REFERENCED;
			return id;
		}
	}
	return NO_ENTITY;
}
static entityID parser_bind(Parser *parser, Source line, int flags, char *name, treeID tree) {
	treeID enclosing;
	entityID entity_id;
	entityT entity;

	enclosing=parser->enclosing;
	entity_id=identify(parser,name,0);

	if (entity_id!=NO_ENTITY) {
		entity=parser->entities[entity_id];
		if (entity.kind==ENTITY_DIRECTORY)  {
			parser_dialog(parser,line,"'%s': name is reserved for symbol directory",name);
		}
		if (entity.scope==parser->scope) {
			//note:re-decl
			// __debugbreak();
			parser_dialog(parser,line,"'%s': is already declared",name);
		} else {
			if (entity.scope>=parser->enclosing->expr_fun.scope) {
				parser_dialog(parser,line,"'%s': this declaration shadows another one",name);
			}
		}
	}

	entity_id=parser->entity_index ++;
	//todo:use stack instead
	ARRAY_GROW(parser->entities,parser->entity_index-ARRAY_LENGTH(parser->entities));

	entity.kind = ENTITY_LOCAL;
	entity.status = flags;
	entity.scope = parser->scope;
	entity.tree = tree;
	entity.name = name;
	entity.line = line;
	parser->entities[entity_id]=entity;
	return entity_id;
}

static treeID parse_unary(Parser *parser, bool flags) {
	tokenT tok;
	treeID v,x;

	v=NO_TREE;
	tok=parser->tok;

	switch (tok.type) {
		case TK_CURLY_LEFT: {
			v=parse_table(parser);
		} break;
		case TK_ELF: {
			get_tok(parser);
			//elf is a keyword!
			if (!peek_tok_inl(parser,TK_DOT)) {
				parser_dialog(parser,tok.line,"incomplete symbol, expected '.' on the same line as 'elf'. Did you mean to use 'elf'? This is a reserved keyword and it refers to the elf directory.");
			}

			char dir[MAX_PATH] = {};
			strcat(dir,"elf");

			take_tok(parser,TK_DOT);
			do{
				get_token_inline(parser,TK_WORD);
				strcat(dir,".");
				strcat(dir,parser->tok_prev.text);
			}while(pick_tok_inl(parser,TK_DOT));

			v=tree_global_ref_by_name(parser,tok.line,dir);
		} break;
		case TK_WORD: {
			entityID id;
			entityT entity;
			char *name;

			get_tok(parser);

			name=tok.text;
			id=identify(parser,name,1);

			if (id!=NO_ENTITY) {
				entity=parser->entities[id];
				if (~parser->entities[id].status & ENTITY_ASSIGNED) {
					parser_dialog(parser,tok.line,"warning: usage of possibly unassigned variable");
				}
				treeID enclosing=parser->enclosing;
				if (entity.scope<enclosing->expr_fun.scope) {
					ASSERT(!"FIXME");
					enclosing=enclosing->expr_fun.enclosing;
					if (entity.scope<enclosing->expr_fun.scope) {
						parser_dialog(parser,tok.line,"cannot capture?");
					}
				}else{
					v=entity.tree;
					// tree_local_ref(parser,tok.line,tok.text,entity.tree);
				}
			} else {
				//todo:
				v=tree_global_ref_by_name(parser,tok.line,name);
			}
		} break;
		case TK_PAREN_LEFT: {
		} break;
		case TK_FUN: {
			get_tok(parser);

			treeID *params,param,body,enclosing;
			params=0;

			take_tok(parser,TK_PAREN_LEFT);
			if (!peek_tok(parser,TK_PAREN_RIGHT)) do {
				//todo:
				__debugbreak();
				tokenT name;
				name=take_tok(parser,TK_WORD);

				param=0;
				ARRAY_ADD(params,param);
			} while (pick_tok(parser,TK_COMMA));

			if (!peek_tok(parser,TK_PAREN_RIGHT)) {
				parser_dialog(parser,0,"did you miss a ',' ?");
			}
			take_tok(parser,TK_PAREN_RIGHT);
			pick_tok(parser,TK_QMARK);

			enclosing=parser->enclosing;

			// function isn't something we require
			// to be a tree we just need closure
			// instruction, and store this separate...
			v=new_tree(parser,TREE_FUNCTION,tok.line);
			v->expr_fun.scope=parser->scope;
			v->expr_fun.enclosing=enclosing;
			v->expr_fun.params=params;
			v->expr_fun.proto=-1;

			parser->enclosing=v;

			// xx parser_bind(parser, line
			// xx , ENTITY_PARAMETER|ENTITY_ASSIGNED|ENTITY_CONSTANT|ENTITY_REFERENCED
			// xx , "this", ir);
			begin_block(parser);
			parse_stat(parser);

			{
				//todo: only if the block doesn't have a return already
				treeID end;
				end=tree_ret(parser,tok.line,end);
				block_add(parser,end);
			}

			body=close_block(parser);


			v->expr_fun.body=body;

			parser->enclosing=enclosing;

			ARRAY_ADD(parser->functions,v);
		} break;
		case TK_NIL: {
			get_tok(parser);
			v=new_tree(parser,EXPR_NIL,tok.line);
		} break;
		case TK_TRUE:{
			get_tok(parser);
			v=new_tree(parser,EXPR_INT,tok.line);
			v->expr_int=1;
		} break;
		case TK_FALSE: {
			get_tok(parser);
			v=new_tree(parser,EXPR_INT,tok.line);
			v->expr_int=0;
		} break;
		case TK_LETTER: case TK_INTEGER: {
			get_tok(parser);
			v=new_tree(parser,EXPR_INT,tok.line);
			v->expr_int=tok.integer;
		} break;
		case TK_NUMBER: {
			get_tok(parser);
			v=new_tree(parser,EXPR_NUM,tok.line);
			v->expr_num=tok.number;
		} break;
		case TK_STRING: {
			get_tok(parser);
			v=new_tree(parser,EXPR_STR,tok.line);
			v->expr_str=tok.text;
		} break;
		default: {
			parser_dialog(parser,tok.line,"'%s': unexpected token", tok2inf[tok.type].name);
			elf_fail(parser->R,0,"syntax error: unexpected token");
		} break;
	}
	return v;
}

static treeID parse_table(Parser *parser) {
	treeID table,key,field,store,value,prev;
	tokenT tok;
	int index;
	treeID *args;

	tok=take_tok(parser,TK_CURLY_LEFT);

	index=0;
	tok=parser->tok;

	table=tree_table(parser,tok.line,0);
	prev=table;

	for (;(tok.type!=TK_NONE)&&(tok.type!=TK_CURLY_RIGHT);tok=parser->tok) {
		value=NO_TREE;
		if ((tok.type==TK_WORD)&&(parser->tok_prox.type==TK_ASSIGN)) {
			tok=get_tok(parser);
			key=tree_str(parser,tok.line,tok.text);
		} else {
			key=value=parse_expr(parser,0);
		}
		tok=parser->tok;
		if (pick_tok(parser,TK_ASSIGN)) {
			value=parse_expr(parser,0);
		} else {
			key=tree_int(parser,tok.line,index++);
		}

		check_tree(parser,tok.line,key);
		check_tree(parser,tok.line,value);
		tok=parser->tok;
		field=tree_field(parser,tok.line,table,key);

		store=tree_store(parser,tok.line,field,value);
		prev->prox=store;
		prev=store;
		// xx ARRAY_ADD(args,store);

		if (pick_tok(parser,TK_COMMA)) {
			continue;
		}
	}
	take_tok(parser,TK_CURLY_RIGHT);

	return table;
}

/* {x} | ( x { ... } ) | { <table-initializer-list> } */
static treeID *parse_call_args(Parser *parser) {
	treeID x;
	treeID *z,*n;
	z=0;
	if (peek_tok(parser,TK_CURLY_LEFT)) {
		x=parse_table(parser);
		ARRAY_ADD(z,x);
	} else if (pick_tok(parser,TK_PAREN_LEFT)) {
		if (!peek_tok(parser,TK_PAREN_RIGHT)) do {
			x=parse_expr(parser,0);
			if(x!=NO_TREE){
				//desugar multi expressions
				if (get_tree_kind(parser,x)==EXPR_MULTI) {
					n=get_tree(parser,x).z;
					FOR_ARRAY(i,n){
						ARRAY_ADD(z,n[i]);
					}
				} else ARRAY_ADD(z,x);
			} else break;
		} while (pick_tok(parser,TK_COMMA));
		take_tok(parser,TK_PAREN_RIGHT);
	} else {
		x=parse_expr(parser,0);
		if (x!=NO_TREE)ARRAY_ADD(z,x);
	}
	return z;
}


static treeID parse_postfix(Parser *parser, int flags) {
	tokenT tok;
	treeID v;

	v=parse_unary(parser,flags);

	while (!eof_or_eol_tok(parser)) {
		tok=parser->tok;

		switch (tok.type) {
#if 0
			case TK_DOT: {
				get_tok(parser);
				// table.(x,y) -> (table.x, table.y)
				if (pick_tok(parser,TK_PAREN_LEFT)) {
					tokenT n;
					treeID x,y,*z;

					z=0;
					do {
						n=take_tok(parser,TK_WORD);
						y=tree_str(parser,n.line,n.text);
						x=tree_field(parser,tok.line,v,y);
						ARRAY_ADD(z,x);
					} while (pick_tok(parser,TK_COMMA));
					v = tree_multi(parser,tok.line,z);
					take_tok(parser,TK_PAREN_RIGHT);
				} else
				// table.{x,y}
				if (pick_tok(parser,TK_CURLY_LEFT)) {
					NO_CODE;
				} else {
					tokenT name;
					treeID field;
					name=take_tok(parser,TK_WORD);
					field=tree_str(parser,name.line,name.text);
					v=tree_field(parser,tk.line,v,field);
				}
			} break;
			/* todo: make this nil safe, so [0,0] shouldn't
			fail if item at 0 is nil  */
			case TK_SQUARE_LEFT: {
				take_tok(parser,TK_SQUARE_LEFT);
				treeID *z;
				treeID index;
				do {
					index=parse_expr(parser,0,0);
					if (index==NO_TREE) break;
					/* registry[location.(y,x)] ->
					registry[location.y,location.x] */
					if (get_tree_kind(parser,index)==IR_MULTI) {
						z=get_tree(parser,index).z;
						FOR_ARRAY(i,z) {
							v=tree_index(parser,tk.line,v,z[i]);
						}
					} else if (get_tree_kind(parser,index)==IR_RANGE) {
						v=tree_ranged_index(parser,tk.line,v,index);
					} else {
						v=tree_index(parser,tk.line,v,index);
					}

					/* todo: this is silly, this is just an
					inner multi expressions, make multi
					expressions be regular 'comma' expressions
					instead */
				} while(pick_tok(parser,TK_COMMA));
				take_tok(parser,TK_SQUARE_RIGHT);
			} break;

			case TK_COLON: {
				tokenT n;
				treeID y;
				get_tok(parser);
				n=take_tok(parser,TK_WORD);
				y=tree_str(parser,n.line,n.text);
				v=tree_metafield(parser,tk.line,v,y);
			} break;
#endif
			case TK_CURLY_LEFT:
			case TK_PAREN_LEFT: {
				treeID *z;
				z=parse_call_args(parser);
				v=tree_call(parser,tok.line,v,z);
			} break;
			default: goto esc;
		}
	}

	esc:;
	return v;
}
//note:if the expression turns out to be an assignment statement,
//then the name expression is returned, the operator isn't parsed.
static treeID parse_subexpr(Parser *parser, int flags, int rank) {
	ASSERT(flags==0);

	int oper,prio;
	treeID x,y;
	tokenT tok;

	x=parse_postfix(parser,flags);
	if (x==NO_TREE) goto esc;

	retry:
	//came accross a statement, leave while we can!
	if (parser->tok_prox.type==TK_ASSIGN) goto esc;

	oper=parser->tok.type;
	prio=tok2prec(oper);

	if (prio<=rank) goto esc;

	tok=get_tok(parser);
	y=parse_subexpr(parser,flags,prio);
	if (y==NO_TREE) goto esc;

	x=tree_xy(parser,tok.line,tok2tree(oper),NT_ANY,x,y);
	goto retry;

	esc:
	return x;
}

static treeID parse_expr(Parser *parser, int flags) {
	switch (parser->tok.type) {
		case TK_NONE:
		case TK_LET:
		case TK_FOR: case TK_WHILE: case TK_LASTLY:
		case TK_COMMA:
		case TK_PAREN_RIGHT: case TK_CURLY_RIGHT: case TK_SQUARE_RIGHT: {
			return NO_TREE;
		}
	}
	return parse_subexpr(parser,flags,0);
}

//todo:just use the same enum for the token and the tree
static int tok2tree(int tok) {
	switch (tok) {
		case TK_DOT_DOT: return EXPR_RANGE;
		case TK_LOG_AND: return EXPR_AND;
		case TK_LOG_OR: return EXPR_OR;
		case TK_NIL_OR: return EXPR_NIL_OR;
		case TK_NIL_AND: return EXPR_NIL_AND;
		case TK_ADD: return EXPR_ADD;
		case TK_SUB: return EXPR_SUB;
		case TK_DIV: return EXPR_DIV;
		case TK_MUL: return EXPR_MUL;
		case TK_POW: return EXPR_POW;
		case TK_MOD: return EXPR_MOD;
		case TK_NEQ: return EXPR_NEQ;
		case TK_EQ: return EXPR_EQ;
		case TK_GT: return EXPR_GT;
		case TK_GTEQ: return EXPR_GTEQ;
		case TK_LT: return EXPR_LT;
		case TK_LTEQ: return EXPR_LTEQ;
		case TK_SHL: return EXPR_BIT_SHL;
		case TK_SHR: return EXPR_BIT_SHR;
		case TK_BIT_XOR: return EXPR_BIT_XOR;
		case TK_BIT_OR: return EXPR_BIT_OR;
		case TK_BIT_AND: return EXPR_BIT_AND;
		default: return TREE_NONE;
	}
}

static treeID *parse_expr_list(Parser *parser) {
	treeID y,*yz,*z=0;
	do{
		y=parse_expr(parser,0);
		if(get_tree_kind(parser,y)==EXPR_MULTI) {
			yz=get_tree(parser,y).z;
			FOR_ARRAY(i,yz) ARRAY_ADD(z,yz[i]);
		} else {
			ARRAY_ADD(z,y);
		}
	}while(pick_tok(parser,TK_COMMA));
	return z;
}

static treeID parse_block(Parser *parser){
	tokenT tok;
	tok=parser->tok;
	if(pick_tok(parser,TK_CURLY_LEFT)){
		begin_block(parser);
		while (parser->tok.type!=TK_NONE && parser->tok.type!=TK_CURLY_RIGHT) {
			parse_stat(parser);
		}
		take_tok(parser,TK_CURLY_RIGHT);
	} else {
		parse_stat(parser);
	}
	return close_block(parser);
}

static int parse_stat(Parser *parser) {
	int success;
	tokenT tok;

	success=1;
	tok=parser->tok;

	switch (tok.type) {
		case TK_NONE: case TK_CURLY_RIGHT:
		case TK_THEN: case TK_ELSE: case TK_ELIF: {
			return 0;
		}
	}

	switch (tok.type) {
		case TK_LEAVE: {
			parser->block.ended=1;
			parser->block.has_ret=1;
			get_tok(parser);
			treeID v;
			v=tree_ret(parser,tok.line,v);
			block_add(parser,v);
		} break;
		case TK_CURLY_LEFT: {
			treeID v;
			v=parse_block(parser);
			block_add(parser,v);
		} break;
		case TK_LET: {
			treeID value,v;
			char *name;

			get_tok(parser);
			name=take_tok(parser,TK_WORD).text;
			take_tok(parser,TK_ASSIGN);
			value=parse_expr(parser,0);
			v=tree_x(parser,tok.line,TREE_ASSIGN_MEM,NT_ANY,value);
			block_add(parser,v);
			parser_bind(parser,tok.line,ENTITY_ASSIGNED,name,value);
		} break;
		case TK_WHILE:{
			treeID pred,body,v;

			get_tok(parser);
			pred=parse_expr(parser,0);
			take_tok(parser,TK_QMARK);
			body=parse_block(parser);

			v=new_tree(parser,TREE_WHILE_LOOP,tok.line);
			v->stat_while.pred=pred;
			v->stat_while.body=body;
			block_add(parser,v);
		}break;
		//todo:
		// for 0...100 ? {
		// 	#index
		// }
		case TK_FOR: {
			tokenT name;
			treeID pred,body,v,index,hi,step;
			treeT tree;
			treeID *steps;

			begin_scope(parser);

			get_tok(parser);

			name=take_tok(parser,TK_WORD);
			take_tok(parser,TK_ASSIGN);

			steps=0;
			do {
				v=parse_expr(parser,0);
				ARRAY_ADD(steps,v);
			}while(pick_tok(parser,TK_COMMA));

			take_tok(parser,TK_QMARK);

			/* todo: I'd like to find a better solution */
			index=tree_int(parser,name.line,0);

			parser_bind(parser,name.line
			, ENTITY_REFERENCED|ENTITY_ASSIGNED|ENTITY_FORLOOP
			, name.text,index);

			v=tree_assign_mem(parser,tok.line,index);
			block_add(parser,v);


			body=parse_block(parser);
			ASSERT(body->kind==STAT_BLOCK);

			FOR_ARRAY(i,steps){
				step=v=steps[i];
				tree=get_tree(parser,v);

				// if(tree.kind==EXPR_RANGE){
				// 	index=tree.x;
				// }

				begin_block(parser);

				if(tree.kind==EXPR_RANGE){
					// index=tree.x;
					v=tree_store(parser,tok.line,index,tree.x);
					block_add(parser,v);

					hi=tree.y;
					// v=tree_assign_mem(parser,tok.line,index);
					// block_add(parser,v);

					pred=tree_less_than(parser,tok.line,index,hi);

					v=tree_xy(parser,tok.line,EXPR_ADD,NT_ANY,index,tree_int(parser,tok.line,1));
					v=tree_store(parser,tok.line,index,v);
					ARRAY_ADD(body->z,v);

					v=new_tree(parser,TREE_WHILE_LOOP,tok.line);
					v->stat_while.pred=pred;
					v->stat_while.body=body;
					block_add(parser,v);
				}else {
					v=tree_store(parser,tok.line,index,step);
					block_add(parser,v);

					block_add(parser,body);
				}

				v=close_block(parser);
				block_add(parser,v);
			}

			close_scope(parser);
		} break;
		case TK_IF: case TK_IFF: {
			ASSERT(tok.type==TK_IF);
			get_tok(parser);

			treeID v;
			treeID pred;
			treeID true_clause;
			treeID else_clause;

			pred=parse_expr(parser,0);
			take_tok(parser,TK_QMARK);
			true_clause=parse_block(parser);
			if (pick_tok(parser,TK_ELSE)) {
				else_clause=parse_block(parser);
			}else else_clause=NO_TREE;

			v=new_tree(parser,STAT_IF,tok.line);
			v->stat_if.pred=pred;
			v->stat_if.true_clause=true_clause;
			v->stat_if.else_clause=else_clause;
			block_add(parser,v);
		} break;
		default: {
			treeID v,x,y;
			x=v=parse_expr(parser,0);
			if(v==NO_TREE){
				parser_dialog(parser,tok.line,"invalid statement");
				success=0;
				goto esc;
			}
			//todo:proper assign
			if(pick_tok(parser,TK_ASSIGN)){
				y=parse_expr(parser,0);
				v=tree_store(parser,tok.line,x,y);
			}
			block_add(parser,v);
		} break;
	}
	esc:
	return success;
}

static int tok2prec(int type) {
	return tok2inf[type].prec;
}


#if 0
static void parse_for_loop(Parser *parser) {
	// treeID y,*z,value,array,index,lo,hi;
	// int block,block_head=NO_BYTE,block_tail=NO_BYTE;
	// int value_register,array_register,index_register;
	// for i = 0...10, 1....100, 1...123 ?
	block = parser_begin_block(parser,BLOCK_LOOP);
	value_register = reg_alloc_deprecated(parser);
	value = tree_load(parser,name.line,value_register);

	/* todo: remove REFERENCED, instead allow the user to not have to specify the name */

	FOR_ARRAY(i,z){
		y=z[i];
		array=NO_TREE;
		array_register=NO_SLOT;
		index=NO_TREE;
		index_register=NO_SLOT;

		if (get_tree_kind(parser,y)==IR_RANGE_INDEX) {
			array=get_tree(parser,y).x;
			array_register=compiler_ir2anyreg(parser,array);

			y=get_tree(parser,y).y;
			ASSERT(get_tree_kind(parser,y)==IR_RANGE);
		}
		if (get_tree_kind(parser,y)==IR_RANGE) {
			lo=get_tree(parser,y).x;
			hi=get_tree(parser,y).y;
			if (array==NO_TREE) {
				index=value;
				// for ... ? { }
				if (lo==NO_TREE)lo=tree_int(parser,tk.line,0);
				if (hi==NO_TREE)hi=tree_int(parser,tk.line,-1);
			} else {
				// for array[...] ? { }
				if (lo==NO_TREE)lo=tree_int(parser,tk.line,0);
				if (hi==NO_TREE)hi=tree_call_metafield(parser,tk.line,array,0,"length");
				/* todo: we're allocating this here, and never freeing it! */
				index=tree_load(parser,tk.line,reg_alloc_deprecated(parser));
			}

			begin_range_loop(parser,tk.line,index,lo,hi);

			get_block(parser,block)->loop.value_register=value_register;
			get_block(parser,block)->loop.array_register=array_register;

			if (array!=NO_TREE) {
				treeID *z = {0};
				ARRAY_ADD(z,index);
				emit_store_deprecated(parser,name.line,value,tree_call_metafield(parser,tk.line,array,z,"idx"));
			}
			/* todo: could be neater */
			if (i==0) {
				block_head=get_instr_cursor(parser);
				parse_stat(parser);
				block_tail=get_instr_cursor(parser);
			} else {
				FOR_RANGE(j,block_head,block_tail) {
					emit_byte_deprecated(parser,elf_get_instr_line(parser->M,j),get_byte(parser,j));
				}
			}
			close_range_loop(parser,tk.line);
		} else {
			emit_store_deprecated(parser,tk.line,value,y);
			if (i==0) {
				block_head=get_instr_cursor(parser);
				parse_stat(parser);
				block_tail=get_instr_cursor(parser);
			} else {
				FOR_RANGE(j,block_head,block_tail){
					emit_byte_deprecated(parser,elf_get_instr_line(parser->M,j),get_byte(parser,j));
				}
			}
			/* because we are within a loop
			the user can use "continue" and
			"break", continues are the ones
			we need to handle here, which
			mean move on to the next step. */
			FileBlock *loop;

			loop=get_block(parser,block);
			patch_jumps(parser,loop->loop.true_jumps);
			ARRAY_DELETE(loop->loop.true_jumps);
			loop->loop.true_jumps = 0;
		}
	}
	parser_close_block(parser);
}

#endif





#if 0

/* Todo: should be the instruction not the register*/
void emit_continue(Parser *fs, Source line, int reg);
void emit_break(Parser *fs, Source line, elf_StackId with_value_register);

static bool peek_tok(Parser *parser, tokenTy k) {
	return parser->tok.type == k;
}

/* whether there are no more tokens or whether the
current token is a match. */
static bool eof_or_tok(Parser *parser, tokenTy k) {
	return parser->tok.type == TK_NONE || parser->tok.type == k;
}

static bool eof_or_eol_tok(Parser *parser) {
	return parser->tok.type == TK_NONE || parser->tok_prev.eol == 1;
}

static bool pick_tok(Parser *parser, tokenTy k) {
	return peek_tok(parser,k) && (get_tok(parser), 1);
}


static bool pick_tok_inl(Parser *parser, tokenTy k) {
	return peek_tok_inl(parser,k) && (get_tok(parser), 1);
}


static tokenT take_tok(Parser *fs, int k) {
	tokenT tk = fs->tk;
	if (!pick_tok(fs,k)) {
		parser_dialog(fs,fs->tok.line,"expected '%s'\n",tok2inf[k].name);
	}
	return tk;
}


/* looks for an enclosed entity within the function,
and returns the index where the entity, the index
can then be used to emit instructions. */
static int get_closure_value_index(IR_Function *fn, entityID2 id) {
	FOR_ARRAY(i,fn->enclosure) {
		if (fn->enclosure[i] == id.id) {
			return i;
		}
	}
	return NO_SLOT;
}


/* encloses an entity within the given function. */
static void enclose_entity(Parser *fs, IR_Function *fn, entityID2 id) {
	/* ensure the entity should actually be captured */
	// ASSERT(id.id < fn->entities);
	/* check whether the entity was already captured */
	FOR_ARRAY(i,fn->enclosure) {
		if (fn->enclosure[i] == id.id) return;
	}
	ARRAY_ADD(fn->enclosure,id.id);
}

/* find the last declared entity for the given register within the current function only! */
// xx static int find_local_entity(Parser *fs, int args) {
// xx 	int id;
// xx 	for (id = fs->entity_index-1; id >= fs->fn->entities; id-=1) {
// xx 		if ((fs->entities[id].kind==ENTITY_LOCAL) && (fs->entities[id].args==args)) {
// xx 			return id;
// xx 		}
// xx 	}
// xx 	return -1;
// xx }


/* Finds the last declared entity with the given name. */



/* binds something to a name within the current scope,
the result is the entity id.
I don't think this necessarily has to be an treeID, because
we can also use this for more syntactic things, like maybe
constants, which are parse time evaluated... although, maybe
this should be code generator's job, either, this remains
fairly open ended for now ... */


void parser_close_block(Parser *parser) {
	close_scope(parser);
	parser->block = parser->block_stack[-- parser->block_index];

	Source line = parser->tok.line;
	treeID ir = tree_pop_memory_state(parser,line);
	ir_add_prox(parser,ir);


#if 0
	// xx FileBlock block = parser->block;
	// xx return ir_add_basic_block(parser,line,block.entry,ir_get_label(parser));

	ASSERT(fs->entity_index >= fs->fn->entities);
	FileBlock *bl = get_block(fs,-1);
	entityID id;
	/* xentity is the first entity within a block, if any. */
	for (id = bl->xentity; id < fs->entity_index; ++ id) {
		if (~fs->entities[id].flags & ENTITY_REFERENCED) {
			parser_dialog(fs,fs->entities[id].line,"'%s': unreferenced entity", fs->entities[id].name);
		}
	}
	fs->entity_index = bl->xentity;
	fs->nnodes    = bl->xnode;
	fs->nblocks  -= 1;
	// ASSERT(bl->level == fs->level);
	// fs->fn->block = bl->enclosing;
	fs->fn->xmemory = bl->xmemory;
	if (bl->leavejumps != 0) {
		patch_jumps(fs,bl->leavejumps);
		ARRAY_DELETE(bl->leavejumps);
		bl->leavejumps = 0;
	}
	fs->nloops -= (bl->flags & BLOCK_LOOP) != 0;
#endif
}


static void close_function(Parser *parser) {
	ir_close_func(parser);

	// IR_Module *ir_module = parser->ir_module;
	// IR_Function *func = & ir_module->funcs[parser->func];
	// parser->func = func->enclosing;

	// Source line = parser->tok.line;
	// this handled in the code generator?
	// xxx patch_jumps(parser,func->yield_jumps);
	// xxx ARRAY_DELETE(func->yield_jumps);
	// xxx func->yield_jumps = 0;

	// emit_bytex_deprecated(parser,line,BC_LEAVE,0);


	// ASSERT(func->entry_block == parser->nblocks);
	// ASSERT(parser->entity_index == func->entities);
}


static tokenTy word2tok(tokenT tk) {
	/* todo: meh... remove this */
	if (tk.type != TK_WORD) return tk.type;
	if (!strcmp(tk.text,"and")) return TK_LOG_AND;
	if (!strcmp(tk.text,"or"))  return TK_LOG_OR;
	if (!strcmp(tk.text,"is"))  return TK_EQ;
	return TK_WORD;
}




static treeKi tok2node(tokenTy tk) {
	switch (tk) {
		case TK_DOT_DOT: return IR_RANGE;
		case TK_LOG_AND: return IR_AND;
		case TK_LOG_OR: return IR_OR;
		case TK_NIL_OR: return IR_NIL_OR;
		case TK_NIL_AND: return IR_NIL_AND;
		case TK_ADD: return IR_ADD;
		case TK_SUB: return IR_SUB;
		case TK_DIV: return IR_DIV;
		case TK_MUL: return IR_MUL;
		case TK_POW: return IR_POW;
		case TK_MOD: return IR_MOD;
		case TK_NEQ: return IR_NEQ;
		case TK_EQ: return IR_EQ;
		case TK_GT: return IR_GT;
		case TK_GTEQ: return IR_GTEQ;
		case TK_LT: return IR_LT;
		case TK_LTEQ: return IR_LTEQ;
		case TK_SHL: return IR_BIT_SHL;
		case TK_SHR: return IR_BIT_SHR;
		case TK_BIT_XOR: return IR_BIT_XOR;
		case TK_BIT_OR: return IR_BIT_OR;
		case TK_BIT_AND: return IR_BIT_AND;
		default: return IR_NONE;
	}
}


treeID parse_subexpr(Parser *fs, BooleanJumps *expr, int rank, int flags) {
	int oper,prio;
	treeID x,y;
	tokenT tk;

	x=parse_unary(fs,expr,flags|EXPR_ALLOW_POSTFIX);
	if (x==NO_TREE) goto esc;

	retry:
	oper=word2tok(fs->tok);
	prio=tok2prec(oper);
	if (prio<=rank) goto esc;
	if (fs->tok_prox.type==TK_ASSIGN) goto esc;
	tk=get_tok(fs);
	y=parse_subexpr(fs,0,prio,flags);
	if (y==NO_TREE) goto esc;
	x=tree_xy(fs,tk.line,tok2node(oper),NT_ANY,x,y);
	goto retry;

	esc:
	return x;
}

static treeID parse_function(Parser *parser) {
	tokenT tok = take_tok(parser,TK_FUN);
	IR_FuncId fun = begin_function(parser,tok.line);

	int arity = 1;

	take_tok(parser,TK_PAREN_LEFT);
	if (!peek_tok(parser,TK_PAREN_RIGHT)) do {
		/* todo: default values would be pretty easy to add, maybe? */

		tokenT name = take_tok(parser,TK_WORD);

		treeID param = ir_param(parser,name.line,NO_TREE);
		ir_add_prox(parser,param);

		parser_bind(parser,name.line,ENTITY_PARAMETER|ENTITY_ASSIGNED,name.text,param);

		arity += 1;
	} while (pick_tok(parser,TK_COMMA));

	ir_set_func_arity(parser,fun,arity);

	if (!peek_tok(parser,TK_PAREN_RIGHT)) {
		parser_dialog(parser,0,"did you miss a ','?");
	}
	take_tok(parser,TK_PAREN_RIGHT);

	/* '?' are now optional */
	pick_tok(parser,TK_QMARK);

	if (peek_tok(parser,TK_CURLY_LEFT)) {
		take_tok(parser,TK_CURLY_LEFT);

		while (parse_stat(parser)) {
			tok = parser->tok;
		}
		/* By default we emit a 'leave this' instruction,
		because this tends to be more convenient... */

		/* todo: only emit the yield if this block
		wasn't terminated by another leave instruction,
		otherwise this is wasteful
		Todo: I was under the impression that 'this' was
		just the default return because it was the first
		register, but I guess not...
		Maybe we can make it the be in the first register... */
		// emit_yield(parser,tok.line,ir_this(parser,tok.line));

		ir_yield(parser,tok.line,ir_this(parser,tok.line));

		take_tok(parser,TK_CURLY_RIGHT);
	} else {

		// emit_yield(parser,tok.line,parse_expr(parser,0,0));
		ir_yield(parser,tok.line,parse_expr(parser,0,0));
	}

	close_function(parser);

	// No longer do this
	// patch_jump(parser,fj);

	/* create a new prototype and add this function
	to the list of prototypes */
	#if 0
	elf_Proto fp = {0};
	fp.arity	  = arity;
	fp.nlocals = fn.nlocals;
	fp.nvalues = ARRAY_LENGTH(fn.enclosure);
	fp.bytes   = fn.bytes;
	fp.nbytes  = parser->M->nbytes - fn.bytes;
	int f = elf_add_proto(parser->M,fp);
	#endif

	// will remove this temporarily
	#if 0
	treeID *z = 0;
	FOR_ARRAY(i,fn.enclosure) {
		entityT entity = parser->entities[fn.enclosure[i]];
		ARRAY_ADD(z,tree_load(parser,entity.line,entity.args));
	}
	#endif

	// now IR instruction use IR functions....
	return ir_new_closure(parser,tok.line,fun,0);
}

// todo: deprecated
#if 0
void parse_assign(Parser *fs, treeID lexpr) {
	if (lexpr == NO_TREE) {
		return;
	}

	treeID x,y;
	int mem;
	tokenT tk,op;

	tk=fs->tk;
	mem=get_mem_state_deprecated(fs);
	x=desugar_range_expr(fs,lexpr,0);
	x=emit_preload_deprecated(fs,x);
	if (pick_tok(fs,TK_ASSIGN)) {
		y=parse_expr(fs,0,0);
		emit_store_deprecated(fs,tk.line,x,y);
	} else if (pick_tok(fs,TK_NIL_ASSIGN)) {
		BooleanJumps js = {0};
		y=parse_expr(fs,0,0);
		emit_jump_if_not_nil(fs,tk.line,&js,x);
		emit_store_deprecated(fs,tk.line,x,y);
		patch_jumps(fs,js.f);
	} else {
		if (tok2prec(tk.type) > 0) {
			op=get_tok(fs);
			take_tok(fs,TK_ASSIGN);
			/* todo: optimization! */
			y=parse_expr(fs,0,0);
			y=tree_xy(fs,op.line,tok2node(op.type),get_tree_type(fs,x),x,y);
			emit_store_deprecated(fs,op.line,x,y);
		} else {
			emit_eval_deprecated(fs,0,-1,0,lexpr);
		}
	}
	desugar_range_expr_epilogue(fs,lexpr);
	set_mem_state_deprecated(fs,mem);
}
#endif

/* todo: add support for:
specifing which for loop you're reffering to. */
#if 0
elf_StackId target_value_register = NO_SLOT;
if (!eof_or_eol_tok(fs)) {
	treeID value = parse_expr(fs,0);
	if (value != NO_TREE) {
		target_value_register = get_treereg_deprecated(fs,TREEID(value));
		if (target_value_register < 0) {
			parser_dialog(fs,get_tree_line(fs,value),"invalid value");
		}
	}
}
#endif

static treeID parse_unary(Parser *parser, BooleanJumps *expr, bool flags) {
	treeID v;
	tokenT tk;
	treeID x;

	v=NO_TREE;
	tk=parser->tok;
	switch (tk.type) {
		case TK_M_INDEX: case TK_M_ARRAY: case TK_M_VALUE: {
			get_tok(parser);
			int reg;

			if (tk.type==TK_M_ARRAY) reg=SPECIAL_REGISTER_ARRAY; else
			if (tk.type==TK_M_VALUE) reg=SPECIAL_REGISTER_VALUE; else reg=SPECIAL_REGISTER_INDEX;

			v=tree_load(parser,tk.line,reg);
		} break;
		/* todo: make this an intrinsic instruction! */
		case TK_M_INT: case TK_M_NUM: {
			char *name;
			treeID fn, *z=0;

			get_tok(parser);
			x=parse_unary(parser,0,flags|EXPR_ALLOW_POSTFIX);
			name=tk.type==TK_M_INT?"ntoi":"iton";
			fn=tree_global_ref_by_name(parser,tk.line,name);
			ARRAY_ADD(z,x);
			v=tree_call(parser,tk.line,fn,z);
		} break;
		case TK_M_REGISTER: {
			get_tok(parser);
			tk=take_tok(parser,TK_WORD);
			x=identify(parser,tk.text,0);
			if (x!=NO_ENTITY) {
				v=tree_int(parser,tk.line,parser->entities[x].args);
			} else parser_dialog(parser,tk.line,"'%s': invalid entity (must be a local)",tk.text);
		} break;
		//
		// 'load' ( <file-name> )
		//
		//  elf.loadfile(<file-name>)
		//
		case TK_LOAD: {
			get_tok(parser);
			treeID *call_args = parse_call_args(parser);

			treeID load_file_func = tree_global_ref_by_name(parser,tk.line,"elf.loadfile");
			treeID call_load_file = tree_call(parser,tk.line,load_file_func,call_args);
			v = call_load_file;
		} break;
		//
		// 'new' <meta-table> ( <argument-list> )
		//
		// leave elf.set_object_metatable({},Vector2):__new(x,y)
		//
		case TK_NEW: {
			get_tok(parser);
			treeID meta_field_name, get_meta_field, call_new
			, meta_table, *call_args, table;

			meta_table=parse_unary(parser,0,0);
			call_args=parse_call_args(parser);

			/* so if the user does something like new Thing {}
			or new Thing({}) the table that was passed in can
			be used as supposed to creating a new one */
			if ((ARRAY_LENGTH(call_args) == 1) && (get_tree_kind(parser,call_args[0]) == IR_TABLE)) {
				table = call_args[0];
			} else {
				/* If the user however, doesn't do this, then we
				create a new table for him */
				table = tree_table(parser,tk.line,0);
			}

			table=tree_call_set_metatable(parser,tk.line,table,meta_table);

			meta_field_name=tree_str(parser,tk.line,"__new");
			get_meta_field=tree_metafield(parser,tk.line,table,meta_field_name);
			call_new=tree_call(parser,tk.line,get_meta_field,call_args);

			v = call_new;
		} break;
		/* empty ranges are allowed and interpreted as min...max + 1 */
		case TK_DOT_DOT: {
			get_tok(parser);
			v=tree_xy(parser,tk.line,IR_RANGE,NT_ANY,NO_TREE,NO_TREE);
		} break;
		/* todo: this is temporary */
		case TK_DOT:
		case TK_ELF: {
			char dir[MAX_PATH] = {};
			if (pick_tok(parser,TK_ELF)) {
				strcat(dir,"elf");
				/* remind the user elf is a reserved keyword */
				if (!peek_tok_inl(parser,TK_DOT)) {
					parser_dialog(parser,tk.line,"incomplete symbol, expected '.' on the same line as 'elf'. Did you mean to use 'elf'? This is a reserved keyword and it refers to the elf directory.");
				}
			}

			take_tok(parser,TK_DOT);
			do {
				get_token_inline(parser,TK_WORD);
				strcat(dir,".");
				strcat(dir,parser->tok_prev.text);
			} while (pick_tok_inl(parser,TK_DOT));
			int x;
			x=elf_get_global(parser->M,elf_alloc_string(parser->R,dir));
			v=tree_global_ref(parser,tk.line,x);
		} break;
		case TK_WORD: {
			get_tok(parser);
			v=find_name(parser,tk.line,flags,tk.text);
		} break;
		case TK_SUB: {
			get_tok(parser);
			v=parse_subexpr(parser,0,10000,flags);
			v=tree_xy(parser,tk.line,IR_SUB,NT_INT,tree_int(parser,tk.line,0),v);
		} break;
		case TK_ADD: {
			get_tok(parser);
			v=parse_subexpr(parser,0,10000,flags);
		} break;
		case TK_CURLY_LEFT: {
			v=parse_table(parser);
		} break;
		case TK_PAREN_LEFT: {
			get_tok(parser);
			v=parse_expr(parser,0,EXPR_ALLOW_POSTFIX);
			take_tok(parser,TK_PAREN_RIGHT);
			/* allow for empty () */
			if (v!=NO_TREE) {
				// xx v=tree_group(parser,tk.line,v);
				// xx ASSERT(v != NO_TREE);
			}
		} break;
		case TK_FUN: {
			v=parse_function(parser);
		} break;
		case TK_NIL: {
			get_tok(parser);
			v=tree_nil(parser,tk.line);
		} break;
		case TK_TRUE: case TK_FALSE: {
			get_tok(parser);
			v=tree_int(parser,tk.line,tk.type==TK_TRUE);
		} break;
		case TK_LETTER: case TK_INTEGER: {
			get_tok(parser);
			v=tree_int(parser,tk.line,tk.integer);
		} break;
		case TK_NUMBER: {
			get_tok(parser);
			v=ir_number(parser,tk.line,tk.number);
		} break;
		case TK_STRING: {
			get_tok(parser);
			v=tree_str(parser,tk.line,tk.text);
		} break;
		case TK_DEFAULT: {
			parser_dialog(parser,tk.line,"syntax error: default expressions can only be top level");
			elf_fail(parser->R,0,"syntax error: default expressions can only be top level");
		} break;
		default: {
			parser_dialog(parser,tk.line,"'%s': unexpected token", tok2inf[tk.type].name);
			elf_fail(parser->R,0,"syntax error: unexpected token");
		} break;
	}

	if (~flags & EXPR_ALLOW_POSTFIX) {
		goto esc;
	}

	/* ensure we don't parse a postfix past a line */
	while (!eof_or_eol_tok(parser)) {
		tk = parser->tk;

		switch (tk.type) {
			case TK_DOT: {
				get_tok(parser);
				// table.(x,y) -> (table.x, table.y)
				if (pick_tok(parser,TK_PAREN_LEFT)) {
					treeID *z = {0};
					do {
						tokenT n = take_tok(parser,TK_WORD);
						treeID x,y;
						y=tree_str(parser,n.line,n.text);
						x=tree_field(parser,tk.line,v,y);
						ARRAY_ADD(z,x);
					} while (pick_tok(parser,TK_COMMA));
					v = tree_multi(parser,tk.line,z);
					take_tok(parser,TK_PAREN_RIGHT);
				} else
				// table.{x,y}
				if (pick_tok(parser,TK_CURLY_LEFT)) {
					NO_CODE;
				} else {
					tokenT name;
					treeID field;
					name=take_tok(parser,TK_WORD);
					field=tree_str(parser,name.line,name.text);
					v=tree_field(parser,tk.line,v,field);
				}
			} break;
			/* todo: make this nil safe, so [0,0] shouldn't
			fail if item at 0 is nil  */
			case TK_SQUARE_LEFT: {
				take_tok(parser,TK_SQUARE_LEFT);
				treeID *z;
				treeID index;
				do {
					index=parse_expr(parser,0,0);
					if (index==NO_TREE) break;
					/* registry[location.(y,x)] ->
					registry[location.y,location.x] */
					if (get_tree_kind(parser,index)==IR_MULTI) {
						z=get_tree(parser,index).z;
						FOR_ARRAY(i,z) {
							v=tree_index(parser,tk.line,v,z[i]);
						}
					} else if (get_tree_kind(parser,index)==IR_RANGE) {
						v=tree_ranged_index(parser,tk.line,v,index);
					} else {
						v=tree_index(parser,tk.line,v,index);
					}

					/* todo: this is silly, this is just an
					inner multi expressions, make multi
					expressions be regular 'comma' expressions
					instead */
				} while(pick_tok(parser,TK_COMMA));
				take_tok(parser,TK_SQUARE_RIGHT);
			} break;

			case TK_COLON: {
				tokenT n;
				treeID y;
				get_tok(parser);
				n=take_tok(parser,TK_WORD);
				y=tree_str(parser,n.line,n.text);
				v=tree_metafield(parser,tk.line,v,y);
			} break;
			case TK_CURLY_LEFT:
			case TK_PAREN_LEFT: {
				treeID *z;
				z=parse_call_args(parser);
				v=tree_call(parser,tk.line,v,z);
			} break;
			default: goto esc;
		}
	}

	esc:
	return v;
}


void parse_for_loop(Parser *fs) {
	__debugbreak();
#if 0
	tokenT tk,name;
	treeID y,*z,value,array,index,lo,hi;
	int block,block_head=NO_BYTE,block_tail=NO_BYTE;
	int value_register,array_register,index_register;

	tk=take_tok(fs,TK_FOR);
	name=take_tok(fs,TK_WORD);
	take_tok(fs,TK_ASSIGN);
	z=parse_expr_list(fs);
	take_tok(fs,TK_QMARK);

	block = parser_begin_block(fs,BLOCK_LOOP);
	value_register = reg_alloc_deprecated(fs);
	value = tree_load(fs,name.line,value_register);

	/* todo: remove REFERENCED, instead allow the user to not have to specify the name */
	parser_bind(fs,name.line,ENTITY_REFERENCED|ENTITY_ASSIGNED|ENTITY_FORLOOP
	, name.text,value_register);

	FOR_ARRAY(i,z){
		y=z[i];
		array=NO_TREE;
		array_register=NO_SLOT;
		index=NO_TREE;
		index_register=NO_SLOT;

		if (get_tree_kind(fs,y)==IR_RANGE_INDEX) {
			array=get_tree(fs,y).x;
			array_register=compiler_ir2anyreg(fs,array);

			y=get_tree(fs,y).y;
			ASSERT(get_tree_kind(fs,y)==IR_RANGE);
		}
		if (get_tree_kind(fs,y)==IR_RANGE) {
			lo=get_tree(fs,y).x;
			hi=get_tree(fs,y).y;
			if (array==NO_TREE) {
				index=value;
				// for ... ? { }
				if (lo==NO_TREE)lo=tree_int(fs,tk.line,0);
				if (hi==NO_TREE)hi=tree_int(fs,tk.line,-1);
			} else {
				// for array[...] ? { }
				if (lo==NO_TREE)lo=tree_int(fs,tk.line,0);
				if (hi==NO_TREE)hi=tree_call_metafield(fs,tk.line,array,0,"length");
				/* todo: we're allocating this here, and never freeing it! */
				index=tree_load(fs,tk.line,reg_alloc_deprecated(fs));
			}

			begin_range_loop(fs,tk.line,index,lo,hi);

			get_block(fs,block)->loop.value_register=value_register;
			get_block(fs,block)->loop.array_register=array_register;

			if (array!=NO_TREE) {
				treeID *z = {0};
				ARRAY_ADD(z,index);
				emit_store_deprecated(fs,name.line,value,tree_call_metafield(fs,tk.line,array,z,"idx"));
			}
			/* todo: could be neater */
			if (i==0) {
				block_head=get_instr_cursor(fs);
				parse_stat(fs);
				block_tail=get_instr_cursor(fs);
			} else {
				FOR_RANGE(j,block_head,block_tail) {
					emit_byte_deprecated(fs,elf_get_instr_line(fs->M,j),get_byte(fs,j));
				}
			}
			close_range_loop(fs,tk.line);
		} else {
			emit_store_deprecated(fs,tk.line,value,y);
			if (i==0) {
				block_head=get_instr_cursor(fs);
				parse_stat(fs);
				block_tail=get_instr_cursor(fs);
			} else {
				FOR_RANGE(j,block_head,block_tail){
					emit_byte_deprecated(fs,elf_get_instr_line(fs->M,j),get_byte(fs,j));
				}
			}
			/* because we are within a loop
			the user can use "continue" and
			"break", continues are the ones
			we need to handle here, which
			mean move on to the next step. */
			FileBlock *loop;

			loop=get_block(fs,block);
			patch_jumps(fs,loop->loop.true_jumps);
			ARRAY_DELETE(loop->loop.true_jumps);
			loop->loop.true_jumps = 0;
		}
	}
	parser_close_block(fs);
#endif
}

void parse_block(Parser *parser) {
	parser_begin_block(parser,0);
	if (pick_tok(parser,TK_CURLY_LEFT)) {
		while (!eof_or_tok(parser,TK_CURLY_RIGHT)) {
			parse_stat(parser);
		}
		take_tok(parser,TK_CURLY_RIGHT);
	} else parse_stat(parser);
	parser_close_block(parser);
}

int parse_stat(Parser *parser) {
	return (int) parse_stat(parser);
}


#if 0
int parse_stat(Parser *parser) {
	treeID ir = NO_TREE;
	tokenT tok = parser->tok;
	switch (tok.type) {
		case TK_NONE: case TK_CURLY_RIGHT:
		case TK_THEN: case TK_ELSE: case TK_ELIF: {
			return 0;
		}
	}


	// xxx FileBlock *bl = get_block(parser,-1);
	// xxx if (bl->flags & BLOCK_ENDED) {
	// xxx 	parser_dialog(parser,tok.line,"warning: unreachable statement");
	// xxx }

	switch (tok.type) {
		/* todo: go back to := */
		case TK_LET: {
			get_tok(parser);
			do {
				if (peek_tok(parser,TK_LET)) {
					parser_dialog(parser,parser->tok_prev.line,"invalid declaration, expected next declarator's name after ',' instead got 'let'");
					parser_dialog(parser,parser->tok.line,"invalid declaration, 'let' after comma");
					elf_fail(parser->R,0,"syntax error: invalid declaration");
				}

				tokenT name = take_tok(parser,TK_WORD);
				take_tok(parser,TK_ASSIGN);

				treeID x = parse_expr(parser,0,0);

				ir = tree_local(parser,name.line,x);
				ir_add_prox(parser,ir);

				parser_bind(parser,name.line,0,name.text,ir);

				// xxx parse_assign(parser,tree_load(parser,name.line,reg));

			} while (pick_tok(parser,TK_COMMA));
		} break;
		case TK_LASTLY: case TK_FINALLY: {
			__debugbreak();
			#if 0
			get_tok(parser);
			if (tk.type == TK_FINALLY) {
				parser_dialog(parser,tk.line,"warning: please consider using 'lastly' instead, 'finally' could change semantics in the future");
			}
			begin_delay_block(parser,tk.line);
			parse_stat(parser);
			close_delay_block(parser,tk.line);
			ASSERT(get_mem_state_deprecated(parser)==mem);
			#endif
		} break;
		case TK_IF: case TK_IFF: {
			ASSERT(tok.type == TK_IF);

			get_tok(parser);

			treeID x;

			x=parse_expr(parser,0,0);
			take_tok(parser,TK_QMARK);
			// xxx BranchJumps s = {0};
			// xxx begin_if(parser,tk.line,&s,x,L_IF);
			//note: if node has to be created prior
			//to its labels to assume control
			ir=tree_if(parser,tok.line,x,-1,-1);
			ir_add_prox(parser,ir);

			int t,f;
			t=ir_start_label(parser,"IF.T");
			parse_block(parser);
			f=ir_start_label(parser,"IF.F");
			// parse_block(parser);
			ir_start_label(parser,"RESUME");

			parser->prev=ir;
			parser->ir[ir].prox=0xffff;
			ARRAY_ADD(parser->ir[ir].z,t);
			ARRAY_ADD(parser->ir[ir].z,f);





#if 0
			while (!peek_tok(parser,TK_NONE)) {
				if (pick_tok(parser,TK_ELIF)) {
					parser_begin_block(parser,0);

					x = parse_expr(parser,0,0);
					take_tok(parser,TK_QMARK);

					// xxx add_elif_clause(parser,parser->tok_prev.line,&s,x);
					// xxx parse_stat(parser);

					parser_close_block(parser);
				} else if (pick_tok(parser,TK_THEN)) {
					parser_begin_block(parser,0);

					// xx add_then_clause(parser,parser->tok_prev.line,&s);
					// xx parse_stat(parser);

					parser_close_block(parser);
				} else if (pick_tok(parser,TK_ELSE)) {
					parser_begin_block(parser,0);

					// xxx add_else_clause(parser,parser->tok_prev.line,&s);
					// xxx parse_stat(parser);

					parser_close_block(parser);
				} else break;
			}
#endif
			// xxx close_if(parser,parser->tok_prev.line,&s);
		} break;
		case TK_LEAVE: {
			get_tok(parser);
			treeID x = parse_expr(parser,0,0);

			// emit_yield(parser,tk.line,x);

		} break;

#if 0
		case TK_BREAK: case TK_CONTINUE: {
			int reg = NO_TREE;
			get_tok(parser);
			/* Todo: defer til code generation */
			if (!eof_or_eol_tok(parser)) {
				treeID value = parse_unary(parser,0,0);
				if (value == NO_TREE) {
					if ((reg=get_treereg_deprecated(parser,TREEID(value)))<0) {
						parser_dialog(parser,get_tree_line(parser,value),"invalid value");
					}
				}
			}
			if (tk.type==TK_CONTINUE) emit_continue(parser,tk.line,reg);
			else emit_break(parser,tk.line,reg);
		} break;
		case TK_WHILE: {
			get_tok(parser);
			parser_begin_block(parser,BLOCK_LOOP);
			x=parse_expr(parser,0,0);
			take_tok(parser,TK_QMARK);
			begin_while_loop(parser,x);
			parse_stat(parser);
			close_while_loop(parser);
			parser_close_block(parser);
		} break;
		case TK_DO: {
			get_tok(parser);
			parser_begin_block(parser,BLOCK_LOOP);
			begin_do_while_loop(parser,tk.line);
			parse_stat(parser);
			take_tok(parser,TK_WHILE);
			x=parse_expr(parser,0,0);
			close_do_while_loop(parser,tk.line,x);
			parser_close_block(parser);
		} break;
		/* parse a block */
		case TK_CURLY_LEFT: {
			parser_begin_block(parser,0);
			get_tok(parser);
			while (!eof_or_tok(parser,TK_CURLY_RIGHT)) {
				parse_stat(parser);
			}
			take_tok(parser,TK_CURLY_RIGHT);
			x = parser_close_block(parser);
		} break;
		case TK_FOR: {
			parse_for_loop(parser);
		} break;
#endif
		default: {
#if 0
			entityID entity;
			int reg;
			if (parser->nblocks>1 && parser->tok_prox.type==TK_ASSIGN){
				get_tok(parser);
				entity=identify(parser,tk.text,0);
				if (entity==-1){
					parser_dialog(parser,tk.line,"new implicit entity: %s", tk.text);
					reg=reg_alloc_deprecated(parser);
					entity=parser_bind(parser,tk.line,0,tk.text,reg);
				} else {
					reg=parser->entities[entity].args;
				}
				x=tree_load(parser,tk.line,reg);
			} else {
				x=parse_expr(parser,0,0);
			}
#endif
			ir_add_prox(parser,tree_push_memory_state(parser,tok.line));
			ir = parse_expr(parser,0,0);
			if (ir != NO_TREE) {
				ir_add_prox(parser,ir);
				// xx parse_assign(parser,x);
			} else {
				parser_dialog(parser,tok.line,"invalid statement");
			}
			ir_add_prox(parser,tree_pop_memory_state(parser,tok.line));
		} break;
	}


	return 1;
}
#endif


#if 0

//
//	I think if statements don't have to be IR, because it wouldn't
// really matter, and 'if' is just a conditional jump, so we can
// translate that directly from code to lower level IR.
//
// yield has to be IR, it's literally an instruction.
//
// break / continue, those can be IR, but all the backend would
// do is store the addresses patch, and it would require the
// backend know what a loop is... So we can translate them to
// lower level IR instead...
//
// '&&' and expressions that can be short-circuited could be
// translated to lower level IR, but then it becomes harder to
// optimize? But then we'd have to replicate the same code for
// each backend.. Ok, since we're not doing and sort of optimization
// yet, we can translate it to lower level IR directly, and then
// if it becomes a problem we can revert.
// All loops can become lower level IR, since loop are pretty easy
// to find anyways... Alrighty then, we would have to remove some
// IR instructions, like AND and OR and so on... we  can leave that
// for last...
//
void begin_if(Parser *fs, Source line, BranchJumps *s, treeID x, int z) {
	BooleanJumps js = {0};
	emit_branch_if(fs,&js,z,x);
	// if  0 = jz
	// iff 1 = jnz
	if (z == L_IF) {
		ASSERT(js.f != 0);
		patch_jumps(fs,js.t);
		ARRAY_DELETE(js.t);
		js.t = 0;
		s->jz = js.f;
	} else {
		ASSERT(js.t != 0);
		patch_jumps(fs,js.f);
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
void add_else_clause(Parser *fs, Source line, BranchJumps *s) {
	if (s->jz == 0) {
		parser_dialog(fs,line,"invalid else clause");
	}
	ASSERT(s->jz != 0);
	int j = emit_jump(fs,line,-1);
	ARRAY_ADD(s->j,j);

	patch_jumps(fs,s->jz);
	ARRAY_DELETE(s->jz);
	s->jz = 0;
}


void add_elif_clause(Parser *fs, Source line, BranchJumps *s, int x) {
	add_else_clause(fs,line,s);
	begin_if(fs,line,s,x,L_IF);
}


void add_then_clause(Parser *fs, Source line, BranchJumps *s) {
	/* we don't need to close the previous block, it can just fall
	through to our branch, do collect all the other exit jumps and
	tie them to this branch block, naturally we don't need to add
	an exit jump since else and elif or closeif will terminate
	this block, multiple then blocks are simply chained together
	naturally. */
	patch_jumps(fs,s->j);
	ARRAY_DELETE(s->j);
	s->j = 0;
}


void close_if(Parser *fs, Source line, BranchJumps *s) {
	/* collect missing else branch */
	if (s->jz != 0) {
		patch_jumps(fs,s->jz);
		ARRAY_DELETE(s->jz);
		s->jz = 0;
	}
	/* collect missing then branch */
	if (s->j != 0) {
		patch_jumps(fs,s->j);
		ARRAY_DELETE(s->j);
		s->j = 0;
	}
}

void begin_do_while_loop(Parser *fs, Source line) {
	FileBlock *bl = get_block(fs,-1); // fs->fn->block
	ASSERT(bl->flags & BLOCK_LOOP);
	bl->loop.entry = get_instr_cursor(fs);
	bl->loop.false_jumps = 0;
	bl->loop.x = NO_TREE;
	bl->loop.index_register = NO_SLOT;
}


void close_do_while_loop(Parser *fs, Source line, treeID x) {
	FileBlock *bl = get_block(fs,-1); // fs->fn->block;
	ASSERT(bl->flags & BLOCK_LOOP);

	BooleanJumps js = {0};
	emit_jump_if_true(fs,&js,x);

	patch_jumps2(fs,js.t,bl->loop.entry);
	ARRAY_DELETE(js.t);
	js.t = 0;

	patch_jumps2(fs,bl->loop.true_jumps,bl->loop.entry);
	ARRAY_DELETE(bl->loop.true_jumps);
	bl->loop.true_jumps = 0;
}


void begin_while_loop(Parser *fs, treeID x) {
	FileBlock *bl = get_block(fs,-1);
	ASSERT(bl->flags & BLOCK_LOOP);

	bl->loop.x = x;

	bl->loop.entry = get_instr_cursor(fs);

	/* todo: this is temporary */
	emit_bytex_deprecated(fs,NO_LINE,BC_LOOP,-1);

	ASSERT(bl->loop.false_jumps == 0);

	BooleanJumps js = {0};
	bl->loop.false_jumps = emit_jump_if_false(fs,&js,x);
}


void close_while_loop(Parser *fs) {
	FileBlock *bl = get_block(fs,-1);
	ASSERT(bl->flags & BLOCK_LOOP);

	patch_jumps(fs,bl->loop.true_jumps);
	ARRAY_DELETE(bl->loop.true_jumps);
	bl->loop.true_jumps = 0;

	/* todo: this is temporary */
	ASSERT(BC_OP(get_byte(fs,bl->loop.entry))==BC_LOOP);
	// fs->M->bytes[bl->loop.entry].x = get_instr_cursor(fs);

	emit_jump(fs,NO_LINE,bl->loop.entry);

	patch_jumps(fs,bl->loop.false_jumps);
	ARRAY_DELETE(bl->loop.false_jumps);
	bl->loop.false_jumps = 0;
}


void begin_range_loop(Parser *fs, Source line, treeID index_node, treeID lo, treeID hi) {
	__debugbreak();

	FileBlock *bl = get_block(fs,-1);
	ASSERT(bl->flags & BLOCK_LOOP);

	ASSERT(index_node != NO_TREE);

	elf_StackId index_register; // xxx = compiler_ir2anyreg(fs,index_node);
	index_node = tree_load(fs,line,index_register);

	bl->loop.index_register = index_register;
	bl->loop.x = index_node;
	emit_eval_deprecated(fs,0,index_register,1,tree_type_guard(fs,get_tree_line(fs,lo),lo,NT_INT));

	bl->loop.entry = get_instr_cursor(fs);

	__debugbreak();
	elf_StackId hi_register; // xxx = compiler_ir2anyreg(fs,tree_type_guard(fs,get_tree_line(fs,hi),hi,NT_INT));
	hi = tree_load(fs,line,hi_register);
	treeID c = tree_less_than(fs,line,index_node,hi);

	ASSERT(bl->loop.false_jumps == 0);

	BooleanJumps js = {0};
	bl->loop.false_jumps = emit_jump_if_false(fs,&js,c);
}


void close_range_loop(Parser *fs, Source line) {
	FileBlock *bl = get_block(fs,-1); // fs->fn->block;
	ASSERT(bl->flags & BLOCK_LOOP);

	patch_jumps(fs,bl->loop.true_jumps);
	ARRAY_DELETE(bl->loop.true_jumps);
	bl->loop.true_jumps = 0;
	// treeID index_node = bl->loop.index_node;
	treeID index_node = tree_load(fs,NO_LINE,bl->loop.index_register);
	treeID k = tree_xy(fs,NO_LINE,IR_ADD,NT_INT,index_node,tree_int(fs,NO_LINE,1));

	__debugbreak();
	// xxx emit_store_deprecated(fs,line,index_node,k);

	emit_jump(fs,line,bl->loop.entry);

	patch_jumps(fs,bl->loop.false_jumps);
	ARRAY_DELETE(bl->loop.false_jumps);
	bl->loop.false_jumps = 0;
}
#endif

#if 0
// this is the code that emits a byte-code yield
// this should be over in emit, all we do here is
// emit the yield ir
void emit_yield(Parser *fs, Source line, treeID id) {
	/* todo: add support for multiple results */
	/* todo: if we only return one value we don't have to reload */
	int mem,reg,nreg,j;
	mem=get_mem_state_deprecated(fs);
	if (id!=NO_TREE) {
		/* todo: multi-returns */
		emit_eval_deprecated(fs,0,reg=reg_alloc_deprecated(fs),nreg=1,id);
		if (fs->fn->nyield < nreg) fs->fn->nyield = nreg;
		j=emit_bytexyz_deprecated(fs,line,BC_YIELD,NO_JUMP,reg,nreg);
		ARRAY_ADD(fs->fn->yield_jumps,j);
	} else emit_bytex_deprecated(fs,line,BC_LEAVE,0);
	set_mem_state_deprecated(fs,mem);
	add_block_flags(fs,BLOCK_ENDED);
}

void emit_continue(Parser *fs, Source line, int reg) {
	__debugbreak();

	ASSERT(fs->nloops > 0);
	Instr jmp;
	FileBlock *bl;

	bl=get_loop_block(fs,reg);
	ASSERT(bl!=0);
	add_block_flags(fs,BLOCK_ENDED);

	jmp=emit_jump(fs,line,get_instr_cursor(fs));
	ARRAY_ADD(bl->loop.true_jumps,jmp);
}

// get_instr_cursor is only for bytecode
void emit_break(Parser *fs, Source line, elf_StackId with_value_register) {
	__debugbreak();

	ASSERT(fs->nloops > 0);
	Instr jmp;
	FileBlock *bl;

	bl=get_loop_block(fs,with_value_register);
	ASSERT(bl!=0);
	add_block_flags(fs,BLOCK_ENDED);

	jmp=emit_jump(fs,line,get_instr_cursor(fs));
	ARRAY_ADD(bl->leavejumps,jmp);
}
#endif
#endif