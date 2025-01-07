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
static treeID parse_bl(Parser *parser);
static int parse_stat(Parser *parser);
static int tok2tree(int tok);
static int tok2prec(int tok);
static void add_this_param(Parser *parser, Source line);
static treeID parse_if(Parser *parser, int negate);
static treeID *parse_args(Parser *parser);


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

	treeID func;
	func=new_tree(parser,TREE_FUNCTION,parser->tok.line);
	add_this_param(parser,parser->tok.line);
	parser->enc=func;
	ARRAY_ADD(parser->functions,func);

	while (parser->tok.type!=TK_NONE) {
		if(!parse_stat(parser))break;
	}
	func->expr_fun.body=tree_block(parser,parser->tok.line,parser->block.body);

	return func;
}

static void check_tree(Parser *parser, Source line, treeID id) {
	if (id == NO_TREE){
		parser_dialog(parser,line,"invalid expression");
	}
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
	treeID enc;
	enc=parser->enc;

	{
		entityID entity_id;
		entity_id=identify(parser,name,0);

		if (entity_id!=NO_ENTITY) {

			entityT entity;
			entity=parser->entities[entity_id];

			if (entity.kind==ENTITY_DIRECTORY) {
				parser_dialog(parser,line,"'%s': name is reserved for symbol directory",name);
			}
			if (entity.scope==parser->scope) {
				parser_dialog(parser,line,"'%s': is already declared",name);
			} else {
				if (entity.scope>=parser->enc->expr_fun.scope) {
					parser_dialog(parser,line,"'%s': this declaration shadows another one",name);
				}
			}
		}
	}

	entityID entity_id;
	entity_id=parser->entity_index ++;

	//todo:use stack instead
	ARRAY_GROW(parser->entities,parser->entity_index-ARRAY_LENGTH(parser->entities));

	entityT entity;
	entity.kind = ENTITY_LOCAL;
	entity.status = flags;
	entity.scope = parser->scope;
	entity.tree = tree;
	entity.name = name;
	entity.line = line;
	parser->entities[entity_id]=entity;
	return entity_id;
}

/* if no assignment is found, the return is tree
passed in */
static treeID parse_assign(Parser *parser, treeID x) {
	ASSERT(x!=NO_TREE);

	tokenT tok;
	tok=parser->tok;

	// int mem;
	// mem=get_mem_state_deprecated(parser);
	// x=desugar_range_expr(parser,lexpr,0);
	// x=emit_preload_deprecated(parser,x);

	treeID v;
	v=x;

	if (pick_tok(parser,TK_ASSIGN)) {
		v=parse_expr(parser,0);
		v=tree_store(parser,tok.line,x,v);
	} else if (pick_tok(parser,TK_NIL_ASSIGN)) {
		// todo:?
		// src_pos(tok.line);

		//todo: use the preload idea, to cache temporary
		//registers....
		// (A.B.C).D ?= 1
		// ^^^^^^ <- otherwise this gets expensive since
		// it has to be evaluated twice...

		v=parse_expr(parser,0);
		v=tree_store(parser,tok.line,x,v);

		x=tree_binary(parser,tok.line,EXPR_EQ,NT_ANY,x,tree_nil(parser,tok.line));
		v=tree_if(parser,tok.line,x,v,0);
	} else if (tok2prec(tok.type) > 0) {
		tok=get_tok(parser);
		take_tok(parser,TK_ASSIGN);
		/* todo: optimization! */
		v=parse_expr(parser,0);
		v=tree_binary(parser,tok.line,tok2tree(tok.type),NT_ANY,x,v);
		v=tree_store(parser,tok.line,x,v);
	}
	// desugar_range_expr_epilogue(parser,lexpr);
	// set_mem_state_deprecated(parser,mem);
	ASSERT(v!=NO_TREE);
	return v;
}

static void add_this_param(Parser *parser, Source line){
	treeID param,x;
	param=tree_dummy(parser,line);

	x=tree_assign_mem(parser,line,param);
	block_add(parser,x);

	parser_bind(parser, line
	, ENTITY_PARAMETER|ENTITY_ASSIGNED|ENTITY_CONSTANT|ENTITY_REFERENCED
	, "this", param);
}
static treeID parse_fun(Parser *parser){
	tokenT tok;

	tok=take_tok(parser,TK_FUN);

	begin_block(parser);

	treeID enc;
	enc=parser->enc;

	treeID fun;
	fun=new_tree(parser,TREE_FUNCTION,tok.line);
	fun->expr_fun.scope=parser->scope;
	fun->expr_fun.enc=enc;
	parser->enc=fun;

	take_tok(parser,TK_PAREN_LEFT);

	add_this_param(parser,tok.line);

	if (!peek_tok(parser,TK_PAREN_RIGHT)) do {
		tokenT name;

		name=take_tok(parser,TK_WORD);

		treeID param,x;
		param=tree_dummy(parser,name.line);
		x=tree_assign_mem(parser,name.line,param);
		block_add(parser,x);

		parser_bind(parser, name.line
		, ENTITY_ASSIGNED|ENTITY_PARAMETER
		, name.text, param);
	} while (pick_tok(parser,TK_COMMA));

	if (!peek_tok(parser,TK_PAREN_RIGHT)) {
		parser_dialog(parser,0,"did you miss a ',' ?");
	}
	take_tok(parser,TK_PAREN_RIGHT);
	pick_tok(parser,TK_QMARK);

	if(peek_tok(parser,TK_CURLY_LEFT)){
		parse_stat(parser);
		{
			// todo: only if the block doesn't have a return already
			entityID ent;
			ent=identify(parser,"this",0);
			ASSERT(ent!=-1);

			entityT this_;
			this_=parser->entities[ent];

			treeID v;
			v=tree_ret(parser,tok.line,this_.tree);
			block_add(parser,v);
		}
	}else{
		treeID v;
		v=parse_expr(parser,0);
		v=tree_ret(parser,tok.line,v);
		block_add(parser,v);
	}

	treeID body;
	body=close_block(parser);

	fun->expr_fun.body=body;

	parser->enc=enc;

	ARRAY_ADD(parser->functions,fun);

	return fun;
}

//
// 'load' ( <file-name> )
//
//  elf.load_file(<file-name>)
//
static treeID parser_load(Parser *parser){
	tokenT tok;
	tok=take_tok(parser,TK_LOAD);

	treeID v,*z;
	z=parse_args(parser);

	v=tree_global_ref_by_name(parser,tok.line,"elf.load_file");
	v=tree_call(parser,tok.line,v,z);
	return v;
}

//
// 'new' <meta-table> ( <argument-list> )
//
// ret elf.set_object_metatable({},Vector2):__new(x,y)
//
static treeID parse_new(Parser *parser){
	tokenT tok;
	tok=take_tok(parser,TK_NEW);

	treeID meta,*args;
	meta=parse_unary(parser,0);
	args=parse_args(parser);

	treeID table;
	if ((ARRAY_LENGTH(args) == 1) && (get_tree_kind(parser,args[0]) == TREE_NEW_TABLE)) {
		/* reuse the table the table literal the user passed */
		table=args[0];
	} else {
		table=tree_table(parser,tok.line);
	}

	/* note: relies on the function returning the table,
	may want to move away from this */
	table=tree_call_set_metatable(parser,tok.line,table,meta);

	treeID v;
	v=tree_str(parser,tok.line,"__new");
	/* todo: this is sort of inefficient, but if we don't issue
	the call instruction with a meta-field, the generator won't
	insert the 'this' parameter */
	v=tree_metafield(parser,tok.line,table,v);
	v=tree_call(parser,tok.line,v,args);
	return v;
}
static treeID parse_unary(Parser *parser, bool flags) {
	tokenT tok;
	treeID v,x;

	v=NO_TREE;
	tok=parser->tok;

	switch (tok.type) {
		//todo: intrinsic!
		case TK_SUB: {
			get_tok(parser);
			v=parse_subexpr(parser,0,10000);
			v=tree_binary(parser,tok.line,EXPR_SUB,NT_ANY,tree_int(parser,tok.line,0),v);
		} break;
		case TK_ADD: {
			get_tok(parser);
			v=parse_subexpr(parser,0,10000);
		} break;
		case TK_NEW: {
			v=parse_new(parser);
		}break;
		case TK_LOAD: {
			v=parser_load(parser);
		} break;
		case TK_CURLY_LEFT: {
			v=parse_table(parser);
		} break;
		/* empty ranges are interpreted accordingly */
		case TK_DOT_DOT: {
			get_tok(parser);
			v=tree_nullary(parser,tok.line,TREE_RANGE,NT_ANY);
		} break;
		//todo: dot syntax is to be repurposed
		case TK_DOT:
		case TK_ELF: {
			char sym[MAX_PATH] = {};

			// elf is a keyword!
			if(pick_tok(parser,TK_ELF)){
				if (!peek_tok_inl(parser,TK_DOT)) {
					parser_dialog(parser,tok.line,"incomplete symbol, expected '.' on the same line as 'elf'. Did you mean to use 'elf'? This is a reserved keyword and it refers to the elf directory.");
				}
				strcat(sym,"elf");
			}


			take_tok(parser,TK_DOT);
			do{
				get_token_inline(parser,TK_WORD);
				strcat(sym,".");
				strcat(sym,parser->tok_prev.text);
			}while(pick_tok_inl(parser,TK_DOT));

			v=tree_global_ref_by_name(parser,tok.line,sym);
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
				treeID enc=parser->enc;
				if (entity.scope<enc->expr_fun.scope) {
					ASSERT(!"FIXME");
					enc=enc->expr_fun.enc;
					if (entity.scope<enc->expr_fun.scope) {
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
			get_tok(parser);
			v=parse_expr(parser,0);
			take_tok(parser,TK_PAREN_RIGHT);
		} break;
		case TK_FUN: {
			v=parse_fun(parser);
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
		default: goto err;
	}

	if(v==NO_TREE){
		err:
		parser_dialog(parser,tok.line,"'%s': unexpected token", tok2inf[tok.type].name);
		elf_fail(parser->R,0,"syntax error: unexpected token");
	}
	return v;
}

static treeID parse_table(Parser *parser) {
	treeID table,prev;
	tokenT tok;
	int index;
	treeID *args;

	tok=take_tok(parser,TK_CURLY_LEFT);

	index=0;
	tok=parser->tok;

	table=tree_table(parser,tok.line);
	prev=table;

	for (;(tok.type!=TK_NONE)&&(tok.type!=TK_CURLY_RIGHT);tok=parser->tok) {
		treeID key,value;

		key=value=NO_TREE;

		if ((tok.type==TK_WORD)&&(parser->tok_prox.type==TK_ASSIGN)) {
			tok=get_tok(parser);
			key=tree_str(parser,tok.line,tok.text);
		} else {
			value=parse_expr(parser,0);
		}
		tok=parser->tok;
		if (pick_tok(parser,TK_ASSIGN)) {
			value=parse_expr(parser,0);
		} else {
			key=tree_int(parser,tok.line,index++);
		}
		ASSERT(value!=NO_TREE);
		ASSERT(key!=NO_TREE);

		check_tree(parser,tok.line,key);
		check_tree(parser,tok.line,value);
		tok=parser->tok;

		treeID field;
		field=tree_field(parser,tok.line,table,key);

		treeID store;
		store=tree_store(parser,tok.line,field,value);
		prev=prev->prox=store;

		if (pick_tok(parser,TK_COMMA)) {
			continue;
		}
	}
	take_tok(parser,TK_CURLY_RIGHT);

	return table;
}

/* {x} | ( x { ... } ) | { <table-initializer-list> } */
static treeID *parse_args(Parser *parser) {
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

	while(parser->tok.type != TK_NONE && !parser->tok_prev.eol) {
		tok=parser->tok;

		switch (tok.type) {
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
					v=tree_field(parser,tok.line,v,field);
				}
			} break;
			/* todo: make this nil safe, so [0,0] shouldn't
			fail if item at 0 is nil  */
			case TK_SQUARE_LEFT: {
				take_tok(parser,TK_SQUARE_LEFT);
				treeID x,*z;
				do {
					x=parse_expr(parser,0);
					if (x==NO_TREE) break;
					/* registry[location.(y,x)] ->
					registry[location.y,location.x] */
					if (get_tree_kind(parser,x)==EXPR_MULTI) {
						z=get_tree(parser,x).z;
						FOR_ARRAY(i,z) {
							v=tree_index(parser,tok.line,v,z[i]);
						}
					} else if (get_tree_kind(parser,x)==TREE_RANGE) {
						v=tree_ranged_index(parser,tok.line,v,x);
					} else {
						v=tree_index(parser,tok.line,v,x);
					}
				} while(pick_tok(parser,TK_COMMA));
				take_tok(parser,TK_SQUARE_RIGHT);
			} break;
			case TK_COLON: {
				tokenT n;
				treeID y;
				get_tok(parser);
				n=take_tok(parser,TK_WORD);
				y=tree_str(parser,n.line,n.text);
				v=tree_metafield(parser,tok.line,v,y);
			} break;
			case TK_CURLY_LEFT:
			case TK_PAREN_LEFT: {
				treeID *z;
				z=parse_args(parser);
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
	//came across a statement, leave while we can!
	if (parser->tok_prox.type==TK_ASSIGN) goto esc;

	oper=parser->tok.type;
	prio=tok2prec(oper);

	if (prio<=rank) goto esc;

	tok=get_tok(parser);
	y=parse_subexpr(parser,flags,prio);
	if (y==NO_TREE) goto esc;

	x=tree_binary(parser,tok.line,tok2tree(oper),NT_ANY,x,y);
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
		case TK_DOT_DOT: return TREE_RANGE;
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

static treeID parse_bl(Parser *parser){
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
		case TK_RET:
		case TK_LEAVE: {

			if(tok.type==TK_LEAVE){
				parser_dialog(parser,tok.line,"consider using 'ret' instead!");
			}

			treeID v;

			parser->block.ended=1;
			parser->block.has_ret=1;

			v=NO_TREE;
			tok=get_tok(parser);
			if(!tok.eol){
				v=parse_expr(parser,0);
			}
			v=tree_ret(parser,tok.line,v);
			block_add(parser,v);
		} break;
		case TK_CURLY_LEFT: {
			treeID v;
			v=parse_bl(parser);
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
			body=parse_bl(parser);

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
			treeID body,v,step;
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

			treeID index_,value_;

			index_=tree_dummy(parser,name.line);
			v=tree_assign_mem(parser,tok.line,index_);
			block_add(parser,v);

			value_=tree_dummy(parser,name.line);
			v=tree_assign_mem(parser,tok.line,value_);
			block_add(parser,v);

			parser_bind(parser,name.line
			, ENTITY_REFERENCED|ENTITY_ASSIGNED|ENTITY_FORLOOP
			, name.text,value_);

			body=parse_bl(parser);
			ASSERT(body->kind==STAT_BLOCK);

			FOR_ARRAY(i,steps){
				step=v=steps[i];

				begin_block(parser);

				treeT range;
				range=get_tree(parser,step);
				if(range.kind==TREE_RANGE || range.kind==EXPR_RANGE_INDEX){
					treeID index,value,array,prev;
					index=index_;
					value=value_;
					array=prev=NO_TREE;

					if(range.kind==EXPR_RANGE_INDEX){
						array=range.x;
						range=get_tree(parser,range.y);
						ASSERT(range.kind==TREE_RANGE);
						ASSERT(index!=value);

						//todo: we use this so often, make intrinsic
						treeID *args=0;
						ARRAY_ADD(args,index);
						v=tree_call_metafield(parser,tok.line,array,args,"idx");

						prev=tree_store(parser,tok.line,value,v);
					}else{
						index=value;
					}

					v=tree_store(parser,tok.line,index,range.x);
					block_add(parser,v);

					treeID pred;
					pred=tree_less_than(parser,tok.line,index,range.y);

					treeID post;
					post=tree_binary(parser,tok.line,EXPR_ADD,NT_ANY,index,tree_int(parser,tok.line,1));
					post=tree_store(parser,tok.line,index,post);

					v=new_tree(parser,TREE_WHILE_LOOP,tok.line);
					v->stat_while.pred=pred;
					v->stat_while.prev=prev;
					v->stat_while.body=body;
					v->stat_while.post=post;
					block_add(parser,v);
				}else {
					v=tree_store(parser,tok.line,value_,step);
					block_add(parser,v);

					block_add(parser,body);
				}

				v=close_block(parser);
				block_add(parser,v);
			}

			close_scope(parser);
		} break;
		case TK_IF: case TK_IFF: {
			get_tok(parser);
			treeID v;
			v=parse_if(parser,tok.type==TK_IFF);
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
			v=parse_assign(parser,x);

			// if(peek_tok(parser,TK_ASSIGN)){
			// 	y=parse_expr(parser,0);
			// 	v=tree_store(parser,tok.line,x,y);
			// }
			block_add(parser,v);
		} break;
	}
	esc:
	return success;
}

static treeID parse_if(Parser *parser, int negate){
	tokenT tok;
	tok=parser->tok;

	begin_block(parser);

	treeID pred;
	pred=parse_expr(parser,0);
	//todo:deprecate
	if(negate){
		pred=tree_binary(parser,pred->line,EXPR_EQ,NT_BOL,pred,tree_int(parser,pred->line,0));
	}

	take_tok(parser,TK_QMARK);

	treeID true_clause;
	true_clause=parse_bl(parser);

	treeID else_clause;
	else_clause=NO_TREE;

	if(pick_tok(parser,TK_ELIF)) else_clause=parse_if(parser,0); else
	if(pick_tok(parser,TK_ELSE)) else_clause=parse_bl(parser);

	treeID v;
	v=tree_if(parser,tok.line,pred,true_clause,else_clause);

	// v=new_tree(parser,TREE_IF,tok.line);
	// v->stat_if.pred=pred;
	// v->stat_if.true_clause=true_clause;
	// v->stat_if.else_clause=else_clause;
	block_add(parser,v);

	return close_block(parser);
}

static int tok2prec(int type) {
	return tok2inf[type].prec;
}

#if 0
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

static treeID parse_unary(Parser *parser, jumpS *expr, bool flags) {
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

		case TK_M_REGISTER: {
			get_tok(parser);
			tk=take_tok(parser,TK_WORD);
			x=identify(parser,tk.text,0);
			if (x!=NO_ENTITY) {
				v=tree_int(parser,tk.line,parser->entities[x].args);
			} else parser_dialog(parser,tk.line,"'%s': invalid entity (must be a local)",tk.text);
		} break;
		/* todo: this is temporary */
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
				z=parse_args(parser);
				v=tree_call(parser,tk.line,v,z);
			} break;
			default: goto esc;
		}
	}

	esc:
	return v;
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
	jumpS js = {0};
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

	jumpS js = {0};
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

	jumpS js = {0};
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

	jumpS js = {0};
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
	treeID k = tree_binary(fs,NO_LINE,IR_ADD,NT_INT,index_node,tree_int(fs,NO_LINE,1));

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