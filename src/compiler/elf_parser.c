//
//	See Copyright Notice In elf.h
//

//
// todo: re-implement if, elif, then
// todo: re-implement #index
//
//

static treeID parse_unary(elf_Parser *parser, int flags);
static treeID parse_expr(elf_Parser *parser, int flags);
static treeID parse_table(elf_Parser *parser);
static treeID parse_subexpr(elf_Parser *parser, int flags, int rank);
static treeID parse_postfix(elf_Parser *parser, int flags);
static treeID parse_block(elf_Parser *parser);
static int parse_stat(elf_Parser *parser);
static int tok2tree(int tok);

static int token_precedence(int type) {
	return g_token_metadata_table[type].prec;
}

static void add_this_param(elf_Parser *parser, Source line);
static treeID parse_if(elf_Parser *parser, int negate);
static bool parse_for(elf_Parser *parser);
static treeID parse_json_expr(elf_Parser *parser);
static treeID *parse_args(elf_Parser *parser);
static void block_add(elf_Parser *parser, treeID id);
// the result is pushed onto the stack
static int parse_constexpr(elf_Parser *parser);

static void c_parser_init(elf_State *S, elf_Parser *parser, char *name, char *text) {
	ASSERT(S != 0);
	ASSERT(parser != 0);
	ASSERT(name != 0);
	ASSERT(text != 0);
	parser->R = S;
	parser->name = name;
	parser->text = text;
	parser->line_pos = text;
	parser->line_num = 1;
	parser->pos = text;
	get_tok(parser);
	get_tok(parser);
}

static void check_tree(elf_Parser *parser, Source line, treeID id) {
	if (id == NO_TREE){
		parser_dialog(parser,line,"invalid expression");
	}
}

static bool peek_tok(elf_Parser *parser, int type) {
	return parser->tok.type == type;
}
static bool pick_tok(elf_Parser *parser, int k) {
	return peek_tok(parser,k) && (get_tok(parser), 1);
}
static bool peek_tok_inl(elf_Parser *parser, int k) {
	return peek_tok(parser,k) && parser->tok_prev.eol != 1;
}
static bool pick_tok_inl(elf_Parser *parser, int k) {
	return peek_tok_inl(parser,k) && (get_tok(parser), 1);
}

static Token get_token_inline(elf_Parser *parser, int k) {
	Token tok = parser->tok;
	if (!pick_tok_inl(parser,k)) {
		parser_dialog(parser,tok.line,"expected '%s'\n",g_token_metadata_table[k].name);
	}
	return tok;
}

static Token take_tok(elf_Parser *parser, int k) {
	Token tok = parser->tok;
	if (!pick_tok(parser,k)) {
		parser_dialog(parser,parser->tok.line,"expected '%s'\n",g_token_metadata_table[k].name);
	}
	return tok;
}

static void begin_scope(elf_Parser *parser) {
	ASSERT(parser->scope_index < _countof(parser->scope_stack));
	parser->scope_stack[parser->scope_index ++] = parser->entity_index;
	parser->scope ++;
}
static void close_scope(elf_Parser *parser) {
	ASSERT(parser->scope_index > 0);
	parser->entity_index = parser->scope_stack[-- parser->scope_index];
	parser->scope --;
}

static void begin_loop(elf_Parser *parser) {
	ASSERT(parser->loop_index < _countof(parser->loop_stack));
	parser->loop_stack[parser->loop_index ++] = parser->loop;
	parser->loop=(Loop){};
}

static Loop close_loop(elf_Parser *parser) {
	Loop loop = parser->loop;
	ASSERT(parser->loop_index > 0);
	parser->loop = parser->loop_stack[-- parser->loop_index];
	return loop;
}

static void block_add(elf_Parser *parser, treeID id){
	ARRAY_ADD(parser->block.body,id);
}

static void begin_block(elf_Parser *parser) {
	if(parser->block_index >= _countof(parser->block_stack)){
		parser_dialog(parser,parser->tok.line,"block nesting too deep");
	}
	ASSERT(parser->block_index < _countof(parser->block_stack));
	parser->block_stack[parser->block_index ++] = parser->block;
	parser->block = (Block){};
	begin_scope(parser);
}

// this is only meant to be called when you reach the end
// of a syntactic block, not when you reach a block
// terminating instruction.
static treeID close_block(elf_Parser *parser) {
	ASSERT(parser->block_index > 0);
	close_scope(parser);
	Block block = parser->block;

	// todo: we actually have to do this whenever
	// we end a block with any block terminating
	// instruction!
	//
	// So say for instance a block has a bunch of
	// defers and also a bunch of breaks and returns
	// here and there, we would have to copy paste
	// all the defers at each terminating instruction,
	// I think is best to simply take all the defers
	// and but them past the default "return" instruction
	// and when a block terminating instruction is reached
	// then jump to the where all the defers were generated.
	//
	//
	FOR_ARRAY(i, block.defers) {
		ARRAY_ADD(block.body, block.defers[i]);
	}

	parser->block = parser->block_stack[-- parser->block_index];
	return tree_block(parser,parser->tok.line,block.body);
}

static entityID identify_by_tree(elf_Parser *parser, treeID tree) {
	entityID id;
	entityT ent;
	for (id = parser->entity_index-1; id >= 0; -- id) {
		ent=parser->entities[id];
		if (ent.tree==tree) {
			return id;
		}
	}
	return NO_ENTITY;
}

static entityID identify_by_name(elf_Parser *parser, char *name) {
	entityID id;
	entityT ent;
	for (id = parser->entity_index-1; id >= 0; -- id) {
		ent=parser->entities[id];
		if (text_eq(ent.name,name)) {
			parser->entities[id].status |= ENTITY_REFERENCED;
			return id;
		}
	}
	return NO_ENTITY;
}
static entityID parser_bind(elf_Parser *parser, Source line, int flags, char *name, treeID tree) {
	treeID enc;
	enc=parser->enc;

	{
		entityID entity_id;
		entity_id=identify_by_name(parser,name);

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

	entityT entity = {};
	entity.kind = ENTITY_LOCAL;
	entity.status = flags;
	entity.scope = parser->scope;
	entity.tree = tree;
	entity.name = name;
	entity.line = line;
	parser->entities[entity_id]=entity;
	return entity_id;
}

static void add_this_param(elf_Parser *parser, Source line) {
	treeID param,x;
	param=tree_nop(parser,line);

	x=tree_assign_mem(parser,line,param);
	block_add(parser,x);

	parser_bind(parser, line
	, ENTITY_PARAMETER|ENTITY_ASSIGNED|ENTITY_CONSTANT|ENTITY_REFERENCED
	, "this", param);
}

static treeID parse_fun(elf_Parser *parser) {
	Token tok;

	tok=take_tok(parser,TK_FUN);

	begin_block(parser);

	treeID enc=parser->enc;

	treeID fun=new_tree(parser,tok.line,TREE_FUNCTION,NT_FUN);
	fun->expr_fun.scope=parser->scope;
	fun->expr_fun.enc=enc;
	parser->enc=fun;

	take_tok(parser,TK_PAREN_LEFT);

	add_this_param(parser,tok.line);

	if (!peek_tok(parser,TK_PAREN_RIGHT)) do {
		Token name;
		treeID para,expr;

		name=take_tok(parser,TK_WORD);
		para=tree_nop(parser,name.line);
		expr=tree_assign_mem(parser,name.line,para);
		block_add(parser,expr);

		parser_bind(parser, name.line
		, ENTITY_ASSIGNED|ENTITY_PARAMETER
		, name.text,para);
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
			ent=identify_by_name(parser,"this");
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
static treeID parse_load(elf_Parser *parser){
	Token tok;
	tok=take_tok(parser,TK_LOAD);

	treeID v,*z;
	z=parse_args(parser);

	v=tree_global_symbol(parser,tok.line,"elf.load_file");
	v=tree_call(parser,tok.line,v,z);
	return v;
}

//
// 'new' <meta-table> ( <argument-list> )
//
// ret elf.set_meta({},Vector2):__new(x,y)
//
// todo: alot of these things call "builtin" functions by name,
// instead have an enum to guarantee they exist no matter what
//
static treeID parse_new(elf_Parser *parser){
	Token tok;
	tok=take_tok(parser,TK_NEW);

	treeID callexpr=parse_postfix(parser,0);
	if(callexpr->kind!=TREE_CALL){
		parser_dialog(parser,tok.line,"invalid expression");
	}

	treeID meta,*args;

	args=callexpr->z;
	meta=callexpr->x;

	treeID table;
	/* check if we can reuse the table literal the user passed */
	if ((ARRAY_LENGTH(args) == 1) && (get_tree_kind(parser,args[0]) == TREE_NEW_TABLE)) {
		table=args[0];
	} else {
		table=tree_table(parser,tok.line);
	}

	/* relies on the builtin function "set_metatable(obj, metatable) -> metatable"
	returning the metatable */
	table=tree_call_set_meta(parser,tok.line,table,meta);

	treeID v=tree_str(parser,get_tree_line(parser, meta),"__new");
	/* todo: this is sort of inefficient, but if we don't issue
	the call instruction with a meta-field, the generator won't
	insert the 'this' parameter */
	v=tree_meta_field(parser,get_tree_line(parser, meta),table,v);
	v=tree_call(parser,get_tree_line(parser, meta),v,args);
	return v;
}
static treeID parse_unary(elf_Parser *parser, bool flags) {
	Token tok;
	treeID v,x;

	v=NO_TREE;
	tok=parser->tok;

	switch (tok.type) {
		case TK_M_GETMEM: {
			get_tok(parser);
			v=parse_subexpr(parser,0,10000);
			v=tree_unary(parser,tok.line,TREE_GETMEM,NT_INT,v);
		} break;
		case TK_M_GETEXPR: {
			get_tok(parser);
			v=parse_subexpr(parser,0,10000);
			v=tree_unary(parser,tok.line,TREE_GETEXPR,NT_INT,v);
		} break;
		// todo: intrinsic!
		case TK_SUB: {
			get_tok(parser);
			v=parse_subexpr(parser,0,10000);
			v=tree_binary(parser,tok.line,EXPR_SUB,NT_ANY,tree_int(parser,tok.line,0),v);
		} break;
		case TK_ADD: {
			get_tok(parser);
			v = parse_subexpr(parser,0,10000);
		} break;
		case TK_JSON: {
			v = parse_json_expr(parser);
		} break;
		case TK_NEW: {
			v = parse_new(parser);
		} break;
		case TK_LOAD: {
			v = parse_load(parser);
		} break;
		case TK_CURLY_LEFT: {
			v=parse_table(parser);
		} break;
		/* empty ranges are interpreted accordingly */
		case TK_DOT_DOT: {
			get_tok(parser);
			v = tree_nullary(parser,tok.line,TREE_RANGE,NT_ANY);
		} break;
		// todo: dot syntax is to be repurposed
		case TK_DOT:
		case TK_ELF: {
			char sym[256] = {};

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

			v=tree_global_symbol(parser,tok.line,sym);
		} break;
		case TK_WORD: {
			entityID id;
			entityT entity;
			char *name;

			get_tok(parser);

			name=tok.text;
			id=identify_by_name(parser,name);

			if (id!=NO_ENTITY) {
				entity=parser->entities[id];
				if (~entity.status & ENTITY_ASSIGNED) {
					parser_dialog(parser,tok.line,"warning: usage of possibly unassigned variable");
				}
				// check if we have to capture this thing
				treeID enc=parser->enc;
				if (entity.scope<enc->expr_fun.scope) {
					int index = -1;
					// check if we've captured this already
					FOR_ARRAY(i,enc->expr_fun.capts) {
						if (enc->expr_fun.capts[i] == entity.tree){
							index = i;
							goto already_captured;
						}
					}
					index=ARRAY_LENGTH(enc->expr_fun.capts);
					ARRAY_ADD(enc->expr_fun.capts,entity.tree);
					already_captured:
					v=tree_closure_value(parser,tok.line,index);

					enc=enc->expr_fun.enc;
					if (entity.scope<enc->expr_fun.scope) {
						parser_dialog(parser,tok.line,"cannot capture?");
					}
				} else {
					v=entity.tree;
				}
			} else {
				// todo: I dislike this system very much...
				// globals should be explicitly declared,
				// otherwise you get some very annoying errors
				// every now and then because you misspelled
				// something and then it thinks that it is a
				// global, instead, you should use a global
				// keyword
				v=tree_global_symbol(parser,tok.line,name);
				// parser_dialog(parser,tok.line,"undeclared entity");
				// exit(1);
			}
		} break;
		case TK_PAREN_LEFT: {
			get_tok(parser);
			if(!peek_tok(parser,TK_PAREN_RIGHT)){
				v=parse_expr(parser,0);
			}
			take_tok(parser,TK_PAREN_RIGHT);
			goto no_err;
		} break;
		case TK_FUN: {
			v=parse_fun(parser);
		} break;
		case TK_NIL: {
			get_tok(parser);
			v = tree_nil(parser,tok.line);
		} break;
		case TK_TRUE:{
			get_tok(parser);
			v = tree_int(parser,tok.line,1);
		} break;
		case TK_FALSE: {
			get_tok(parser);
			v = tree_int(parser,tok.line,0);
		} break;
		case TK_LETTER: case TK_INTEGER: {
			get_tok(parser);
			v=tree_int(parser,tok.line,tok.integer);
		} break;
		case TK_NUMBER: {
			get_tok(parser);
			v = tree_num(parser,tok.line,tok.number);
		} break;
		case TK_STRING: {
			get_tok(parser);
			v = tree_str(parser,tok.line,tok.text);
		} break;
		default: goto err;
	}

	if (v == NO_TREE) {
		err:
		parser_dialog(parser,tok.line,"'%s': unexpected token", g_token_metadata_table[tok.type].name);
		elf_error(parser->R,0,"syntax error: unexpected token");
	}
	no_err:
	return v;
}

static treeID parse_table(elf_Parser *parser) {
	Token tok = take_tok(parser,TK_CURLY_LEFT);
	int index = 0;
	treeID table = tree_table(parser,tok.line);
	treeID prev = table;

	tok = parser->tok;
	for (;(tok.type != TK_NONE) && (tok.type != TK_CURLY_RIGHT); tok = parser->tok) {
		treeID key, value;
		key = value = NO_TREE;

		if ((tok.type == TK_WORD) && (parser->tok_prox.type == TK_ASSIGN)) {
			tok = get_tok(parser);
			key = tree_str(parser,tok.line,tok.text);
			if (key == NO_TREE) goto _err;
		} else {
			key = value = parse_expr(parser,0);
			if (key == NO_TREE) goto _err;
		}
		tok = parser->tok;
		if (pick_tok(parser,TK_ASSIGN)) {
			value = parse_expr(parser,0);
			if (value == NO_TREE) goto _err;
		} else {
			key = tree_int(parser,tok.line,index++);
			if (key == NO_TREE) goto _err;
		}

		ASSERT(value != NO_TREE);
		ASSERT(key != NO_TREE);

		check_tree(parser,tok.line,key);
		check_tree(parser,tok.line,value);
		tok = parser->tok;

		treeID field = tree_field(parser,tok.line,table,key);
		treeID store = tree_store(parser,tok.line,field,value);
		prev = prev->prox = store;

		if (pick_tok(parser,TK_COMMA)) {
			continue;
		}
	}
	take_tok(parser,TK_CURLY_RIGHT);

	return table;
	_err:
	parser_dialog(parser,parser->tok.line,"invalid field intializer");
	return table;
}

//
// {x} | ( x { ... } ) | { <table-initializer-list> }
//
static treeID *parse_args(elf_Parser *parser) {
	treeID x;
	treeID *z,*n;
	z=0;
	if (peek_tok(parser,TK_CURLY_LEFT)) {
		x=parse_table(parser);
		ARRAY_ADD(z,x);
	} else if (pick_tok(parser,TK_PAREN_LEFT)) {
		pick_tok(parser,TK_COMMA);
		if (!peek_tok(parser,TK_PAREN_RIGHT)) do {
			x=parse_expr(parser,0);
			if(x!=NO_TREE){
				// desugar tuple expressions
				if (get_tree_kind(parser,x)==TREE_TUPLE) {
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

// parse a postfix expression
static treeID parse_postfix(elf_Parser *parser, int flags) {
	Token tok;
	treeID v;

	tok=parser->tok;
	v=parse_unary(parser,flags);

	while(parser->tok.type != TK_NONE && !parser->tok_prev.eol) {
		tok=parser->tok;

		switch (tok.type) {
			case TK_DOT: {
				get_tok(parser);
				// table.(x,y) -> (table.x, table.y)
				if (pick_tok(parser,TK_PAREN_LEFT)) {
					Token n;
					treeID x,y,*z=0;
					do {
						n=take_tok(parser,TK_WORD);
						y=tree_str(parser,n.line,n.text);
						x=tree_field(parser,tok.line,v,y);
						ARRAY_ADD(z,x);
					} while (pick_tok(parser,TK_COMMA));
					v = tree_tuple(parser,tok.line,z);
					take_tok(parser,TK_PAREN_RIGHT);
				} else
				// table.{x,y}
				if (pick_tok(parser,TK_CURLY_LEFT)) {
					NO_CODE;
				} else {
					Token name;
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
					x = parse_expr(parser,0);
					if (x == NO_TREE) break;
					//
					// apply desugaring
					// A [ B . (y, x) ] -> A [ B . y, B . x ]
					//
					if (get_tree_kind(parser,x) == TREE_TUPLE) {
						z = get_tree(parser,x).z;
						FOR_ARRAY(i,z) {
							v = tree_index(parser,tok.line,v,z[i]);
						}
					} else if (get_tree_kind(parser,x) == TREE_RANGE) {
						v = tree_ranged_index(parser,tok.line,v,x);
					} else {
						v = tree_index(parser,tok.line,v,x);
					}
				} while(pick_tok(parser,TK_COMMA));
				take_tok(parser,TK_SQUARE_RIGHT);
			} break;
			case TK_COLON: {
				Token n;
				treeID y;
				get_tok(parser);
				n=take_tok(parser,TK_WORD);
				y=tree_str(parser,n.line,n.text);
				v=tree_meta_field(parser,tok.line,v,y);
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

static treeID parse_subexpr(elf_Parser *parser, int flags, int rank) {
	ASSERT(flags==0);

	Token tok = parser->tok;

	treeID x,y;

	x = parse_postfix(parser,flags);
	if (x == NO_TREE) goto esc;
	if (x->type == NT_NON) {
		parser_dialog(parser,x->line,"invalid data class");
		goto esc;
	}

	for(;;) {
		tok=parser->tok;

		int prio = token_precedence(tok.type);
		if (prio <= rank) goto esc;

		if (parser->tok_prox.type == TK_ASSIGN) {
			goto esc;
		}

		get_tok(parser);

		y = parse_subexpr(parser,flags,prio);
		if (y == NO_TREE) goto esc;

		if(y->type == NT_NON){
			parser_dialog(parser,y->line,"invalid data class, no data operand");
			goto esc;
		}

		x = tree_binary(parser,tok.line,tok2tree(tok.type),NT_ANY,x,y);
	}

	esc:
	return x;
}

static treeID parse_expr(elf_Parser *parser, int flags) {
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

// todo:just use the same enum for the token and the tree
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

static treeID *parse_expr_list(elf_Parser *parser) {
	treeID y,*yz,*z=0;
	do{
		y=parse_expr(parser,0);
		if(get_tree_kind(parser,y)==TREE_TUPLE) {
			yz=get_tree(parser,y).z;
			FOR_ARRAY(i,yz) ARRAY_ADD(z,yz[i]);
		} else {
			ARRAY_ADD(z,y);
		}
	}while(pick_tok(parser,TK_COMMA));
	return z;
}

static treeID parse_block (elf_Parser *parser) {
	begin_block(parser);

	if (pick_tok(parser,TK_CURLY_LEFT)) {
		while (parse_stat(parser));
		take_tok(parser,TK_CURLY_RIGHT);
	} else {
		parse_stat(parser);
	}
	return close_block(parser);
}

static Loop *get_loop(elf_Parser *parser, treeID name) {
	if(parser->loop_index==0) return 0;
	int index;
	Loop *loop;

	index=parser->loop_index-1;
	loop=&parser->loop;
	do {
		if ((name == NO_TREE) || (loop->name == name)) {
			return loop;
		}
	}while(-- index >= 0);

	return 0;
}

static void check_assign(elf_Parser *parser, Source line, treeID tree){
	if(tree->type==NT_NON){
		parser_dialog(parser,line
		,	"invalid data class");
	}
	entityID e = identify_by_tree(parser,tree);
	if(e != NO_ENTITY){
		if (parser->entities[e].status & ENTITY_CONSTANT){
			parser_dialog(parser,line
			,	"reassignment of constant entity");
			parser_dialog(parser,parser->entities[e].line
			,	"see declaration");
		}
	}
}

static int parse_stat(elf_Parser *parser) {
	int success = true;
	Token tok = parser->tok;

	switch (tok.type) {
		case TK_NONE: case TK_CURLY_RIGHT:
		case TK_THEN: case TK_ELSE: case TK_ELIF: {
			return 0;
		}
	}

	switch (tok.type) {
		case TK_DEFER:
		case TK_LASTLY:
		case TK_FINALLY: {
			get_tok(parser);
			if (tok.type != TK_DEFER) {
				parser_dialog(parser,tok.line,"consider using defer instead!");
			}
			treeID v = parse_block(parser);
			ARRAY_ADD(parser->block.defers, v);
		} break;

		// todo: deprecate leave!
		case TK_LEAVE:
		case TK_RET:
		case TK_HARD_ARROW:
		case TK_BREAK:
		case TK_CONTINUE: {
			get_tok(parser);

			if(tok.type==TK_LEAVE){
				parser_dialog(parser,tok.line,"consider using 'ret' instead!");
			}
			parser->block.ended=1;
			parser->block.has_ret=1;

			treeID v = NO_TREE;
			if(!tok.eol) {
				v = parse_expr(parser,0);
			}
			if((tok.type == TK_CONTINUE) || (tok.type == TK_BREAK)) {
				Loop *loop = get_loop(parser,v);
				if(loop != 0){
					ASSERT(loop != parser->loop_stack);
					v = tree_goto(parser,tok.line);
					if(tok.type == TK_CONTINUE) {
						ARRAY_ADD(loop->continues,v);
					} else {
						ARRAY_ADD(loop->breaks,v);
					}
				} else {
					parser_dialog(parser,tok.line,"not in a loop");
				}
			} else {
				v = tree_ret(parser,tok.line,v);
			}
			block_add(parser,v);
		} break;
		case TK_CURLY_LEFT: {
			treeID v = parse_block(parser);
			block_add(parser,v);
		} break;
		#if 0
		case TK_LET: {
			treeID value,v;
			char *name;
			get_tok(parser);
			name=take_tok(parser,TK_WORD).text;
			take_tok(parser,TK_ASSIGN);
			value=parse_expr(parser,0);
			v=tree_unary(parser,tok.line,TREE_SETMEM,NT_ANY,value);
			block_add(parser,v);
			parser_bind(parser,tok.line,ENTITY_ASSIGNED,name,value);
		} break;
		#endif
		case TK_WHILE: {
			get_tok(parser);
			treeID pred = parse_expr(parser,0);
			take_tok(parser,TK_QMARK);

			begin_loop(parser);
			treeID body = parse_block(parser);
			Loop loop = close_loop(parser);

			treeID v;
			// begin_block(parser);
			{
				v = new_tree(parser,tok.line,TREE_WHILE_LOOP,NT_NON);
				v->loop.pred = pred;
				v->loop.body = body;
				v->loop.c = loop.continues;
				v->loop.b = loop.breaks;
				block_add(parser,v);
			}
			// v = close_block(parser);

			block_add(parser,v);
		}break;
		//todo:
		// for 0...100 ? {
		// 	#index
		// }
		case TK_FOR: {
			parse_for(parser);
		} break;
		case TK_IF: case TK_IFF: {
			get_tok(parser);
			treeID v = parse_if(parser,tok.type==TK_IFF);
			block_add(parser,v);
		} break;
		default: {
			bool parse_decl = (parser->tok.type == TK_WORD) && (parser->tok_prox.type == TK_BIND || parser->tok_prox.type == TK_HARD_BIND);
			if (parse_decl) {
				char *name = take_tok(parser,TK_WORD).text;

				int flags = ENTITY_ASSIGNED;
				if (pick_tok(parser,TK_HARD_BIND)) {
					flags |= ENTITY_CONSTANT;
				} else if(pick_tok(parser,TK_BIND)) {
				} else {
					ASSERT(!"WUT?");
				}

				treeID v = parse_expr(parser,0);
				// parse_expr could return a reference to some
				// other tree that has already been assigned
				// memory, set memory only will only assign new
				// memory to trees that have no memory assigned
				// to them, so we do set mem on a load tree, so
				// we essentially wrap the tree.
				// todo: also, if the entity is constant,
				// there's no need to allocate more memory
				// for this tree, it can just act as
				// an alias, so maybe this is a reason to
				// have set mem be strict about whether the
				// tree already had memory or not
				v = tree_load(parser,tok.line,v);

				parser_bind(parser,tok.line,flags,name,v);

				// todo: i disabled this because I didn't exactly
				// check how this would play out with closures...
				// I assume the code that checks whether to capture
				// something or not would have to check whether this
				// is a constant or not too, maybe we could set a flag
				// or something.
				// Or maybe the entity should just be the
				// constant value...
				// if ((flags & ENTITY_CONSTANT) && is_tree_trivial_constant(parser,v)) {
				// 	// tree is simple enough that there's no
				// 	// need to allocate memory for it.
				// } else
				{
					v = tree_assign_mem(parser,tok.line,v);
				}
				block_add(parser,v);
			} else {
				treeID v = parse_expr(parser,0);
				if (v == NO_TREE) {
					parser_dialog(parser,tok.line,"invalid statement");
					success = 0;
					goto esc;
				}
				// int mem;
				// x=desugar_range_expr(parser,lexpr,0);
				// x=emit_preload_deprecated(parser,x);
				tok = parser->tok;

				treeID y;
				if (pick_tok(parser,TK_ASSIGN)) {
					check_assign(parser,tok.line,v);

					y = parse_expr(parser,0);
					v = tree_store(parser,tok.line,v,y);
				} else if (pick_tok(parser,TK_NIL_ASSIGN)) {
					check_assign(parser,tok.line,v);
					y = parse_expr(parser,0);
					v = tree_if(parser,tok.line
					, tree_binary(parser,tok.line,EXPR_EQ,NT_ANY,v,tree_nil(parser,tok.line))
					, tree_store(parser,tok.line,v,y)
					, 0);
				} else if (token_precedence(tok.type) > 0) {
					check_assign(parser,tok.line,v);

					tok = get_tok(parser);
					take_tok(parser,TK_ASSIGN);

					y = parse_expr(parser,0);
					y = tree_binary(parser,tok.line,tok2tree(tok.type),NT_ANY,v,y);
					v = tree_store(parser,tok.line,v,y);
				}
				// desugar_range_expr_epilogue(parser,lexpr);
				// set_mem_state_deprecated(parser,mem);
				ASSERT(v != NO_TREE);
				block_add(parser,v);
			}
		} break;
	}
	esc:
	return success;
}

static treeID parse_if(elf_Parser *parser, int negate){

	Token tok = parser->tok;
	begin_block(parser);

	treeID pred = parse_expr(parser,0);

	// todo:deprecate
	if (negate) {
		pred = tree_binary(parser,pred->line,EXPR_EQ,NT_BOL,pred,tree_int(parser,pred->line,0));
	}

	take_tok(parser,TK_QMARK);

	treeID true_clause = parse_block(parser);
	treeID else_clause = NO_TREE;
	if (pick_tok(parser,TK_ELIF)) else_clause = parse_if(parser,0); else
	if (pick_tok(parser,TK_ELSE)) else_clause = parse_block(parser);

	treeID v = tree_if(parser,tok.line,pred,true_clause,else_clause);
	block_add(parser,v);
	return close_block(parser);
}

static bool parse_for(elf_Parser *parser){
	bool noerror = true;

	Token tok = take_tok(parser,TK_FOR);

	Token name = take_tok(parser,TK_WORD);
	take_tok(parser,TK_ASSIGN);

	// parse all the steps, for i = < X, Y, Z, ... > ?
	treeID v, *steps = 0;
	do {
		v = parse_expr(parser,0);
		if (v == NO_TREE) {
			noerror = false;
			goto esc;
		}
		ARRAY_ADD(steps, v);
	} while (pick_tok(parser,TK_COMMA));

	take_tok(parser, TK_QMARK);

	treeID index_, value_;

	// create memory locations for index and value variables
	index_ = tree_nop(parser,name.line);
	v = tree_assign_mem(parser,tok.line,index_);
	block_add(parser,v);

	value_ = tree_nop(parser,name.line);
	v = tree_assign_mem(parser,tok.line,value_);
	block_add(parser,v);

	// begin a new scope (the scope contains the entire for loop)
	begin_scope(parser);

	// bind the name to the value variable, for <i> = ... ?
	parser_bind(parser, name.line
	, ENTITY_REFERENCED|ENTITY_ASSIGNED|ENTITY_FORLOOP
	, name.text, value_);

	// begin a new loop
	begin_loop(parser);

	// parse the body, for i = ... ? < { ... } >
	treeID body = parse_block(parser);
	ASSERT(body->kind == STAT_BLOCK);

	//
	// a for loop declares a variable that can take on multiple values.
	//
	// each step of the loop is a new value.
	//
	// A step can be single a values or a range.
	//
	// A ranges translates to an actual loop, single values to stores.
	//

	// generate loops or stores for each step
	FOR_ARRAY(i, steps) {
		treeID step = steps[i];

		begin_block(parser);


		// check if the step translates into a loop
		if (step->kind == TREE_RANGE || step->kind == EXPR_RANGE_INDEX) {

			// we have two registers, one for the index, one for the value
			treeID index = index_;
			treeID value = value_;

			treeID rangelo = NO_TREE, rangehi = NO_TREE;
			treeID prebody = NO_TREE;

			if (step->kind == EXPR_RANGE_INDEX) {
				ASSERT(step->x != NO_TREE);
				ASSERT(step->y != NO_TREE);

				treeID array = step->x;
				treeID range = step->y;
				ASSERT(range->kind == TREE_RANGE);

				rangelo = range->x;
				rangehi = range->y;

				// index array and store in value register
				treeID *args = 0;
				ARRAY_ADD(args, index);
				v = tree_meta_call(parser,tok.line,array,args,"idx");
				prebody = tree_store(parser,tok.line,value,v);

				rangelo = range->x, rangehi = range->y;

				if (rangelo == NO_TREE) rangelo = tree_int(parser,tok.line,0);
				if (rangehi == NO_TREE) rangehi = tree_meta_call(parser,tok.line,array,0,"length");
			} else {

				// value same as index
				index = value;

				rangelo = step->x, rangehi = step->y;

				// todo: true infinite
				if(rangehi == NO_TREE) rangehi = tree_int(parser,tok.line,0xffffffffLLU);
				if(rangelo == NO_TREE) rangelo = tree_int(parser,tok.line,0);
			}

			ASSERT(rangelo != NO_TREE);
			ASSERT(rangehi != NO_TREE);


			// initialize index
			v = tree_store(parser,tok.line,index,rangelo);
			block_add(parser,v);

			// generate predicate
			treeID pred = tree_less_than(parser,tok.line,index,rangehi);

			// what to do after the loop body's
			treeID probody;
			probody = tree_binary(parser,tok.line,EXPR_ADD,NT_ANY,index,tree_int(parser,tok.line,1));
			probody = tree_store(parser,tok.line,index,probody);

			v = new_tree(parser,tok.line,TREE_WHILE_LOOP,NT_NON);
			v->loop.pred = pred;
			v->loop.body = body;
			v->loop.prebody = prebody;
			v->loop.probody = probody;
			v->loop.c = parser->loop.continues;
			v->loop.b = parser->loop.breaks;
			block_add(parser,v);
		} else {
			// todo: should still store 0 to index?
			v = tree_store(parser,tok.line,value_,step);
			block_add(parser,v);
			block_add(parser,body);

			// todo: actually define this semantically...
			if(parser->loop.continues || parser->loop.breaks){
				FOR_ARRAY(i,parser->loop.continues){
					parser_dialog(parser
					, parser->loop.continues[i]->line,"breaks and continues are only for ranges");
				}
				FOR_ARRAY(i,parser->loop.breaks){
					parser_dialog(parser
					, parser->loop.breaks[i]->line,"breaks and continues are only for ranges");
				}
				elf_error(parser->R,0,"invalid syntax");
			}
		}

		v = close_block(parser);
		block_add(parser,v);
	}

	close_loop(parser);
	close_scope(parser);
	esc:
	return noerror;
}

//	static bool iskeyexpr(int tok) {
//		return tok == TK_INTEGER || tok == TK_NUMBER || tok == TK_STRING || tok == TK_WORD;
//	}

static void checkstk(elf_Parser *parser, int state) {
	int index = parser->R->stack_ptr - parser->R->stack;
	if (index != state) {
		parser_dialog(parser, parser->tok.line, "internal error, invalid stack state");
		elf_error(parser->R, NO_BYTE, "internal error, invalid stack state");
	}
}
// todo: legitimize
// todo: also json is a constant
static int parse_constexpr(elf_Parser *parser) {
	Token tok = parser->tok;
	int ret = -1;
	int sign = 1;
	switch (tok.type) {
		case TK_ADD: case TK_SUB: {
			sign = (tok.type == TK_ADD) * 2 - 1;
			get_tok(parser);
			if (peek_tok(parser, TK_NUMBER)) goto numcase;
			else if (peek_tok(parser, TK_INTEGER)) goto intcase;
			parser_dialog(parser, parser->tok.line, "expected integer or number after '-'");
			goto errorcase;
		} break;
		case TK_NUMBER: { numcase:
			get_tok(parser);
			elf_push_num(parser->R, tok.number * sign);
			ret = 1;
		} break;
		case TK_INTEGER: { intcase:
			get_tok(parser);
			elf_push_int(parser->R, tok.integer * sign);
			ret = 1;
		} break;
		case TK_NIL: {
			get_tok(parser);
			elf_push_nil(parser->R);
			ret = 1;
		} break;
		case TK_STRING: {
			get_tok(parser);
			elf_push_string(parser->R, tok.text);
			ret = 1;
		} break;
		case TK_CURLY_LEFT: {
			get_tok(parser);

			int tablestk = elf_push_table(parser->R);

			while(!peek_tok(parser,TK_NONE) && !peek_tok(parser,TK_CURLY_RIGHT)) {

				// push key (or value)
				if (pick_tok(parser, TK_WORD)) {

					elf_push_string(parser->R, parser->tok.text);

				} else  {

					ret = parse_constexpr(parser);
					// todo: instead attempt to do some error recovery
					if (ret == -1) goto esc;

				}

				if (pick_tok(parser, TK_ASSIGN)) {

					// push value
					ret = parse_constexpr(parser);
					// todo: instead attempt to do some error recovery
					if (ret == -1) goto esc;

					checkstk(parser, tablestk + 2 + 1);

					elf_table_set(parser->R);
				} else {

					checkstk(parser, tablestk + 1 + 1);

					// keyless entry
					elf_array_add(parser->R);
				}

				pick_tok(parser,TK_COMMA);
			}
			take_tok(parser,TK_CURLY_RIGHT);

			checkstk(parser, tablestk + 1);
			ret = 1;

			goto esc;

		} break;
		default: {
			errorcase:
			elf_push_nil(parser->R);
			parser_dialog(parser,tok.line,"not a constant expression");
		} break;
	}

	esc:
	return ret;
}

static elf_Table * c_parse_json(elf_Parser *parser);

//
// todo: we won't screw anything else by
// adding stuff to the stack right?
//
//	todo: we're adding this to the globals,
//	should add to some other pool?
//
//
static treeID parse_json_expr(elf_Parser *parser){

	Token tok = take_tok(parser,TK_JSON);
	elf_Table * tab = c_parse_json(parser);
	int gid = elf_set_global(parser->R, 0, VALUE_TABLE(tab));

	return tree_global(parser,tok.line,gid);
}

static elf_Value parse_json_value(elf_Parser *parser);

static elf_Table *parse_json_array(elf_Parser *parser){
	elf_Table *arr = elf_new_table(parser->R);
	take_tok(parser,TK_SQUARE_LEFT);
	if(!pick_tok(parser,TK_SQUARE_RIGHT)) do {
		elf_Value val = parse_json_value(parser);
		elf_array_add_raw(arr,val);
	} while(pick_tok(parser,TK_COMMA));
	take_tok(parser,TK_SQUARE_RIGHT);
	return arr;
}

static elf_Table * c_parse_json(elf_Parser *parser){
	Token tok;
	elf_Table * table;

	table = elf_new_table(parser->R);
	elf_Value key,val;


	take_tok(parser,TK_CURLY_LEFT);
	if (!peek_tok(parser,TK_CURLY_RIGHT)) do {
		tok=take_tok(parser,TK_STRING);
		key=VALUE_STRING(elf_new_string(parser->R,tok.text));
		take_tok(parser,TK_COLON);
		val=parse_json_value(parser);
		elf_table_set_raw(table,key,val);
	} while(pick_tok(parser,TK_COMMA));
	take_tok(parser,TK_CURLY_RIGHT);

	return table;
}

static elf_Value parse_json_value(elf_Parser *parser) {
	elf_Value val = VALUE_NIL();
	elf_Table * obj;
	Token tok = parser->tok;
	switch (tok.type) {
		case TK_STRING: {
			get_tok(parser);
			val = VALUE_STRING(elf_new_string(parser->R,tok.text));
		} break;
		case TK_INTEGER: {
			get_tok(parser);
			val = VALUE_INTEGER(tok.integer);
		} break;
		case TK_NUMBER: {
			get_tok(parser);
			val = VALUE_INTEGER(tok.number);
		} break;
		case TK_CURLY_LEFT: {
			obj = c_parse_json(parser);
			val = VALUE_TABLE(obj);
		} break;
		case TK_SQUARE_LEFT: {
			obj = parse_json_array(parser);
			val = VALUE_TABLE(obj);
		} break;
		default: {
			parser_dialog(parser,tok.line,"invalid json value");
		} break;
	}
	return val;
}




#if 0
		case TK_M_INDEX: case TK_M_ARRAY: case TK_M_VALUE: {
			get_tok(parser);
			int reg;

			if (tk.type==TK_M_ARRAY) reg=SPECIAL_REGISTER_ARRAY; else
			if (tk.type==TK_M_VALUE) reg=SPECIAL_REGISTER_VALUE; else reg=SPECIAL_REGISTER_INDEX;

			v=tree_load(parser,tk.line,reg);
		} break;
		case TK_M_REGISTER: {
			get_tok(parser);
			tk=take_tok(parser,TK_WORD);
			x=identify_by_name(parser,tk.text,0);
			if (x!=NO_ENTITY) {
				v=tree_int(parser,tk.line,parser->entities[x].args);
			} else parser_dialog(parser,tk.line,"'%s': invalid entity (must be a local)",tk.text);
		} break;
		/* todo: this is temporary */
	if (~flags & EXPR_ALLOW_POSTFIX) {
		goto esc;
	}
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
}
#endif


