//
//	See Copyright Notice In elf.h
//



// Todos:
// - better format string parsing system, maybe the lexer
// switches to a different mode
// - re-implement if, elif, then
// - breaks in any block will leave the block
// - fix defer statements!
// - global declarations, global keyword to make
// accessing globals explicit!
//
//
//		something_undeclared = 1
//		; elf complains, what are you talking about?
//
//		global something_undeclared = 1
//		; now elf knows, less error prone
//
//		local_version := global something_undeclared
//
//		something_undeclared :: global
//
//
//
//
//
//



static treeID parse_unary(elf_Parser *parser, int flags);
static treeID parse_expr(elf_Parser *parser, int flags);
static treeID parse_table(elf_Parser *parser);
static treeID parse_subexpr(elf_Parser *parser, int flags, int rank);
static treeID parse_postfix(elf_Parser *parser, int flags);
static treeID parse_block(elf_Parser *parser);
static int tok2tree(int tok);
static int parse_stat(elf_Parser *parser);

// the result is pushed onto the stack
static int parse_json_object(elf_Parser *parser);
static int parse_json_value(elf_Parser *parser);
static int parse_constexpr(elf_Parser *parser);

static int tokenrank(int type) {
	return g_token_metadata_table[type].rank;
}

static void add_this_param(elf_Parser *parser, Source line);
static treeID parse_if(elf_Parser *parser);
static bool parse_for(elf_Parser *parser);
static treeID *parse_args(elf_Parser *parser);
static void block_add(elf_Parser *parser, treeID id);



static void parser_restart(elf_Parser *parser, char *cursor) {
	parser->cursor = cursor;
	// so that I don't forget to do this!
	next_tok(parser);
	next_tok(parser);
}



static void elf_end_parser(elf_Parser *parser) {
	sys_virtual_free(parser->tree_memory);
	free(parser);
}


static elf_Parser *elf_new_parser(elf_State *inter, const char *name, const char *source) {
	elf_Parser *parser = calloc(1, sizeof(*parser));
	// todo: arena!
	parser->tree_memory = sys_virtual_alloc(GIGABYTES(1));

	ASSERT(inter != 0);
	ASSERT(parser != 0);
	ASSERT(name != 0);
	ASSERT(source != 0);
	ASSERT(strlen(name) < sizeof(parser->name));
	parser->inter = inter;
	copy_memory(parser->name, name, strlen(name) + 1);
	parser->source = (char *) source;
	parser_restart(parser, (char *) source);
	return parser;
}



static void check_tree(elf_Parser *parser, Source line, treeID id) {
	if (id <= Y_NULL){
		parser_dialog(parser,line,"invalid expression");
	}
}



static bool peek_tok(elf_Parser *parser, int type) {
	return parser->tok.type == type;
}
static bool peek_prox_tok(elf_Parser *parser, int type) {
	return parser->tok_prox.type == type;
}
static bool pick_tok(elf_Parser *parser, int k) {
	return peek_tok(parser,k) && (next_tok(parser), 1);
}
static bool peek_tok_inl(elf_Parser *parser, int k) {
	return peek_tok(parser,k) && parser->tok_prev.eol != 1;
}
static bool pick_tok_inl(elf_Parser *parser, int k) {
	return peek_tok_inl(parser,k) && (next_tok(parser), 1);
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


#define SCOPED_PAIR(X, Y) for(int _i=((X),0); _i++<1; (Y))
#define SCOPE(par) SCOPED_PAIR(begin_scope(par),close_scope(par))
// todo: make it actually just be the block, no scope!
#define BLOCK(par) SCOPED_PAIR(begin_block(par),close_block(par))
#define BLOCK_AND_SCOPE(par) SCOPED_PAIR(begin_block(par),close_block(par))


static void begin_scope(elf_Parser *par) {
	ASSERT(par->scope_index < COUNTOF(par->scope_stack));
	par->scope_stack[par->scope_index ++] = par->entity_index;
	par->scope ++;
}



static void close_scope(elf_Parser *par) {
	ASSERT(par->scope_index > 0);

	int new_index = par->scope_stack[-- par->scope_index];

	for (int i=par->entity_index-1; i>=new_index; --i)
	{
		if (~par->entities[i].status & ENTITY_REFERENCED)
		{
			parser_dialog(par, par->entities[i].line
			, "warning: unreferenced entity");
		}
	}

	ASSERT(new_index <= par->entity_index);

	par->entity_index = new_index;
	par->scope --;
}



static Loop *begin_loop(elf_Parser *parser) {
	ASSERT(parser->loop_index < COUNTOF(parser->loop_stack));
	Loop *loop = & parser->loop_stack[parser->loop_index ++];
	zero_memory(loop, sizeof(* loop));
	return loop;
}



static void close_loop(elf_Parser *parser) {
	ASSERT(parser->loop_index > 0);
	-- parser->loop_index;
}



static Loop *get_loop(elf_Parser *parser, treeID name) {
	// no loop
	if (parser->loop_index < 1) {
		return 0;
	}

	Loop *loop = parser->loop_stack + parser->loop_index - 1;

	// this top one
	if (name == Y_NULL) {
		return loop;
	}

	// find loop
	while (loop > parser->loop_stack) {
		if (loop->name == name) {
			return loop;
		}
	}

	return 0;
}



static Loop *checkloop(elf_Parser *parser, Source line, treeID name) {
	Loop *loop = get_loop(parser, name);
	if (!loop) {
		parser_dialog(parser, line, "not in a loop");
	}
	return loop;
}



// add an instruction to the block, instructions within a block
// are executed in order
static void block_add(elf_Parser *parser, treeID id) {
	if (~parser->block.status & BLOCK_ENDED) {
		darr_add(parser->block.body, id);
	}
	else {
		parser_dialog(parser, id->line
		, "warning: statement will be ignored because the block has already ended.");
	}
}


static void begin_block(elf_Parser *parser) {
	if (parser->block_index >= COUNTOF(parser->block_stack)) {
		parser_dialog(parser, parser->tok.line, "block nesting too deep");
	}

	ASSERT(parser->block_index < COUNTOF(parser->block_stack));

	parser->block_stack[parser->block_index ++] = parser->block;
	parser->block = (Block){};

	begin_scope(parser);
}






// this is only meant to be called when you reach the end
// of a syntactic block, not when you reach a block
// terminating instruction.
static void close_block(elf_Parser *parser) {
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
		darr_add(block.body, block.defers[i]);
	}

	parser->block = parser->block_stack[-- parser->block_index];
}



static treeID get_block_as_tree(elf_Parser *parser) {
	treeID bl = tree_block(parser, parser->tok.line, parser->block.body);
	return bl;
}



static void close_memory_region(elf_Parser *par) {
	treeID bl = get_block_as_tree(par);
	close_block(par);
	block_add(par, bl);
}



// begin a new memory block, closes the memory block and adds to the
// parent block
#define MEMORY_SCOPE(par) SCOPED_PAIR(begin_block(par),close_memory_region(par))



// todo: we could just store the entity id within the
// tree, because now all entities point their own trees...
static int identify_tree(elf_Parser *par, treeID tree) {
	int id;

	for (id=par->entity_index-1; id >= 0; --id)
	{
		Entity en=par->entities[id];

		if (en.tree==tree)
		{
			return id;
		}
	}
	return NO_ENTITY;
}


// todo: can we at-least hash the name?
static int identify_name(elf_Parser *par, char *name) {

	int id;
	for (id=par->entity_index-1; id>=0; --id)
	{
		Entity en=par->entities[id];

		if (text_eq(en.name,name))
		{
			par->entities[id].status |= ENTITY_REFERENCED;
			return id;
		}
	}
	return NO_ENTITY;
}



static entID parser_bind(elf_Parser *parser, Source line, int flags, char *name, treeID tree) {
	{
		entID id = identify_name(parser, name);

		if (id != NO_ENTITY) {

			Entity en = parser->entities[id];

			if (en.kind == ENTITY_DIRECTORY) {
				parser_dialog(parser,line,"'%s': name is reserved for symbol directory",name);
			}

			// declared within the same scope
			if (en.scope == parser->scope) {
				parser_dialog(parser, line, "'%s': is already declared", name);
			}
			// declared outside of this function
			else if (en.scope >= parser->enc->tree_funexpr.scope) {
				parser_dialog(parser, line, "'%s': this declaration shadows another one", name);
			}
		}
	}

	// todo: ensure that all parameters are generated one after the other
	// and we could even increment the arity of this function here whenever
	// we see a param but that's crazy work
	ASSERT(parser->entity_index < MAX_ENTITIES);
	entID id = parser->entity_index ++;
	Entity *entity = & parser->entities[id];
	entity->kind = ENTITY_LOCAL;
	entity->status = flags;
	entity->scope = parser->scope;
	entity->tree = tree;
	entity->name = name;
	entity->line = line;
	return id;
}


static void add_this_param(elf_Parser *parser, Source line) {
	treeID y = tree_nop(parser,line);

	treeID x = tree_memory(parser,line,y);
	block_add(parser,x);

	parser_bind(parser, line
	, ENTITY_PARAMETER|ENTITY_ASSIGNED|ENT_FLAG_CONSTANT|ENTITY_REFERENCED
	, "this", x);
}


// todo: reset the parser just in case
static int parse_file(elf_Parser *parser) {
	treeID func = new_tree(parser, parser->tok.line, TREE_FUNCTION, NT_FUN);
	func->tree_funexpr.arity = 1;
	func->tree_funexpr.variadic = true;

	// always the first function
	darr_add(parser->functions, func);

	parser->enc = func;

	BLOCK(parser) {
		add_this_param(parser, parser->tok.line);
		while (parse_stat(parser));

		func->tree_funexpr.body = get_block_as_tree(parser);

	}
	// todo: return proper error code
	return true;
}



static treeID parse_function(elf_Parser *parser) {
	Token tok = parser->tok;
	if (pick_tok(parser, TK_FUN)) {
	}
	// todo: experimental
	else if (pick_tok(parser, TK_FUNCTION)) {
	}
	else {
		NO_CODE;
	}

	treeID enc = parser->enc;
	treeID function;

	BLOCK_AND_SCOPE(parser) {


		function = new_tree(parser, tok.line, TREE_FUNCTION, NT_FUN);
		function->tree_funexpr.scope = parser->scope;
		function->tree_funexpr.enc   = enc;
		function->tree_funexpr.arity = 1;
		parser->enc = function;



		// add 'this'
		add_this_param(parser, tok.line);

		take_tok(parser,TK_PAREN_LEFT);

		if (!peek_tok(parser,TK_PAREN_RIGHT)) do {

			if (pick_tok(parser, TK_DOT_DOT)) {
				function->tree_funexpr.variadic = true;
				if (!peek_tok(parser, TK_PAREN_RIGHT)) {
					parser_dialog(parser, parser->tok.line, "expected ')' after '...', you may not have more parameters after '...'");
				}
				break;
			}


			Token name = take_tok(parser, TK_WORD);
			treeID param = tree_memory(parser, name.line, tree_nop(parser, name.line));
			block_add(parser,param);

			parser_bind(parser, name.line
			, ENTITY_ASSIGNED|ENTITY_PARAMETER
			, name.text, param);

			function->tree_funexpr.arity ++;
		} while (pick_tok(parser,TK_COMMA));

		if (!peek_tok(parser, TK_PAREN_RIGHT)) {
			parser_dialog(parser,0,"did you miss a ',' ?");
		}

		take_tok(parser,TK_PAREN_RIGHT);

		// todo: remove! or optional?
		pick_tok(parser,TK_QMARK);

		if (peek_tok(parser,TK_CURLY_LEFT)) {
			parse_stat(parser);
			{
				// todo: only if the block doesn't have a return already
				entID thisid = identify_name(parser,"this");
				ASSERT(thisid != -1);

				Entity thisen=parser->entities[thisid];

				treeID v = tree_ret(parser,tok.line,thisen.tree);
				block_add(parser, v);
			}
		}
		// direct return
		else if (pick_tok(parser, TK_HARD_ARROW)) {
			treeID v = parse_expr(parser, 0);
			v = tree_ret(parser, tok.line, v);
			block_add(parser,v);
		}
		else {
			parser_dialog(parser, parser->tok.line, "missing function body, expected '{' or '-->'");
		}

		treeID body = get_block_as_tree(parser);
		function->tree_funexpr.body = body;

		darr_add(parser->functions, function);
	}

	parser->enc = enc;

	return function;
}



//
// 'load' ( <file-name> )
//
//  elf.load_file(<file-name>)
//
static treeID parse_load(elf_Parser *parser){
	Token tok=take_tok(parser,TK_LOAD);

	treeID v,*z;
	z=parse_args(parser);

	v=tree_callcoreapi(parser,tok.line,BUILTIN_LOADFILE,z);
	return v;
}

//
// 'new' <meta-table> ( <argument-list> )
//
// ret elf.set_meta({},Vector2):__new(x,y)
//
static treeID parse_new(elf_Parser *parser){
	Token tok=take_tok(parser,TK_NEW);

	treeID v, t, c;

	c=parse_postfix(parser,0);
	if (c->kind != TREE_CALL) {
		parser_dialog(parser,tok.line,"invalid expression");
	}

	treeID meta,*args;

	args=c->z;
	meta=c->x;


	/* check if we can reuse the table literal the user passed */
	if ((darr_l(args) == 1) && (get_tree_kind(parser,args[0]) == TREE_NEW_TABLE)) {
		t=args[0];
	} else {
		t=tree_table(parser,tok.line,0);
	}

	/* todo: this is sort of inefficient, but if we don't issue
	the call instruction with a meta-field, the generator won't
	insert the 'this' parameter */

	/* todo: have each builtin come with a signature that we can
	actually very at compile time, implement this once we do directories */
	t=tree_callcoreapi2(parser, tok.line, BUILTIN_SETMETA, t, meta);

	v=tree_meta_call(parser, meta->line, t, "__new", args);
	return v;
}



static treeID nametotree(elf_Parser *parser, Source line, char *name, int lval) {

	treeID v = Y_NULL;

	entID id = identify_name(parser, name);

	if (id != NO_ENTITY) {
		Entity entity = parser->entities[id];

		if (!lval) {
			if (~entity.status & ENTITY_ASSIGNED) {
				parser_dialog(parser, parser->tok.line, "warning: usage of possibly unassigned variable");
			}
		}


		// check if we have to capture this thing
		treeID enc = parser->enc;
		if (entity.scope < enc->tree_funexpr.scope) {
			int index = -1;

			// check if we've captured this already
			FOR_ARRAY(i, enc->tree_funexpr.capts) {
				if (enc->tree_funexpr.capts[i] == entity.tree){
					index = i;
					break;
				}
			}

			// add to captures if not
			if (index == -1) {
				index = darr_l(enc->tree_funexpr.capts);
				darr_add(enc->tree_funexpr.capts, entity.tree);
			}

			v = tree_closure_value(parser, line, index);

			// goto next function
			enc = enc->tree_funexpr.enc;
			if (entity.scope < enc->tree_funexpr.scope) {
				parser_dialog(parser, line, "cannot capture?");
			}
		} else {
			v = entity.tree;
		}

	} else {
		// todo: we can only do this after we implement directories
		// because otherwise all of our libs break!
		// parser_dialog(parser, line, "undeclared identifier, if this is a global use the 'global' keyword or declared a global");

		v = tree_global_symbol(parser, line, name);
	}
	return v;
}



static treeID nametorval(elf_Parser *parser, Source line, char *name) {
	return nametotree(parser, line, name, false);
}

static treeID nametolval(elf_Parser *parser, Source line, char *name) {
	return nametotree(parser, line, name, true);
}



static treeID parse_unary(elf_Parser *parser, bool unused) {
	Token tok=parser->tok;

	treeID v=Y_NULL;
	switch (tok.type) {

		case TK_M_INDEX: {
			next_tok(parser);

			// todo: handle operand '#index(<loop-name>)'
			Loop *loop = checkloop(parser, tok.line, Y_NULL);

			v = loop->index;
			ASSERT(v != Y_NULL);
		} break;

		case TK_M_VALUE: {
			next_tok(parser);

			// todo: handle operand '#index(<loop-name>)'
			Loop *loop = checkloop(parser, tok.line, Y_NULL);

			v = loop->value;
			ASSERT(v != Y_NULL);
		} break;
		case TK_M_GETMEM: {
			next_tok(parser);
			v=parse_subexpr(parser,0,10000);
			v=tree_unary(parser,tok.line,TREE_DEBUG_GET_MEMORY,NT_INT,v);
		} break;
		case TK_M_GETEXPR: {
			next_tok(parser);
			v=parse_subexpr(parser,0,10000);
			v=tree_unary(parser,tok.line,TREE_DEBUG_GET_EXPRESSION_NAME,NT_STR,v);
		} break;
		case TK_TILDE: {
			next_tok(parser);
			v=parse_subexpr(parser,0,10000);
			v=tree_unary(parser,tok.line,EXPR_BIT_NOT,NT_ANY,v);
		} break;

		// the global keyword tells the parser that a name is a global
		case TK_GLOBAL: {
			next_tok(parser);
			if (pick_tok(parser, TK_PAREN_LEFT)){
				Token name=take_tok(parser, TK_WORD);
				v=tree_global_symbol(parser, name.line, name.text);
				take_tok(parser, TK_PAREN_LEFT);
			} else {
				Token name=take_tok(parser, TK_WORD);
				v=tree_global_symbol(parser, name.line, name.text);
			}
		} break;

		case TK_SUB: {
			next_tok(parser);
			v=parse_subexpr(parser,0,10000);
			v=tree_binary(parser,tok.line,EXPR_SUB,NT_ANY,tree_int(parser,tok.line,0),v);
		} break;
		case TK_ADD: {
			next_tok(parser);
			v = parse_subexpr(parser,0,10000);
		} break;
		case TK_JSON: {
			//
			//	todo: we need some constant pool, for now we
			// add to globals...
			//

			Token tok = take_tok(parser,TK_JSON);

			int noerr = parse_json_object(parser);
			if (noerr) {

				// todo:
				V json = popvalue(parser->inter);
				int index = darr_grow(parser->inter->globals->array, 1);
				parser->inter->globals->array[index] = json;


				v = tree_global(parser, tok.line, index);
			}
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

		// todo: dot syntax is to be repurposed
		case TK_DOT:
		// todo: make this legitimate, add directories and stuff
		case TK_ELF: {
			char sym[256] = {};

			// elf is a keyword!
			if (pick_tok(parser,TK_ELF)) {
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
			next_tok(parser);
			v = nametorval(parser, tok.line, tok.text);
		} break;
		case TK_PAREN_LEFT: {
			next_tok(parser);
			if (!peek_tok(parser,TK_PAREN_RIGHT)) {
				v=parse_expr(parser,0);
			}
			take_tok(parser,TK_PAREN_RIGHT);
		} break;
		case TK_FUNCTION:
		case TK_FUN: {
			v=parse_function(parser);
		} break;
		case TK_NIL: {
			next_tok(parser);
			v = tree_nil(parser,tok.line);
		} break;
		case TK_TRUE:{
			next_tok(parser);
			v = tree_int(parser,tok.line,1);
		} break;
		case TK_FALSE: {
			next_tok(parser);
			v = tree_int(parser,tok.line,0);
		} break;
		case TK_LETTER: case TK_INTEGER: {
			next_tok(parser);
			v=tree_int(parser,tok.line,tok.integer);
		} break;
		case TK_NUMBER: {
			next_tok(parser);
			v = tree_num(parser,tok.line,tok.number);
		} break;
		case TK_STRING: {
			next_tok(parser);
			v = tree_str(parser,tok.line,tok.text);
		} break;
		case TK_FORMAT_STRING: {
			next_tok(parser);

			// todo: leak!
			// the format string can only get shorter
			char *heapbuf = malloc(strlen(tok.text) + 1);
			char *write = heapbuf;

			v = tree_str(parser,tok.line,heapbuf);

			treeID *args = 0;
			darr_add(args, v);

			char *old_cursor;
			Token old_tok,old_tok_prev,old_tok_prox;
			// save lexing state
			old_cursor = parser->cursor;
			old_tok = parser->tok;
			old_tok_prev = parser->tok_prev;
			old_tok_prox = parser->tok_prox;

			char *format = tok.text;
			while (*format) {
				while (*format && *format != FORMAT_CHAR) {
					*write ++ = *format ++;
				}
				if (*format == FORMAT_CHAR) {
					*write ++ = *format ++;
					if (*format == '{') {
						parser_restart(parser, format);
						if (!pick_tok(parser, TK_CURLY_LEFT)) {
							parser_dialog(parser, 0, "expected '{'");
						}
						v = parse_expr(parser, 0);
						darr_add(args, v);
						if (!pick_tok(parser, TK_CURLY_RIGHT)) {
							parser_dialog(parser, 0, "expected '}'");
						}
						ASSERT(*parser->tok_prev.line == '}');
						// use the previous token + 1 because the parser
						// skips white space after each token
						format = parser->tok_prev.line + 1;
					}
				}
			}
			*write ++ = '\0';

			// restore lexing state
			parser->cursor = old_cursor;
			parser->tok = old_tok;
			parser->tok_prev = old_tok_prev;
			parser->tok_prox = old_tok_prox;

			v = tree_callcoreapi(parser, tok.line, BUILTIN_FORMAT, args);
		} break;
		// it's ok
		default: ;
	}

	esc:
	return v;
}



static treeID parse_table(elf_Parser *parser) {
	take_tok(parser,TK_CURLY_LEFT);

	Token tok = parser->tok;

	treeID *key_value_tuples = 0;

	int index = 0;
	for (;(tok.type != TK_NONE) && (tok.type != TK_CURLY_RIGHT); tok = parser->tok) {
		treeID x = Y_NULL, y = Y_NULL;

		// multi stores
		if ((tok.type == TK_WORD) && (parser->tok_prox.type == TK_ASSIGN)) {
			tok = next_tok(parser);
			x = tree_str(parser,tok.line,tok.text);
			if (x == Y_NULL) goto _err;
		} else {
			x = y = parse_expr(parser,0);
			if (x == Y_NULL) goto _err;
		}

		tok = parser->tok;
		if (pick_tok(parser,TK_ASSIGN)) {

			y = parse_expr(parser, 0);
			if (y == Y_NULL) goto _err;

		} else {

			// todo: unsafe, could overwrite previous stuff
			x = tree_int(parser,tok.line,index++);
			if (x == Y_NULL) goto _err;
		}

		ASSERT(y != Y_NULL);
		ASSERT(x != Y_NULL);

		check_tree(parser,tok.line,x);
		check_tree(parser,tok.line,y);
		tok = parser->tok;

		darr_add(key_value_tuples, tree_tuple2(parser, tok.line, x, y));

		if (pick_tok(parser,TK_COMMA)) {
			continue;
		}
	}
	take_tok(parser,TK_CURLY_RIGHT);


	treeID table = tree_table(parser, tok.line, key_value_tuples);
	return table;
	_err:
	parser_dialog(parser,parser->tok.line,"invalid field intializer");
	return Y_NULL;
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
		darr_add(z,x);
	} else if (pick_tok(parser,TK_PAREN_LEFT)) {
		pick_tok(parser,TK_COMMA);
		if (!peek_tok(parser,TK_PAREN_RIGHT)) do {
			x=parse_expr(parser,0);
			if(x!=Y_NULL){
				// desugar tuple expressions
				if (get_tree_kind(parser,x)==TREE_TUPLE) {
					n=get_tree(parser,x).z;
					FOR_ARRAY(i,n){
						darr_add(z,n[i]);
					}
				} else darr_add(z,x);
			} else break;
		} while (pick_tok(parser,TK_COMMA));
		take_tok(parser,TK_PAREN_RIGHT);
	} else {
		x=parse_expr(parser,0);
		if (x!=Y_NULL)darr_add(z,x);
	}
	return z;
}



// parse a postfix expression
static treeID parse_postfix(elf_Parser *parser, int flags) {

	Token tok = parser->tok;

	treeID v = parse_unary(parser, flags);
	if (v == Y_NULL) goto esc;

	while (parser->tok.type != TK_NONE && !parser->tok_prev.eol) {
		tok = parser->tok;

		switch (tok.type) {
			case TK_DOT: {
				next_tok(parser);
				// <expr>.(x,y) -> (<expr>.x, <expr>.y)
				if (pick_tok(parser,TK_PAREN_LEFT)) {
					Token n;
					treeID x,y,*z=0;
					do {
						n=take_tok(parser,TK_WORD);
						y=tree_str(parser,n.line,n.text);
						x=tree_field(parser,tok.line,v,y);
						darr_add(z,x);
					} while (pick_tok(parser,TK_COMMA));
					v = tree_tuple(parser,tok.line,z);
					take_tok(parser,TK_PAREN_RIGHT);
				}
				// for this return a new table with the fields x and y
				// from 'table'
				// table.{x,y}
				else if (pick_tok(parser,TK_CURLY_LEFT)) {
					NO_CODE;
				} else {
					Token name;
					treeID field;
					name=take_tok(parser,TK_WORD);
					field=tree_str(parser,name.line,name.text);
					v=tree_field(parser,tok.line,v,field);
				}
			} break;
			case TK_SQUARE_SQUARE_LEFT: {
				take_tok(parser,TK_SQUARE_SQUARE_LEFT);
				treeID x = parse_expr(parser, 0);
				v = tree_direct_index(parser,tok.line, v, x);
				take_tok(parser,TK_SQUARE_SQUARE_RIGHT);
			} break;
			/* todo: make this nil safe, so [0,0] shouldn't
			fail if item at 0 is nil, we should have a separate
			instruction for getfield, which is like getfieldoptional
			or something to avoid having to generate additional code */
			case TK_SQUARE_LEFT: {
				take_tok(parser,TK_SQUARE_LEFT);

				do {
					treeID x = parse_expr(parser,0);
					if (x == Y_NULL) break;
					//
					// apply desugaring
					// A [ B . (y, x) ] -> A [ B . y , B . x ]
					//
					if (x->kind == TREE_TUPLE) {
						treeID *z = get_tree(parser,x).z;
						FOR_ARRAY(i,z) {
							v = tree_field(parser,tok.line,v,z[i]);
						}
					}
					else if (x->kind == TREE_RANGE) {
						v = tree_ranged_index(parser,tok.line,v,x);
					}
					else {
						v = tree_field(parser,tok.line,v,x);
					}
				} while(pick_tok(parser,TK_COMMA));

				take_tok(parser,TK_SQUARE_RIGHT);
			} break;
			case TK_COLON: {
				Token n;
				treeID y;
				next_tok(parser);
				n=take_tok(parser,TK_WORD);
				y=tree_str(parser,n.line,n.text);
				v=tree_meta_field(parser,tok.line,v,y);
			} break;
			case TK_CURLY_LEFT:
			case TK_PAREN_LEFT: {
				treeID *z=parse_args(parser);
				v=tree_call(parser,tok.line,v,z);
			} break;
			default: goto esc;
		}
	}

	esc:;
	return v;
}



static treeID parse_subexpr(elf_Parser *parser, int nrets, int rank) {

	Token tok = parser->tok;

	treeID x,y;

	if (peek_tok(parser, TK_DOT_DOT)) {
		x = Y_NULL;
		// empty range
		// ... <y>
		goto parsey;
	}
	else {
		x = parse_postfix(parser, nrets);
		if (x == Y_NULL) goto esc;

		if (x->type == NT_NON) {
			parser_dialog(parser, x->line, "invalid data type");
			goto esc;
		}
	}

	for(;;) {
		parsey:

		tok = parser->tok;

		int prio = tokenrank(tok.type);
		if (prio <= rank) goto esc;

		// assign is not an expression, quit now and let
		// the caller handle it
		if (parser->tok_prox.type == TK_ASSIGN) {
			goto esc;
		}

		next_tok(parser);

		y = parse_subexpr(parser, 1, prio);
		if (tok.type != TK_DOT_DOT) {
			if (!y || y->type == NT_NON) {
				parser_dialog(parser, tok.line, "invalid right operand");
				goto esc;
			}
		}

		x = tree_binary(parser, tok.line, tok2tree(tok.type), NT_ANY, x, y);

		if (y == Y_NULL) goto esc;
	}

	esc:
	return x;
}


static treeID parse_expr(elf_Parser *parser, int nrets) {
	switch (parser->tok.type) {
		case TK_NONE:
		case TK_FOR: case TK_WHILE:
		case TK_COMMA:
		case TK_PAREN_RIGHT: case TK_CURLY_RIGHT: case TK_SQUARE_RIGHT: {
			return Y_NULL;
		}
		default: ;
	}
	return parse_subexpr(parser, nrets, 0);
}


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



static treeID parse_block(elf_Parser *par) {

	treeID v;

	BLOCK_AND_SCOPE(par) {

		if (pick_tok(par,TK_CURLY_LEFT)) {

			while (parse_stat(par));

			take_tok(par,TK_CURLY_RIGHT);
		}
		else {
			parse_stat(par);
		}

		v = get_block_as_tree(par);
	}
	return v;
}



static void checkstoreto(elf_Parser *par, Source line, treeID tree){


	if (tree <= 0 || tree->type == NT_NON) {
		parser_dialog(par, line, "invalid l-value for store");
	}

	entID e = identify_tree(par, tree);

	if (e != NO_ENTITY) {
		if (par->entities[e].status & ENT_FLAG_CONSTANT) {
			parser_dialog(par, line
			,	"reassignment of constant entity");
			parser_dialog(par, par->entities[e].line
			,	"see declaration");
		}
	}
}




// todo: this double evaluates the operands
// of the condition expression!
static void parse_assert(elf_Parser *parser) {
	Token tok = take_tok(parser, TK_M_ASSERT);

	treeID cond = parse_expr(parser, 0);


	treeID *tassertargs = 0;
	darr_add(tassertargs, cond);

	String_Builder sb = {};
	switch (get_tree_kind(parser, cond)) {
		case EXPR_NEQ:   sb_sprintf(&sb, "(%% != %%) is false"); goto _bcase;
		case EXPR_EQ:    sb_sprintf(&sb, "(%% == %%) is false"); goto _bcase;
		case EXPR_OR:    sb_sprintf(&sb, "(%% || %%) is false"); goto _bcase;
		case EXPR_LT:    sb_sprintf(&sb, "(%% < %%) is false"); goto _bcase;
		case EXPR_GT:    sb_sprintf(&sb, "(%% > %%) is false"); goto _bcase;
		case EXPR_GTEQ:  sb_sprintf(&sb, "(%% >= %%) is false"); goto _bcase;
		case EXPR_LTEQ:  sb_sprintf(&sb, "(%% <= %%) is false"); goto _bcase;

		default: {

			parser_dialog(parser, tok.line, "expression not supported for #assert");
			goto esc;

			_bcase:
			// todo: leak! sb cannot be freed because trees still use
			// char *
			treeID tformatstr = tree_str(parser, tok.line, sb.buf);

			treeID tcallformat = tree_callcoreapi3(parser, tok.line
			, BUILTIN_FORMAT, tformatstr, cond->x, cond->y);

			darr_add(tassertargs, tcallformat);
		} break;
	}


	treeID v = tree_callcoreapi(parser, tok.line, BUILTIN_ASSERT, tassertargs);
	block_add(parser, v);

	esc:
	return;
}



static treeID *parse_rhs(elf_Parser *parser, int nrets, treeKi empty) {
	treeID *ys = 0;

	do {
		if (pick_tok(parser, TK_DOT_DOT)) {

			// helper message
			if (darr_l(ys) >= nrets) {
				parser_dialog(parser, parser->tok.line
				, "redundant ellipsis, enough r-values already");
			}

			// '...' with no previous y's just means fill everything with nil,
			// which is what happens by default
			if (darr_l(ys)) {
				treeID ylast = ys[darr_l(ys)-1];
				for (int i=darr_l(ys); i<nrets; ++i) {
					darr_add(ys, ylast);
				}
			}


			// helper message
			if (peek_tok(parser, TK_COMMA)) {
				parser_dialog(parser, parser->tok.line
				, "'...': terminates the expression list");
			}
			break;
		}

		treeID v=parse_expr(parser, 0);
		if (v==TREE_NONE) break;

		darr_add(ys, v);

		if (darr_l(ys)>nrets) {
			parser_dialog(parser, parser->tok.line
			, "warning: excess right hand side value");
		}
	} while (pick_tok(parser, TK_COMMA));

	// fill with nil
	for (int i=darr_l(ys); i<nrets; ++i) {
		darr_add(ys, tree_nullary(parser, parser->tok.line, empty, NT_ANY));
	}
	return ys;
}


static void rewrite_lhs(elf_Parser *parser, treeID lhs) {
	if (lhs->kind == EXPR_FIELD || lhs->kind == EXPR_DIRECT_INDEX) {

		// already memory
		if (lhs->x->kind != TREE_MEMORY)
		{
			treeID v = tree_memory(parser, lhs->x->line, lhs->x);
			block_add(parser, v);

			lhs->x = v;
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
		default: ;
	}

	switch (tok.type) {
		case TK_M_ASSERT: {
			parse_assert(parser);
		} break;

		case TK_DEFER: {
			next_tok(parser);

			treeID v = parse_block(parser);
			darr_add(parser->block.defers, v);
		} break;

		case TK_RET:
		case TK_HARD_ARROW:

		case TK_BREAK:
		case TK_CONTINUE: {
			next_tok(parser);


			treeID v = Y_NULL;
			if(!tok.eol) {
				v = parse_expr(parser,0);
			}

			if((tok.type == TK_CONTINUE) || (tok.type == TK_BREAK))
			{
				Loop *loop = checkloop(parser, tok.line, v);
				if (tok.type == TK_CONTINUE) {
					v = tree_goto(parser, tok.line);
					darr_add(loop->continues, v);
				}
				else {
					// v = tree_exit_loop(parser, tok.line);
					v = tree_goto(parser, tok.line);
					darr_add(loop->breaks, v);
				}
			}
			else {
				v = tree_ret(parser,tok.line, v);
			}


			block_add(parser, v);

			parser->block.status = BLOCK_ENDED | BLOCK_HASRET;
		} break;

		case TK_CURLY_LEFT:
		{
			treeID v = parse_block(parser);
			block_add(parser,v);
		} break;

		case TK_WHILE: {
			next_tok(parser);
			treeID pred = parse_expr(parser,0);
			take_tok(parser,TK_QMARK);



			begin_loop(parser);
			treeID body = parse_block(parser);
			close_loop(parser);

			// get the loop we just popped
			Loop loop = parser->loop_stack[parser->loop_index];

			treeID v = new_tree(parser,tok.line,TREE_WHILE_LOOP,NT_NON);
			v->loop.pred = pred;
			v->loop.body = body;
			v->loop.c = loop.continues;
			v->loop.b = loop.breaks;
			block_add(parser, v);
		} break;

		case TK_FOR: {
			parse_for(parser);
		} break;

		case TK_IF: {
			next_tok(parser);
			treeID v = parse_if(parser);
			block_add(parser, v);
		} break;

		default: {
			Token *token_buf = 0;
			treeID *xs = 0, *ys = 0;

			// consume:
			// (WORD ,) ':='
			// (WORD ,) '::='
			while (peek_tok(parser, TK_WORD) && peek_prox_tok(parser, TK_COMMA)) {
				darr_add(token_buf, next_tok(parser));
				take_tok(parser, TK_COMMA);
			}

			// this is a declaration
			if (peek_tok(parser, TK_WORD) && (peek_prox_tok(parser, TK_BIND) || peek_prox_tok(parser, TK_HARD_BIND))) {
				darr_add(token_buf, next_tok(parser));


				int status = ENTITY_ASSIGNED;

				if (pick_tok(parser, TK_HARD_BIND)) {
					status |= ENT_FLAG_CONSTANT;
				} else {
					take_tok(parser, TK_BIND);
				}

				ys = parse_rhs(parser, darr_l(token_buf), TREE_NOP);
				ASSERT(darr_l(ys) >= darr_l(token_buf));


				for (int i=0; i<darr_l(token_buf); i++)
				{
					char *name = token_buf[i].text;

					treeID v=tree_memory(parser, token_buf[i].line, ys[i]);
					block_add(parser, v);

					parser_bind(parser, token_buf[i].line, status, name, v);
				}

			} else {

				// convert all names to storage targets
				for (int i=0; i<darr_l(token_buf); i++) {
					char *name = token_buf[i].text;

					treeID v=nametolval(parser, tok.line, name);
					if (v==Y_ERROR) goto _err;

					darr_add(xs, v);
				}

				// parse additional expressions
				do {
					treeID v=parse_expr(parser, 0);
					if (v==Y_ERROR) goto _err;

					darr_add(xs, v);
				} while (pick_tok(parser, TK_COMMA));



				tok=parser->tok;


				if (pick_tok(parser,TK_ASSIGN)){
				} else if (pick_tok(parser,TK_NIL_ASSIGN)){
				} else if (tokenrank(tok.type) > 0){
					next_tok(parser);
					take_tok(parser,TK_ASSIGN);
				} else {
					goto _regularexpr;
				}

				for (int i=0; i<darr_l(xs); ++i) {
					checkstoreto(parser, tok.line, xs[i]);
				}


				ys = parse_rhs(parser, darr_l(xs), EXPR_NIL);
				ASSERT(darr_l(ys) >= darr_l(xs));


				// regular store
				if (tok.type==TK_ASSIGN)
				{
					for (int i=0; i<darr_l(xs); ++i)
					{
						MEMORY_SCOPE(parser) {
							treeID v = tree_store(parser, tok.line, xs[i], ys[i]);
							block_add(parser, v);
						}
					}
				}
				else if (tok.type==TK_NIL_ASSIGN)
				{
					for (int i=0; i<darr_l(xs); ++i)
					{
						MEMORY_SCOPE(parser) {
							rewrite_lhs(parser, xs[i]);

							treeID c = tree_binary(parser, tok.line, EXPR_EQ, NT_ANY, xs[i], tree_nil(parser, tok.line));
							treeID s = tree_store(parser, tok.line, xs[i], ys[i]);
							treeID v = tree_if(parser, tok.line, c, s, 0);
							block_add(parser, v);
						}
					}
				}
				else if (tokenrank(tok.type) > 0)
				{
					//
					// if we got here it means that we attempted to
					// parse an expression that looked like a binary
					// operator, e.g '+''=', but it turned out to be
					// an assignment, so the parser returned the left
					// hand side expression but didn't take the token.
					//
					for (int i=0; i<darr_l(xs); ++i)
					{
						MEMORY_SCOPE(parser) {
							rewrite_lhs(parser, xs[i]);

							treeID o = tree_binary(parser, tok.line, tok2tree(tok.type), NT_ANY, xs[i], ys[i]);
							treeID v = tree_store(parser, tok.line, xs[i], o);
							block_add(parser, v);
						}
					}
				}
				else {
					_regularexpr:
					// if they're regular expressions, just add them to the
					// block and the generator will figure out whether
					// to keep them or not
					FOR_ARRAY(i, xs) {
						block_add(parser, xs[i]);
					}
				}
			// not a decl branch
			}

			darr_free(token_buf);
			darr_free(ys);
			darr_free(xs);

		// end default case
		} break;
	}

	esc:
	return success;

	_err:
	return 0;
}



static treeID parse_if(elf_Parser *parser){
	Token tok = parser->tok;

	treeID v;

	// todo: why is this a block
	BLOCK_AND_SCOPE(parser)
	{
		treeID pred = parse_expr(parser,0);

		take_tok(parser, TK_QMARK);

		treeID true_clause = parse_block(parser);
		treeID else_clause = Y_NULL;

		// todo: elif != else if
		if (pick_tok(parser,TK_ELIF)) {
			else_clause = parse_if(parser);
		}
		else if (pick_tok(parser,TK_ELSE)) {
			else_clause = parse_block(parser);
		}

		v = tree_if(parser, tok.line, pred, true_clause, else_clause);
		block_add(parser, v);

		v = get_block_as_tree(parser);
	}
	return v;
}


static bool parse_for(elf_Parser *parser) {
	bool noerror = true;

	Token tok = take_tok(parser,TK_FOR);

	Token name = take_tok(parser,TK_WORD);
	take_tok(parser,TK_ASSIGN);

	//
	// parse all the steps, for i = X, Y, Z, ... ?
	//
	treeID v, *steps = 0;
	do {
		v = parse_expr(parser,0);
		if (v == Y_NULL) {
			noerror = false;
			goto esc;
		}
		darr_add(steps, v);
	} while (pick_tok(parser,TK_COMMA));

	take_tok(parser, TK_QMARK);

	// begin a new block, this also creates a new scope
	// todo: couldn't we just put value and index inside
	// of a range step? declare the entity per step?
	begin_block(parser);


	// create memory locations for index and value variables
	treeID index_, value_;

	index_ = tree_memory(parser,tok.line,tree_nop(parser,name.line));
	block_add(parser,index_);

	value_ = tree_memory(parser,tok.line,tree_nop(parser,name.line));
	block_add(parser,value_);



	// bind the name to the value variable, for <i> = ... ?
	parser_bind(parser, name.line
	, ENTITY_REFERENCED|ENTITY_ASSIGNED|ENTITY_FORLOOP
	, name.text, value_);


	// begin a new loop
	Loop *loop = begin_loop(parser);

	loop->index = index_;
	loop->value = value_;

	// parse the body
	treeID body = parse_block(parser);
	ASSERT(body->kind == TREE_MEMORY_BLOCK);

	close_loop(parser);


	// generate loops or stores for each step
	FOR_ARRAY(i, steps) {
		treeID step = steps[i];

		// create a new block for each step
		begin_block(parser);


		// loops have two registers, one for the index, one for the value
		treeID index = index_;
		treeID value = value_;

		// check if the step translates into a loop
		if (step->kind == TREE_RANGE || step->kind == EXPR_RANGE_INDEX) {

			treeID rangelo = Y_NULL, rangehi = Y_NULL;
			treeID prebody = Y_NULL;

			if (step->kind == EXPR_RANGE_INDEX) {
				ASSERT(step->x != Y_NULL);
				ASSERT(step->y != Y_NULL);

				treeID array = step->x;
				treeID range = step->y;
				ASSERT(range->kind == TREE_RANGE);

				rangelo = range->x;
				rangehi = range->y;

				// index array and store in value register
				// todo: does the user really expect us to do this on
				// every iteration for a range?
				treeID *args = 0;
				darr_add(args, index);
				v = tree_meta_call(parser, tok.line, array, "idx", args);
				prebody = tree_store(parser, tok.line, value, v);

				rangelo = range->x, rangehi = range->y;

				if (rangelo == Y_NULL) rangelo = tree_int(parser,tok.line,0);
				if (rangehi == Y_NULL) rangehi = tree_meta_call(parser,tok.line,array,"length",0);
			} else {

				// value same as index
				index = value;

				rangelo = step->x, rangehi = step->y;

				// todo: true infinite
				if(rangehi == Y_NULL) rangehi = tree_int(parser,tok.line,UINT_MAX);
				if(rangelo == Y_NULL) rangelo = tree_int(parser,tok.line,0);
			}

			ASSERT(rangelo != Y_NULL);
			ASSERT(rangehi != Y_NULL);


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
			v->loop.c = loop->continues;
			v->loop.b = loop->breaks;
			block_add(parser,v);
		}
		// otherwise this translates to a single store
		else {

			index = index_;
			value = value_;

			// a single value is a range of length 1 with, so index is 0
			// todo: we don't have to emit the store always, only if there
			// was a range before us that could have incremented index
			v = tree_store(parser, tok.line, index, tree_int(parser, tok.line, 0));
			block_add(parser, v);

			v = tree_store(parser, tok.line, value, step);
			block_add(parser, v);
			block_add(parser, body);

			// todo: actually define this semantically...
			if (loop->continues || loop->breaks) {

				FOR_ARRAY(i, loop->continues){
					parser_dialog(parser
					,  loop->continues[i]->line,"breaks and continues are only for ranges");
				}

				FOR_ARRAY(i, loop->breaks){
					parser_dialog(parser
					, loop->breaks[i]->line,"breaks and continues are only for ranges");
				}

				elf_error(parser->inter, 0, "invalid syntax");
			}
		}

		// end block for this step and add to parent block
		v = get_block_as_tree(parser);
		close_block(parser);

		block_add(parser,v);
	}

	// close loop block, note that this is closed after
	// the body is generated
	v = get_block_as_tree(parser);
	close_block(parser);
	block_add(parser, v);

	esc:
	return noerror;
}

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
			next_tok(parser);
			if (peek_tok(parser, TK_NUMBER)) goto numcase;
			else if (peek_tok(parser, TK_INTEGER)) goto intcase;
			parser_dialog(parser, parser->tok.line, "expected integer or number after '-'");
			goto errorcase;
		} break;
		case TK_TRUE: {
			next_tok(parser);
			pushint(parser->R, true);
			ret = 1;
		} break;
		case TK_FALSE: {
			next_tok(parser);
			pushint(parser->R, false);
			ret = 1;
		} break;
		case TK_NUMBER: { numcase:
			next_tok(parser);
			pushnum(parser->R, tok.number * sign);
			ret = 1;
		} break;
		case TK_INTEGER: { intcase:
			next_tok(parser);
			pushint(parser->R, tok.integer * sign);
			ret = 1;
		} break;
		case TK_NIL: {
			next_tok(parser);
			pushnil(parser->R);
			ret = 1;
		} break;
		case TK_STRING: {
			next_tok(parser);
			pushtext(parser->R, tok.text);
			ret = 1;
		} break;
		case TK_CURLY_LEFT: {
			next_tok(parser);

			Tab tab = pushtable(parser->R);
			int tablestk = stack2index(parser->R) - 1;

			while(!peek_tok(parser,TK_NONE) && !peek_tok(parser,TK_CURLY_RIGHT)) {

				// push key (or value)
				// interpret words as raw key names not as entities
				if (pick_tok(parser, TK_WORD)) {

					pushtext(parser->R, parser->tok.text);

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

					// todo: why are we using public stuff!
					elf_setfield(parser->R);
				} else {

					checkstk(parser, tablestk + 1 + 1);

					// keyless entry

					// todo: why are we using public stuff!
					elf_arrayadd(parser->R);
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
			pushnil(parser->R);
			parser_dialog(parser,tok.line,"not a constant expression");
		} break;
	}

	esc:
	return ret;
}



static int parse_json_array(elf_Parser *parser){
	int noerr = true;
	pushtable(parser->inter);

	take_tok(parser, TK_SQUARE_LEFT);

	if (!pick_tok(parser,TK_SQUARE_RIGHT)) do {

		// todo: attempt to recover and ensure the
		// stack is proper still
		noerr = parse_json_value(parser);
		if (!noerr) goto esc;

		elf_arrayadd(parser->inter);

	} while (pick_tok(parser, TK_COMMA));

	take_tok(parser, TK_SQUARE_RIGHT);

	esc:
	return noerr;
}



static int parse_json_object(elf_Parser *parser) {
	int noerr = true;
	pushtable(parser->inter);

	take_tok(parser,TK_CURLY_LEFT);
	if (!peek_tok(parser,TK_CURLY_RIGHT)) do {

		Token tok = take_tok(parser, TK_STRING);
		pushtext(parser->inter, tok.text);

		take_tok(parser, TK_COLON);

		noerr = parse_json_value(parser);
		if (noerr != true) goto esc;

		elf_setfield(parser->inter);
	} while (pick_tok(parser,TK_COMMA));


	take_tok(parser,TK_CURLY_RIGHT);
	esc:
	return noerr;
}



static int parse_json_value(elf_Parser *parser) {
	int noerr = true;

	Token tok = parser->tok;
	switch (tok.type) {
		case TK_STRING: {
			next_tok(parser);
			pushtext(parser->inter, tok.text);
		} break;
		case TK_INTEGER: {
			next_tok(parser);
			pushint(parser->inter, tok.integer);
		} break;
		case TK_NUMBER: {
			next_tok(parser);
			pushnum(parser->inter, tok.number);
		} break;
		case TK_CURLY_LEFT: {
			parse_json_object(parser);
		} break;
		case TK_SQUARE_LEFT: {
			parse_json_array(parser);
		} break;
		default: {
			pushnil(parser->inter);
			parser_dialog(parser, tok.line, "invalid json value");
			noerr = false;
		} break;
	}
	return noerr;
}


// todo: how come we have json but no do whiles...
#if 0
case TK_DO: {
	next_tok(parser);
	parser_begin_block(parser,BLOCK_LOOP);
	begin_do_while_loop(parser,tk.line);
	parse_stat(parser);
	take_tok(parser,TK_WHILE);
	x=parse_expr(parser,0,0);
	close_do_while_loop(parser,tk.line,x);
	parser_close_block(parser);
} break;
#endif


