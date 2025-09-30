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


static treeID parse_unary(Parser *parser, bool flags);
static treeID parse_expr(Parser *parser, int flags);
static treeID parse_subexpr(Parser *parser, int flags, int rank);
static treeID parse_postfix(Parser *parser, int flags);



static treeID parse_table(Parser *parser);
static treeID parse_block(Parser *parser);
static int tok2tree(int tok);
static int parse_stat(Parser *parser);



// the result is pushed onto the stack
static int parse_json_object(Parser *parser);
static int parse_json_value(Parser *parser);
static int parse_constexpr(Parser *parser);


static int tokenrank(int type) {
	return g_token_metadata_table[type].rank;
}



static void add_this_param(Parser *parser, Source line);
static treeID parse_if(Parser *parser);
static bool parse_for(Parser *parser);
static treeID *parse_args(Parser *parser);
static void next_instr(Parser *parser, treeID id);


//
//
//	Message message_stack[...]
//
//	push_note()
//
//	push_warning()
//	push_error()
//	push_fatal()
//	push_suggestion()
//	push_deprecation_warning()
//
//
enum {
	ERROR_NONE = 0,
	ERROR_UNEXPECTED_TOKEN,
	ERROR_INVALID_EXPRESSION,
	ERROR_EXPECTED_TOKEN,
	ERROR_NOT_IN_A_LOOP,
	ERROR_UNREACHABLE_STAT,
	ERROR_INVALID_CONTRACT_NAME,
	ERROR_INVALID_DECL,
	ERROR_SYNTAX_DEPRECATION,


	EC_INTERNAL_CANNOT_CAPTURE_NO_MEMORY,
};



#define tok2s(type) (g_token_metadata_table[type].name)




static void push_note(Parser *parser, Source line, int error, const char *message) {
}




static void push_error(Parser *parser, Source line, int error, const char *message) {
	parser_dialog(parser, line, message);
	reporterror(parser->inter, -1, message);
}




static void push_warning(Parser *parser, Source line, int error, const char *message) {
	parser_dialog(parser, line, message);
}





static void push_error_unexpected_token(Parser *parser, Token tok) {
	push_error(parser, tok.line, ERROR_UNEXPECTED_TOKEN, "unexpected token");
}






static void push_error_expected_token(Parser *parser, Token tok, int expected) {

	const char *message = tpf("got token: '%s', expected: '%s'", tok2s(tok.type), tok2s(expected));

	push_error(parser, tok.line, ERROR_EXPECTED_TOKEN, message);
}






static void push_error_expected_token_pair(Parser *parser, Token tok, int expected, Token pair) {


	const char *message = tpf("got token: '%s', expected: '%s'", tok2s(tok.type), tok2s(expected));

	// todo: push note
	parser_dialog(parser, pair.line, "note: pair started here");

	push_error(parser, tok.line, ERROR_EXPECTED_TOKEN, message);

}






static void push_error_not_in_a_loop(Parser *parser, Source line) {

	push_error(parser, line, ERROR_NOT_IN_A_LOOP, "not in a loop");
}





static void push_error_block_ended_expr_ignored(Parser *parser, treeID v) {
	// todo: generate hint using the current block!
	push_warning(parser, v->line, ERROR_UNREACHABLE_STAT
	, "statement ignored because the block ended");
}





static void check_tree(Parser *parser, Source line, treeID v) {
	if (v <= Y_NULL){
		push_error(parser, v->line, ERROR_INVALID_EXPRESSION, "invalid expression");
	}
}








static void parser_restart(Parser *parser, char *cursor) {
	parser->cursor = cursor;
	// so that I don't forget to do this!
	next_tok(parser);
	next_tok(parser);
}






static void elf_end_parser(Parser *parser) {
	sys_virtual_free(parser->tree_memory);
	free(parser);
}








static Parser *elf_new_parser(elf_State *inter, const char *name, const char *source) {
	ASSERT(inter != 0);
	ASSERT(name != 0);
	ASSERT(source != 0);



	Parser *parser = calloc(1, sizeof(*parser));
	ASSERT(parser != 0);



	// todo: arena!
	parser->tree_memory = sys_virtual_alloc(GIGABYTES(1));
	ASSERT(parser->tree_memory != 0);



	parser->inter = inter;


	ASSERT(strlen(name) < sizeof(parser->name));
	copy_memory(parser->name, name, strlen(name) + 1);



	parser->source = (char *) source;
	parser_restart(parser, (char *) source);



	return parser;
}







static inline bool peek_tok(Parser *parser, int type) {
	return parser->tok.type == type;
}





static inline bool peek_prox_tok(Parser *parser, int type) {
	return parser->tok_prox.type == type;
}





static inline bool pick_tok(Parser *parser, int k) {
	return peek_tok(parser,k) && (next_tok(parser), 1);
}






// todo: to be deprecated! explicit new line token!
static bool peek_tok_inl(Parser *parser, int k) {
	return peek_tok(parser,k) && parser->tok_prev.eol != 1;
}





// todo: to be deprecated! explicit new line token!
static bool pick_tok_inl(Parser *parser, int k) {
	return peek_tok_inl(parser,k) && (next_tok(parser), 1);
}





// todo: to be deprecated! explicit new line token!
static Token get_token_inline(Parser *parser, int type) {
	Token tok = parser->tok;

	if (!pick_tok_inl(parser, type)) {
		push_error_expected_token(parser, parser->tok, type);
	}

	return tok;
}





static Token take_tok(Parser *parser, int type) {
	Token tok = parser->tok;

	if (!pick_tok(parser, type)) {
		push_error_expected_token(parser, parser->tok, type);
	}

	return tok;
}




static Token take_tok_pair(Parser *parser, int type, Token pair) {
	Token tok = parser->tok;

	if (!pick_tok(parser, type)) {
		push_error_expected_token_pair(parser, parser->tok, type, pair);
	}

	return tok;
}






#define SCOPED_PAIR(X, Y) for(int _i=((X),0); _i++<1; (Y))


static void begin_scope(Parser *par) {
	ASSERT(par->scope_index < COUNTOF(par->scope_stack));
	par->scope_stack[par->scope_index ++] = par->entity_index;
	par->scope ++;
}



static void close_scope(Parser *par) {
	ASSERT(par->scope_index > 0);

	int new_index = par->scope_stack[-- par->scope_index];

	for (int i=par->entity_index-1; i>=new_index; --i)
	{
		if (~par->entities[i].status & ENTITY_REFERENCED)
		{
			// todo: in release mode, ignore these entities, in test mode keep them
			parser_dialog(par, par->entities[i].line
			, "warning: unreferenced entity");
		}
	}

	ASSERT(new_index <= par->entity_index);

	par->entity_index = new_index;
	par->scope --;
}



static Loop *begin_loop(Parser *parser) {
	ASSERT(parser->loop_index < COUNTOF(parser->loop_stack));
	Loop *loop = & parser->loop_stack[parser->loop_index ++];
	zero_memory(loop, sizeof(* loop));
	return loop;
}



static void close_loop(Parser *parser) {
	ASSERT(parser->loop_index > 0);
	-- parser->loop_index;
}



static Loop *get_loop(Parser *parser, treeID name) {
	// no loop
	// todo: rename to loop counter instead!
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





static Loop *checkloop(Parser *parser, Source line, treeID name) {
	Loop *loop = get_loop(parser, name);
	if (!loop) {
		push_error_not_in_a_loop(parser, line);
	}
	return loop;
}



// add an instruction to the block, instructions within a block
// are executed in order
static void next_instr(Parser *parser, treeID v) {
	// v->debug ++;
	// if (v->debug > 1) {
	// 	parser_dialog(parser, v->line, "tree '%p' added multiple times", v);
	// 	elf_ldebug("tree '%p' added multiple times", v);
	// }

	if (~parser->block.status & BLOCK_ENDED) {
		heap_array_add(parser->block.body, v);
	}
	else {
		push_error_block_ended_expr_ignored(parser, v);
	}
}





static void begin_block(Parser *par) {
	if (par->block_index >= COUNTOF(par->block_stack)) {
		parser_dialog(par, par->tok.line, "block nesting too deep");
		reporterror(par->R, -1, "block nesting too deep");
	}

	ASSERT(par->block_index < COUNTOF(par->block_stack));

	par->block_stack[par->block_index ++] = par->block;
	par->block = (Block){};

	begin_scope(par);
}






// this is only meant to be called when you reach the end
// of a syntactic block, not when you reach a block
// terminating instruction.
static treeID close_block(Parser *parser) {
	ASSERT(parser->block_index > 0);
	close_scope(parser);

	Block *block = & parser->block;

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
	FOR_ARRAY(i, block->defers) {
		heap_array_add(block->body, block->defers[i]);
	}

	treeID bl = tree_block(parser, parser->tok.line, block->body);

	parser->block = parser->block_stack[-- parser->block_index];

	return bl;
}




static treeID close_memory_region(Parser *par) {
	treeID bl = close_block(par);
	next_instr(par, bl);
	return bl;
}




// begin a new memory block, closes the memory block and adds to the
// parent block
#define MEMORY_SCOPE(par) SCOPED_PAIR(begin_block(par),close_memory_region(par))



// todo: we could just store the entity id within the
// tree, because now all entities point their own trees...
static int identify_tree(Parser *par, treeID tree) {
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
static int identify_name(Parser *par, char *name) {

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



static entID parser_bind(Parser *parser, Source line, int flags, char *name, treeID tree) {
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





static void add_this_param(Parser *parser, Source line) {
	treeID y = tree_nop(parser, line);

	treeID x = tree_memory(parser, line, y);
	next_instr(parser, x);

	parser_bind(parser, line
	, ENTITY_PARAMETER|ENTITY_ASSIGNED|ENT_FLAG_CONSTANT|ENTITY_REFERENCED
	, "this", x);
}








// todo: reset the parser just in case
static int parse_file(Parser *parser) {
	treeID func = new_tree(parser, parser->tok.line, TREE_FUNCTION, NT_FUN);
	func->tree_funexpr.arity = 1;
	func->tree_funexpr.variadic = true;

	// always the first function
	heap_array_add(parser->functions, func);

	parser->enc = func;

	begin_block(parser);
	{
		add_this_param(parser, parser->tok.line);
		while (parse_stat(parser));
	}

	if (parser->tok.type != TK_NONE) {
		push_error_unexpected_token(parser, parser->tok);
	}


	func->tree_funexpr.body = close_block(parser);

	// todo: return proper error code
	return true;
}









static treeID *parse_expr_list(Parser *parser, int flags) {
	treeID *l = 0;

	do {

		treeID v = parse_expr(parser, flags);
		if (notree(v)) goto esc;

		heap_array_add(l, v);

	} while (pick_tok(parser, TK_COMMA));

	esc:
	return l;
}







//
// todo:
//	Aside from arguments, other contracts are enforced on store,
// i: int = 1
//
// So there could be dedicated store instructions that enforce
// that contract or extend the current one!
//
//


// todo: we can't switch blocks to generate type rule validation
typedef struct {
	treeID  memory;
	Token   rule;
} Param;



typedef struct {
	const char *name;
	TypeRule rule;
} NameRule;



// todo: string map!
static const NameRule namerules[] = {
	{"int"         , TRULE_INTEGER  },
	{"num"         , TRULE_NUMBER   },
	{"numeric"     , TRULE_NUMERIC  },
	{"callable"    , TRULE_CALLABLE },
	{"str"         , TRULE_STRING   },
	{"buf"         , TRULE_BUFFER   },
	{"non_nil"     , TRULE_NONNIL   },
};







static void gen_typerule(Parser *parser, treeID memory, Token tok) {

	for (int i=0; i<COUNTOF(namerules); ++i) {
		if (text_eq(namerules[i].name, tok.text)) {
			treeID v = tree_enforce(parser, tok.line, memory, namerules[i].rule);
			next_instr(parser, v);
			break;
		}
	}

#if 0
		case CONTRACT_NON_NIL: {

			treeID *args = 0;
			treeID v;

			v = tree_binary(parser, line, EXPR_NEQ, NT_BOL, memory, tree_nil(parser, line));
			heap_array_add(args, v);

			v = tree_str(parser, line, "non_nil contract violation");
			heap_array_add(args, v);

			v = tree_callcoreapi(parser, line, BUILTIN_ASSERT, args);
			next_instr(parser, v);

		} break;
#endif
}










static int parse_function_params(Parser *parser, int *variadic) {
	*variadic = false;


	int arity = 1;

	Token tok = take_tok(parser, TK_PAREN_LEFT);


	// todo: memory!
	Param *params = 0;

	// add implicit 'this'
	add_this_param(parser, tok.line);

	if (!peek_tok(parser,TK_PAREN_RIGHT)) do {

		if (pick_tok(parser, TK_DOT_DOT)) {
			*variadic = true;

			if (!peek_tok(parser, TK_PAREN_RIGHT)) {
				parser_dialog(parser, parser->tok.line, "expected ')' after '...', you may not have more parameters after '...'");
			}
			break;
		}


		Token name = take_tok(parser, TK_WORD);

		treeID param = tree_memory(parser, name.line, tree_nop(parser, name.line));
		next_instr(parser, param);

		Param p = { param };
		if (pick_tok(parser, TK_COLON)) {
			p.rule = take_tok(parser, TK_WORD);
		}
		heap_array_add(params, p);

		parser_bind(parser, name.line
		, ENTITY_ASSIGNED | ENTITY_PARAMETER
		, name.text, param);

		arity += 1;

	} while (pick_tok(parser,TK_COMMA));

	//
	// code for all contracts must be generated after
	// memory for all parameters has been allocated ...
	//
	for (int i=1; i<arity; ++i) {
		Param p = params[i-1];
		gen_typerule(parser, p.memory, p.rule);
	}

	free_heap_array(params);

	take_tok(parser,TK_PAREN_RIGHT);
	return arity;
}












static treeID parse_function(Parser *parser) {
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


	begin_block(parser);
	{
		// every function receives an inner scope
		function = new_tree(parser, tok.line, TREE_FUNCTION, NT_FUN);
		function->tree_funexpr.scope = parser->scope;
		function->tree_funexpr.enc   = enc;
		parser->enc = function;


		int variadic;
		function->tree_funexpr.arity = parse_function_params(parser, &variadic);
		function->tree_funexpr.variadic = variadic;

		// todo: remove! or optional?
		pick_tok(parser,TK_QMARK);

		if (peek_tok(parser,TK_CURLY_LEFT)) {
			parse_stat(parser);
			{
				// todo: only if the block doesn't have a return already
				// todo:
				//
				// So call instructions are limited, we have a limited
				// number of things we can specify, because of this, we
				// have to load the function along with the arguments in
				// the same memory, block, and the return is placed on
				// the closure, so the default return cannot be 'this',
				// it's the closure itself, for something like __new,
				// we want the result to be this.
				//
				//	To fix this, do a better 'new', so that it doesn't
				// rely on this
				//
				//
				//
				entID thisid = identify_name(parser,"this");
				ASSERT(thisid != -1);

				Entity thisen=parser->entities[thisid];

				treeID v = tree_ret(parser,tok.line,thisen.tree);
				next_instr(parser, v);
			}
		}
		// direct return
		else if (pick_tok(parser, TK_HARD_ARROW)) {
			treeID *l = parse_expr_list(parser, 0);
			treeID v = tree_ret2(parser, tok.line, l);
			next_instr(parser,v);
		}
		else {
			parser_dialog(parser, parser->tok.line, "missing function body, expected '{' or '-->'");
		}

		treeID body = close_block(parser);
		function->tree_funexpr.body = body;

		parser->enc = enc;
	}


	heap_array_add(parser->functions, function);

	return function;
}




// todo: deprecate!
//
// 'load' ( <file-name> )
//
//  elf.load_file(<file-name>)
//
static treeID parse_load(Parser *parser) {
	Token tok = take_tok(parser,TK_LOAD);

	treeID *z = parse_args(parser);

	treeID v = tree_callcoreapi(parser, tok.line, BUILTIN_LOADFILE, z);
	return v;
}





//
// 'new' <meta-table> ( <argument-list> )
//
// ret elf.set_meta({},Vector2):__new(x,y)
//
static treeID parse_new(Parser *parser){
	Token tok=take_tok(parser,TK_NEW);

	treeID v, t;

	treeID c=parse_postfix(parser, 0);
	if (c->kind != TREE_CALL) {
		parser_dialog(parser,tok.line,"invalid expression");
	}

	treeID meta,*args;

	args=c->z;
	meta=c->x;


	/* check if we can reuse the table literal the user passed */
	if ((heap_array_length(args) == 1) && (get_tree_kind(parser,args[0]) == TREE_NEW_TABLE)) {
		t=args[0];
	} else {
		t=tree_table(parser,tok.line,0);
	}

	/* todo: this is sort of inefficient, but if we don't issue
	the call instruction with a meta-field, the generator won't
	insert the 'this' parameter */
	t=tree_callcoreapi2(parser, tok.line, BUILTIN_SETMETA, t, meta);

	v=tree_meta_call(parser, meta->line, t, "__new", args);
	return v;
}



static treeID nametotree(Parser *parser, Source line, char *name, int lval) {

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
				index = heap_array_length(enc->tree_funexpr.capts);
				heap_array_add(enc->tree_funexpr.capts, entity.tree);
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



static treeID nametorval(Parser *parser, Source line, char *name) {
	return nametotree(parser, line, name, false);
}

static treeID nametolval(Parser *parser, Source line, char *name) {
	return nametotree(parser, line, name, true);
}





static treeID parse_unary(Parser *parser, bool unused) {
	Token tok = parser->tok;

	treeID v = Y_NULL;
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

			int index = 0;

			if (peek_tok(parser, TK_SQUARE_LEFT)) {

				Token index_token = take_tok(parser, TK_INTEGER);
				index = index_token.integer;

				take_tok(parser, TK_SQUARE_RIGHT);
			}

			if (index >= heap_array_length(loop->values)) {
				reporterror(parser->R, -1, "invalid index");
			}

			v = loop->values[index];

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
				int index = heap_array_grow(parser->inter->globals->array, 1);
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
			heap_array_add(args, v);

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
						heap_array_add(args, v);
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



static treeID parse_table(Parser *parser) {
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

		heap_array_add(key_value_tuples, tree_tuple2(parser, tok.line, x, y));

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
static treeID *parse_args(Parser *parser) {
	treeID x;
	treeID *z,*n;
	z=0;
	if (peek_tok(parser,TK_CURLY_LEFT)) {
		x=parse_table(parser);
		heap_array_add(z,x);
	} else if (pick_tok(parser,TK_PAREN_LEFT)) {
		pick_tok(parser,TK_COMMA);
		if (!peek_tok(parser,TK_PAREN_RIGHT)) do {
			x=parse_expr(parser,0);
			if(x!=Y_NULL){
				// desugar tuple expressions
				if (get_tree_kind(parser,x)==TREE_TUPLE) {
					n=get_tree(parser,x).z;
					FOR_ARRAY(i,n){
						heap_array_add(z,n[i]);
					}
				} else heap_array_add(z,x);
			} else break;
		} while (pick_tok(parser,TK_COMMA));
		take_tok(parser,TK_PAREN_RIGHT);
	} else {
		x=parse_expr(parser,0);
		if (x!=Y_NULL)heap_array_add(z,x);
	}
	return z;
}



// parse a postfix expression
static treeID parse_postfix(Parser *parser, int flags) {

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
						x=tree_table_field(parser,tok.line,v,y);
						heap_array_add(z,x);
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
					v=tree_table_field(parser,tok.line,v,field);
				}
			} break;
			case TK_SQUARE_SQUARE_LEFT: {
				take_tok(parser,TK_SQUARE_SQUARE_LEFT);
				treeID x = parse_expr(parser, 0);
				v = tree_index(parser,tok.line, v, x, 0);
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
							v = tree_table_field(parser,tok.line,v,z[i]);
						}
					}
					else if (x->kind == TREE_RANGE) {
						v = tree_ranged_index(parser,tok.line,v,x);
					}
					else {
						v = tree_table_field(parser,tok.line,v,x);
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



static treeID parse_subexpr(Parser *parser, int flags, int rank) {

	Token tok = parser->tok;

	treeID x,y;

	if (peek_tok(parser, TK_DOT_DOT)) {
		x = Y_NULL;
		// empty range
		// ... <y>
		goto parsey;
	}
	else {
		x = parse_postfix(parser, 0);
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

		y = parse_subexpr(parser, 0, prio);
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






static treeID parse_expr(Parser *parser, int flags) {
	switch (parser->tok.type) {
		case TK_NONE:
		case TK_FOR: case TK_WHILE:
		case TK_COMMA:
		case TK_PAREN_RIGHT: case TK_CURLY_RIGHT: case TK_SQUARE_RIGHT: {
			return Y_NULL;
		}
		default: ;
	}
	return parse_subexpr(parser, 0, 0);
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





static treeID parse_block(Parser *par) {

	begin_block(par);

	Token tok = par->tok;
	if (pick_tok(par, TK_CURLY_LEFT)) {

		while (parse_stat(par));

		take_tok_pair(par, TK_CURLY_RIGHT, tok);
	}
	else {
		parse_stat(par);
	}

	return close_block(par);
}




static void checkstoreto(Parser *par, Source line, treeID tree){


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
static void parse_assert(Parser *parser) {
	Token tok = take_tok(parser, TK_M_ASSERT);

	treeID cond = parse_expr(parser, 0);


	treeID *tassertargs = 0;
	heap_array_add(tassertargs, cond);

	Stringer sb = {};

	// todo: also, what's the point of doing any of this...
	switch (get_tree_kind(parser, cond)) {
		case EXPR_NEQ:   sb_writetextf(&sb, "(%% != %%) is false"); goto _bcase;
		case EXPR_EQ:    sb_writetextf(&sb, "(%% == %%) is false"); goto _bcase;
		case EXPR_OR:    sb_writetextf(&sb, "(%% || %%) is false"); goto _bcase;
		case EXPR_LT:    sb_writetextf(&sb, "(%% < %%) is false"); goto _bcase;
		case EXPR_GT:    sb_writetextf(&sb, "(%% > %%) is false"); goto _bcase;
		case EXPR_GTEQ:  sb_writetextf(&sb, "(%% >= %%) is false"); goto _bcase;
		case EXPR_LTEQ:  sb_writetextf(&sb, "(%% <= %%) is false"); goto _bcase;

		default: {

			parser_dialog(parser, tok.line, "expression not supported for #assert");
			goto esc;

			_bcase:
			// todo: leak! sb cannot be freed because trees still use
			// char *
			treeID tformatstr = tree_str(parser, tok.line, sb.buf);

			treeID tcallformat = tree_callcoreapi3(parser, tok.line
			, BUILTIN_FORMAT, tformatstr, cond->x, cond->y);

			heap_array_add(tassertargs, tcallformat);
		} break;
	}


	treeID v = tree_callcoreapi(parser, tok.line, BUILTIN_ASSERT, tassertargs);
	next_instr(parser, v);

	esc:
	return;
}





static void rewrite_lhs(Parser *parser, treeID lhs) {
	if (lhs->kind == TREE_TABLE_FIELD || lhs->kind == TREE_INDEX) {

		// already memory
		if (lhs->x->kind != TREE_MEMORY)
		{
			treeID v = tree_memory(parser, lhs->x->line, lhs->x);
			next_instr(parser, v);

			lhs->x = v;
		}
	}
}





static treeID *parse_rhs(Parser *parser, int nlhs, treeKi empty) {
	treeID *rhs = 0;

	do {
		if (pick_tok(parser, TK_DOT_DOT)) {

			// helper message
			if (heap_array_length(rhs) >= nlhs) {
				parser_dialog(parser, parser->tok.line
				, "redundant ellipsis, enough r-values already");
			}

			// '...' with no previous y's just means fill everything with nil,
			// which is what happens by default
			if (heap_array_length(rhs)) {
				treeID r = rhs[heap_array_length(rhs)-1];
				for (int i=heap_array_length(rhs); i<nlhs; ++i) {
					heap_array_add(rhs, r);
				}
			}


			// helper message
			if (peek_tok(parser, TK_COMMA)) {
				parser_dialog(parser, parser->tok.line
				, "'...': terminates the expression list");
			}
			break;
		}

		treeID v = parse_expr(parser, 0);
		if (notree(v)) break;

		heap_array_add(rhs, v);

		if (heap_array_length(rhs) > nlhs) {
			parser_dialog(parser, parser->tok.line
			, "warning: excess right hand side value");
		}
	} while (pick_tok(parser, TK_COMMA));

	// fill with nil
	for (int i=heap_array_length(rhs); i<nlhs; ++i) {
		heap_array_add(rhs, tree_nullary(parser, parser->tok.line, empty, NT_ANY));
	}
	return rhs;
}
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
static Token *parse_decl_names(Parser *par) {
	// todo: dynamic array!
	Token *s = 0;

	// (WORD ,) * :=
	// (WORD ,) * ::=
	while (peek_tok(par, TK_WORD) && peek_prox_tok(par, TK_COMMA)) {

		Token tok = next_tok(par);
		heap_array_add(s, tok);

		take_tok(par, TK_COMMA);
	}


	// = || := || ::=
	if (peek_tok(par, TK_WORD) && (peek_prox_tok(par, TK_BIND) || peek_prox_tok(par, TK_HARD_BIND) || peek_prox_tok(par, TK_ASSIGN)))
	{

		Token tok = next_tok(par);
		heap_array_add(s, tok);

	}

	return s;
}
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
static treeID *complete_lhs(Parser *parser, Token *names) {
	treeID *l = 0;

	// convert all names to storage targets
	for (int i=0; i<heap_array_length(names); ++i) {
		char *name = names[i].text;

		treeID v=nametolval(parser, names[i].line, name);
		if (v==Y_ERROR) goto _err;

		heap_array_add(l, v);
	}

	// parse additional expressions
	do {
		treeID v = parse_expr(parser, 0);
		if (notree(v)) goto _err;

		heap_array_add(l, v);
	} while (pick_tok(parser, TK_COMMA));


	_err:
	return l;
}
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
static int parse_expr_stat(Parser *parser) {
	int success = 0;

	Token tok = parser->tok;
	treeID *xs = 0, *ys = 0;


	Token *names = parse_decl_names(parser);

	// this is a declaration
	if (peek_tok(parser, TK_BIND) || peek_tok(parser, TK_HARD_BIND)) {

		if (heap_array_length(names) <= 0) {
			push_error(parser, tok.line, ERROR_INVALID_DECL, "invalid declaration, left hand side is not a name");
		}

		int status = ENTITY_ASSIGNED;

		if (pick_tok(parser, TK_HARD_BIND)) {
			status |= ENT_FLAG_CONSTANT;
		} else {
			take_tok(parser, TK_BIND);
		}

		ys = parse_rhs(parser, heap_array_length(names), TREE_NOP);
		ASSERT(heap_array_length(ys) >= heap_array_length(names));

		//
		//
		// todo: we can make this much neater by removing the whole
		// memory takes an initializer thing, instead generate a
		// store instruction, so you can take the same code path!
		//
		//	A top level store instruction will cause the memory tree
		// to receive a value, that way you don't allocate memory
		// before its first use!
		//
		//
		// note that we only bind the lhs once we've parsed
		// the rhs!
		for (int i=0; i<heap_array_length(names); i++)
		{
			char *name = names[i].text;

			int rem = heap_array_length(names) - i;

			treeID v = tree_memory2(parser, names[i].line, ys[i], rem);
			next_instr(parser, v);

			parser_bind(parser, names[i].line, status, name, v);
		}

	} else {

		treeID *xs = complete_lhs(parser, names);



		tok=parser->tok;


		if (pick_tok(parser,TK_ASSIGN)){
		} else if (pick_tok(parser,TK_NIL_ASSIGN)){
		} else if (tokenrank(tok.type) > 0){
			next_tok(parser);
			take_tok(parser,TK_ASSIGN);
		} else {
			goto _regularexpr;
		}

		for (int i=0; i<heap_array_length(xs); ++i) {
			checkstoreto(parser, tok.line, xs[i]);
		}


		ys = parse_rhs(parser, heap_array_length(xs), EXPR_NIL);
		ASSERT(heap_array_length(ys) >= heap_array_length(xs));


				// regular store
		if (tok.type==TK_ASSIGN)
		{
			for (int i=0; i<heap_array_length(xs); ++i)
			{
				MEMORY_SCOPE(parser) {
					treeID v = tree_store(parser, tok.line, xs[i], ys[i]);
					next_instr(parser, v);
				}
			}
		}
		else if (tok.type==TK_NIL_ASSIGN)
		{
			for (int i=0; i<heap_array_length(xs); ++i)
			{
				MEMORY_SCOPE(parser) {
					rewrite_lhs(parser, xs[i]);

					treeID c = tree_binary(parser, tok.line, EXPR_EQ, NT_ANY, xs[i], tree_nil(parser, tok.line));
					treeID s = tree_store(parser, tok.line, xs[i], ys[i]);
					treeID v = tree_if(parser, tok.line, c, s, 0);
					next_instr(parser, v);
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
			for (int i=0; i<heap_array_length(xs); ++i)
			{
				MEMORY_SCOPE(parser) {
					rewrite_lhs(parser, xs[i]);

					treeID o = tree_binary(parser, tok.line, tok2tree(tok.type), NT_ANY, xs[i], ys[i]);
					treeID v = tree_store(parser, tok.line, xs[i], o);
					next_instr(parser, v);
				}
			}
		}
		else {
			_regularexpr:
					// if they're regular expressions, just add them to the
					// block and the generator will figure out whether
					// to keep them or not
			FOR_ARRAY(i, xs) {
						// skip memory trees because otherwise the code
						// generator thinks we're trying to allocate the
						// same tree
				if (xs[i]->kind != TREE_MEMORY) {
					next_instr(parser, xs[i]);
				}
			}
		}
			// not a decl branch
	}

	free_heap_array(names);
	free_heap_array(ys);
	free_heap_array(xs);

	return success;
}




static int parse_stat(Parser *parser) {
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
			heap_array_add(parser->block.defers, v);
		} break;

		case TK_RET:
		case TK_HARD_ARROW:
		{
			next_tok(parser);
			treeID *l = parse_expr_list(parser, 0);

			treeID v = tree_ret2(parser, tok.line, l);
			next_instr(parser, v);

			// todo: I think is best if next_instr sets this
			parser->block.status = BLOCK_ENDED | BLOCK_HASRET;
		} break;

		case TK_BREAK:
		case TK_CONTINUE: {
			next_tok(parser);


			treeID v = Y_NULL;

			if (!tok.eol) {
				v = parse_expr(parser, 0);
			}

			Loop *loop = checkloop(parser, tok.line, v);

			if (tok.type == TK_CONTINUE) {
				v = tree_goto(parser, tok.line);
				heap_array_add(loop->continues, v);
			}
			else {
				v = tree_goto(parser, tok.line);
				heap_array_add(loop->breaks, v);
			}

			next_instr(parser, v);

			// todo: I think is best if next_instr sets this
			parser->block.status = BLOCK_ENDED;
		} break;

		case TK_CURLY_LEFT:
		{
			treeID v = parse_block(parser);
			next_instr(parser,v);
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
			next_instr(parser, v);

		} break;

		case TK_FOR: {
			parse_for(parser);
		} break;

		case TK_IF: {
			next_tok(parser);
			treeID v = parse_if(parser);
			next_instr(parser, v);
		} break;

		default: {
			parse_expr_stat(parser);
		} break;
	}

	esc:
	return success;

	_err:
	return 0;
}





static treeID parse_if(Parser *par){
	Token tok = par->tok;

	treeID v;

	// todo: why is this a block
	begin_block(par);
	{
		treeID pred = parse_expr(par,0);

		take_tok(par, TK_QMARK);

		treeID true_clause = parse_block(par);
		treeID else_clause = Y_NULL;

		// todo: elif != else if
		if (pick_tok(par,TK_ELIF)) {
			else_clause = parse_if(par);
		}
		else if (pick_tok(par,TK_ELSE)) {
			else_clause = parse_block(par);
		}

		v = tree_if(par, tok.line, pred, true_clause, else_clause);
		next_instr(par, v);
	}
	v = close_block(par);
	return v;
}



static treeID build_range_loop(Parser *parser, treeID expr, treeID index, treeID *values, treeID body)
{

	ASSERT(expr);
	ASSERT(index);
	ASSERT(values);
	ASSERT(body);


	Token tok = parser->tok;


	// caller must have created a loop!
	Loop *loop = checkloop(parser, tok.line, Y_NULL);


	treeID v;


	ASSERT(expr->kind == TREE_RANGE || expr->kind == EXPR_RANGE_INDEX);


	treeID rangelo = Y_NULL, rangehi = Y_NULL;
	treeID prebody = Y_NULL;

	if (expr->kind == EXPR_RANGE_INDEX) {
		ASSERT(expr->x != Y_NULL);
		ASSERT(expr->y != Y_NULL);

		treeID array = expr->x;
		treeID range = expr->y;
		ASSERT(range->kind == TREE_RANGE);

		rangelo = range->x;
		rangehi = range->y;

		begin_block(parser);
		{
			FOR_ARRAY(i, values) {
				// todo: special multi-index instruction?
				v = tree_index(parser, tok.line, array, index, i);
				v = tree_store(parser, tok.line, values[i], v);
				next_instr(parser, v);
			}
		}
		prebody = close_block(parser);

		rangelo = range->x, rangehi = range->y;

		if (rangelo == Y_NULL) rangelo = tree_int(parser, range->line, 0);
		if (rangehi == Y_NULL) rangehi = tree_length(parser, range->line, array);
	}
	// RANGE
	else {
		// value same as index
		index = values[0];

		begin_block(parser);
		{
			for(int i=1; i<heap_array_length(values); ++ i) {
				v = tree_add_int(parser, tok.line, index, i);
				v = tree_store(parser, tok.line, values[i], v);
				next_instr(parser, v);
			}
		}
		prebody = close_block(parser);

		rangelo = expr->x, rangehi = expr->y;

		if(rangelo == Y_NULL) rangelo = tree_int(parser,tok.line,0);
		// todo: if we don't have an upper bound replace condition entirely
		if(rangehi == Y_NULL) rangehi = tree_int(parser,tok.line,UINT_MAX);
	}


	ASSERT(rangelo != Y_NULL);
	ASSERT(rangehi != Y_NULL);


	// initialize index
	v = tree_store(parser,tok.line,index,rangelo);
	next_instr(parser, v);

	// generate predicate
	treeID pred = tree_less_than(parser, tok.line, index, rangehi);

	// what to do after the loop body's
	treeID probody = tree_add_int_store(parser, tok.line, index, heap_array_length(values));



	// todo: instead of generating all this crap, just have
	// label instructions! that we can reference!
	v = new_tree(parser, tok.line, TREE_WHILE_LOOP, NT_NON);
	v->loop.pred    = pred;
	v->loop.body    = body;
	v->loop.prebody = prebody;
	v->loop.probody = probody;
	v->loop.c       = loop->continues;
	v->loop.b       = loop->breaks;
	return v;
}




static treeID parse_tuple(Parser *parser) {

	treeID v = parse_expr(parser,0);
	if (notree(v)) goto esc;

	Token tok = parser->tok;

	// tuple
	if (pick_tok(parser, TK_COMMA)) {

		treeID y = parse_expr(parser,0);
		if (notree(y)) goto esc;

		v = tree_tuple2(parser, tok.line, v, y);

		// big tuple
		while(pick_tok(parser, TK_COMMA)) {

			y = parse_expr(parser,0);
			if (notree(y)) goto esc;

			heap_array_add(v->z, y);
		}
	}

	esc:
	return v;
}




// todo: check for references to loop labels, which are likely errors!
static treeID *parse_for_steps(Parser *par, int elem) {

	treeID *s = 0;

	do {

		treeID v;

		if (pick_tok(par, TK_PAREN_LEFT)) {
			v = parse_tuple(par);
			take_tok(par, TK_PAREN_RIGHT);
		}
		else {
			v = parse_tuple(par);
		}

		if (notree(v)) goto esc;

		heap_array_add(s, v);

	} while (pick_tok(par,TK_SEMI_COLON));

	esc:
	return s;
}

//
//
//
//
//
//
//
//

static bool parse_for(Parser *parser) {
	bool noerror = true;

	begin_block(parser);


	Token tok = take_tok(parser,TK_FOR);

	// todo: can we unify with regular statements?
	// a function that just returns a list of stores,
	// and then we desugar them? and that way is just
	// the one function?
	Token *names = parse_decl_names(parser);


	int status = ENTITY_ASSIGNED;

	if (pick_tok(parser, TK_HARD_BIND)) {

		status |= ENT_FLAG_CONSTANT;
	}

	else if (pick_tok(parser, TK_BIND)) {
	}

	// todo: for compatibility!
	else if (pick_tok(parser, TK_ASSIGN)) {

		push_warning(parser, tok.line, ERROR_SYNTAX_DEPRECATION, "consider using ':=' instead!");
	}


	treeID *steps = parse_for_steps(parser, heap_array_length(names));

	take_tok(parser, TK_QMARK);

	treeID *values = 0;

	for (int i=0; i<heap_array_length(names); ++i) {
		Token name = names[i];

		treeID v = tree_memory(parser, name.line, tree_nop(parser, name.line));
		next_instr(parser, v);

		heap_array_add(values, v);

		parser_bind(parser, name.line
		, ENTITY_REFERENCED|ENTITY_ASSIGNED|ENTITY_FORLOOP
		, name.text, v);
	}



	// todo: generate conditionally, per step?
	treeID index_ = tree_memory(parser,tok.line,tree_nop(parser,tok.line));
	next_instr(parser,index_);

	// begin a new loop
	Loop *loop = begin_loop(parser);
	loop->index  = index_;
	loop->values = values;

	// todo: ensure success!
	treeID body = parse_block(parser);


	treeID v;
	// generate loops or stores for each step
	FOR_ARRAY(i, steps) {
		treeID step = steps[i];

		// create a new block for each step
		begin_block(parser);

		// check if the step translates into a loop
		if (step->kind == TREE_RANGE || step->kind == EXPR_RANGE_INDEX) {
			v = build_range_loop(parser, step, index_, values, body);
			next_instr(parser, v);
		}
		// DESUGAR EXPRESSION
		else if (step->kind == TREE_TUPLE) {
			treeID index = index_;

			// todo: we shouldn't even have to do this!
			v = tree_store(parser, tok.line, index, tree_int(parser, tok.line, 0));
			next_instr(parser, v);

			ASSERT(step->x);
			ASSERT(step->y);

			v = tree_store(parser, tok.line, values[0], step->x);
			next_instr(parser, v);

			if (heap_array_length(values) > 1) {
				v = tree_store(parser, tok.line, values[1], step->y);
				next_instr(parser, v);
			}

			for (int i=0; i<heap_array_length(values)-2; ++i){
				if (i<heap_array_length(step->z)){
					v = step->z[i];
				}
				else {
					v = tree_nil(parser, tok.line);
				}
				v = tree_store(parser, tok.line, values[i+2], v);
				next_instr(parser, v);
			}

			next_instr(parser, body);

		}
		// SINGLE EXPRESSION
		else {
			treeID index = index_;
			treeID value = values[0];

			// todo: we shouldn't even have to do this!
			v = tree_store(parser, tok.line, index, tree_int(parser, tok.line, 0));
			next_instr(parser, v);

			v = tree_store(parser, tok.line, value, step);
			next_instr(parser, v);
			next_instr(parser, body);

			// todo: break and continue should move on to the next
			// step!
			if (loop->continues || loop->breaks) {

				FOR_ARRAY(i, loop->continues){
					parser_dialog(parser
					,  loop->continues[i]->line,"breaks and continues are only for ranges");
				}

				FOR_ARRAY(i, loop->breaks){
					parser_dialog(parser
					, loop->breaks[i]->line,"breaks and continues are only for ranges");
				}

				reporterror(parser->inter, 0, "invalid syntax");
			}
		}


		// end block for this step and add to parent block
		v = close_block(parser);
		next_instr(parser,v);
	}


	close_loop(parser);

	v = close_block(parser);
	next_instr(parser, v);

	esc:
	return noerror;
}





static void checkstk(Parser *parser, int state) {
	int index = parser->R->stack_ptr - parser->R->stack;
	if (index != state) {
		parser_dialog(parser, parser->tok.line, "internal error, invalid stack state");
		reporterror(parser->R, NO_BYTE, "internal error, invalid stack state");
	}
}




// todo: legitimize
// todo: also json is a constant
static int parse_constexpr(Parser *parser) {
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

			Tab tab = pushnewtable(parser->R);
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



static int parse_json_array(Parser *parser){
	int noerr = true;
	pushnewtable(parser->inter);

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



static int parse_json_object(Parser *parser) {
	int noerr = true;
	pushnewtable(parser->inter);

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



static int parse_json_value(Parser *parser) {
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


