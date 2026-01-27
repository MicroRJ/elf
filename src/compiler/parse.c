//
//	See Copyright Notice In elf.h
//
// Todo, make global declarations explicit!
// Todo, remove dynamic array usage!
//

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static TreeId parse_unary(Parser *parser, bool flags);
static TreeId parse_expr(Parser *parser, int flags);
static TreeId parse_subexpr(Parser *parser, int flags, int rank);
static TreeId parse_postfix(Parser *parser, int flags);
static TreeId parse_table(Parser *parser);
static TreeId parse_block(Parser *parser);
static int parse_stat(Parser *parser);
// the result is pushed onto the stack
static int parse_json_object(Parser *parser);
static int parse_json_value(Parser *parser);
static int parse_constexpr(Parser *parser);
static TreeChain parse_expr_list(Parser *parser, u32 flags);
static TreeId parse_if(Parser *parser);
static bool parse_for(Parser *parser);
static TreeChain parse_args(Parser *parser);
static void next_instr(Parser *parser, TreeId id);
static void add_this_param(Parser *parser, Source line);


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define tok2s(type) (g_token_metadata_table[type].name)

static int token_rank_from_type(int type)
{
	return g_token_metadata_table[type].rank;
}

static int tree_kind_from_token_type(int tok)
{
	switch (tok)
	{
		case TK_DOT_DOT:   return TREE_RANGE;
		case TK_LOG_AND:   return EXPR_AND;
		case TK_LOG_OR:    return EXPR_OR;
		case TK_NIL_OR:    return EXPR_NIL_OR;
		case TK_NIL_AND:   return EXPR_NIL_AND;
		case TK_ADD:       return EXPR_ADD;
		case TK_SUB:       return EXPR_SUB;
		case TK_DIV:       return EXPR_DIV;
		case TK_MUL:       return EXPR_MUL;
		case TK_POW:       return EXPR_POW;
		case TK_MOD:       return EXPR_MOD;
		case TK_NEQ:       return EXPR_NEQ;
		case TK_EQ:        return EXPR_EQ;
		case TK_GT:        return EXPR_GT;
		case TK_GTEQ:      return EXPR_GTEQ;
		case TK_LT:        return EXPR_LT;
		case TK_LTEQ:      return EXPR_LTEQ;
		case TK_SHL:       return EXPR_BIT_SHL;
		case TK_SHR:       return EXPR_BIT_SHR;
		case TK_BIT_XOR:   return EXPR_BIT_XOR;
		case TK_BIT_OR:    return EXPR_BIT_OR;
		case TK_BIT_AND:   return EXPR_BIT_AND;
		default:           return TREE_NONE;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef enum
{
	SEVERITY_NONCHALANT = 0,
	SEVERITY_NOTE,
	SEVERITY_WARNING,
	SEVERITY_FATAL,
}
Severity;

typedef enum
{
	ERROR_NONE = 0,

	INTERNAL_ERROR_INVALID_TREE,

	ERROR_UNEXPECTED_TOKEN,
	ERROR_INVALID_EXPRESSION,
	ERROR_EXPECTED_TOKEN,
	ERROR_NOT_IN_A_LOOP,
	ERROR_UNREACHABLE_STAT,
	ERROR_INVALID_CONTRACT_NAME,
	ERROR_INVALID_DECL,
	ERROR_SYNTAX_DEPRECATION,
	ERROR_INVALID_FIELD_INITIALIZER,
	ERROR_MISS_COMMA,
	ERROR_EXCESS_COMMA,
	ERROR_MISSING_FUNCTION_BODY,
	ERROR_INVALID_STORE_CLOSURE_VALUE,
	ERROR_INVALID_STORE_METAFIELD,
	ERROR_INVALID_STORE,
	ERROR_UNREFERENCED_ENTITY,
	ERROR_EXCESS_RVALUE,

	EC_INTERNAL_CANNOT_CAPTURE_NO_MEMORY,
}
Error;

#define push_error(par, err, line, fmt, ...) push_error_(par, SEVERITY_FATAL, err, line, fmt, __VA_ARGS__)
#define push_warning(par, err, line, fmt, ...) push_error_(par, SEVERITY_WARNING, err, line, fmt, __VA_ARGS__)
#define push_note(par, err, line, fmt, ...) push_error_(par, SEVERITY_NOTE, err, line, fmt, __VA_ARGS__)

static void push_error_(Parser *par, Severity severity, Error error, Source line, const char *format, ...)
{
	if (!line) line = par->tok.line;

	va_list vargs;
	va_start(vargs, format);
	char *message = tempvpf(format, vargs);
	va_end(vargs);

	parser_dialog(par, line, message);
	if (severity >= SEVERITY_FATAL) {
		reporterror(par->S, -1, message);
	}
}

static void warning_excess_rvalue(Parser *parser, TreeId v)
{
	push_error(parser, ERROR_EXCESS_RVALUE, v->line, "excess rvalue");
}

static void push_error_unexpected_token(Parser *parser, Token tok)
{
	push_error(parser, ERROR_UNEXPECTED_TOKEN, tok.line, "unexpected token");
}

static void push_error_expected_token(Parser *parser, Token tok, int expected)
{
	const char *message = tpf("got token: '%s', expected: '%s'", tok2s(tok.type), tok2s(expected));

	push_error(parser, ERROR_EXPECTED_TOKEN, tok.line, message);
}

static void push_error_expected_token_pair(Parser *parser, Token tok, int expected, Token pair)
{
	const char *message = tpf("got token: '%s', expected: '%s'", tok2s(tok.type), tok2s(expected));

	// todo: push note
	parser_dialog(parser, pair.line, "note: pair started here");

	push_error(parser, ERROR_EXPECTED_TOKEN, tok.line, message);
}

static void push_error_not_in_a_loop(Parser *parser, Source line)
{
	push_error(parser, ERROR_NOT_IN_A_LOOP, line, "not in a loop");
}

static void push_error_block_ended_expr_ignored(Parser *parser, TreeId v)
{
	// todo: generate hint using the current block!
	push_warning(parser, ERROR_UNREACHABLE_STAT, v->line, "statement ignored because the block ended");
}

static void push_error_invalid_field_initializer(Parser *par, Source line, TreeId x)
{
	push_error(par, ERROR_INVALID_FIELD_INITIALIZER, line, "invalid field initializer");
}

static void check_tree(Parser *parser, Source line, TreeId v)
{
	if (v <= Y_NULL){
		push_error(parser, ERROR_INVALID_EXPRESSION, v->line, "invalid expression");
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static Token next_tok(Parser *par) {
	Token tok = lex_token(par);
	#if 0
	switch (tok.type) {
		case TK_CURLY_LEFT: {
			ASSERT(par->tok_index < COUNTOF(par->tok_stack));
			par->tok_stack[par->tok_index ++] = tok;
		} break;
		case TK_CURLY_RIGHT: {
			ASSERT(par->tok_index > 0);
			Token pair = par->tok_stack[-- par->tok_index];
			if (pair.type != tok.type) {
				push_error_expected_token_pair(par, tok, pair.type, pair);

			}
		} break;
		default: ;
	}
	#endif
	return tok;
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

static inline bool peek_tok(Parser *par, int type) {
	return par->tok.type == type;
}

static inline bool pick_tok(Parser *par, int type) {
	if (peek_tok(par, type)) {
		next_tok(par);
		return true;
	}
	return false;
}

static Token take_tok(Parser *parser, int type) {
	Token tok = parser->tok;

	if (!pick_tok(parser, type)) {
		push_error_expected_token(parser, parser->tok, type);
	}

	return tok;
}

static inline bool peek_prox_tok(Parser *parser, int type) {
	return parser->tok_prox.type == type;
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
		if (~par->entities[i].status & ENTITY_BIT_REFERENCED)
		{
			// todo: in release mode, ignore these entities, in test mode keep them
			push_warning(par, ERROR_UNREFERENCED_ENTITY, par->entities[i].line
			, "warning: unreferenced entity");
		}
	}

	ASSERT(new_index <= par->entity_index);

	par->entity_index = new_index;
	par->scope --;
}

static Loop *begin_loop(Parser *parser)
{
	ASSERT(parser->loop_index < COUNTOF(parser->loop_stack));
	Loop *loop = & parser->loop_stack[parser->loop_index ++];
	zero_memory(loop, sizeof(* loop));
	return loop;
}

static void close_loop(Parser *parser)
{
	ASSERT(parser->loop_index > 0);
	-- parser->loop_index;
}

static Loop *get_loop(Parser *parser, TreeId name)
{
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





static Loop *checkloop(Parser *parser, Source line, TreeId name) {
	Loop *loop = get_loop(parser, name);
	if (!loop) {
		push_error_not_in_a_loop(parser, line);
	}
	return loop;
}



// add an instruction to the current block, instructions within a block
// are executed in order
static
void next_instr(Parser *par, TreeId v)
{
	Block *bl = & par->block;

	if (~bl->status & BLOCK_ENDED)
	{
		tree_chain_add(&bl->body, v);

		if (v->kind == TREE_RET)
		{
			bl->status |= BLOCK_HASRET;
		}

		if (v->kind == TREE_RET || v->kind == TREE_GOTO)
		{
			bl->status |= BLOCK_ENDED;
		}
	}
	else
	{
		push_error_block_ended_expr_ignored(par, v);
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

// --- Todo, fix-defer logic ---
// Defer statements must execute right after any block-terminating instruction.
// Who does this better, parser as sugar-coating or generator ?
static TreeId close_block(Parser *parser)
{
	ASSERT(parser->block_index > 0);

	close_scope(parser);

	Block bl = parser->block;

	parser->block = parser->block_stack[-- parser->block_index];

	FOR_ARRAY(i, bl.defers)
	{
		tree_chain_add(&bl.body, bl.defers[i]);
	}

	return tree_block(parser, parser->tok.line, bl.body);
}

static TreeId close_memory_region(Parser *par)
{
	TreeId bl = close_block(par);
	next_instr(par, bl);
	return bl;
}

// begin a new memory block, closes the memory block and adds to the
// parent block
#define MEMORY_SCOPE(par) SCOPED_PAIR(begin_block(par),close_memory_region(par))



// todo: we could just store the entity id within the
// tree, because now all entities point their own trees...
static int identify_tree(Parser *par, TreeId tree) {
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
			par->entities[id].status |= ENTITY_BIT_REFERENCED;
			return id;
		}
	}
	return NO_ENTITY;
}



static EntityId parser_bind(Parser *parser, Source line, int flags, char *name, TreeId tree) {
	{
		EntityId id = identify_name(parser, name);

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
	EntityId id = parser->entity_index ++;
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
	TreeId y = tree_nop(parser, line);

	TreeId x = tree_memory(parser, line, y);
	next_instr(parser, x);

	parser_bind(parser, line
	, ENTITY_BIT_PARAMETER|ENTITY_BIT_ASSIGNED|ENTITY_BIT_CONSTANT|ENTITY_BIT_REFERENCED
	, "this", x);
}








// todo: reset the parser just in case
static int parse_file(Parser *parser) {
	TreeId func = tree_new(parser, parser->tok.line, TREE_FUNCTION, NT_FUN);
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
typedef struct Param Param;
struct Param
{
	TreeId  tree;
	Token   rule;
};

typedef struct {
	const char *name;
	TypeRule    rule;
} NameRule;

// todo: string map!
static const NameRule typerules[] = {
	{"int"         , TRULE_INTEGER  },
	{"num"         , TRULE_NUMBER   },
	{"numeric"     , TRULE_NUMERIC  },
	{"callable"    , TRULE_CALLABLE },
	{"str"         , TRULE_STRING   },
	{"buf"         , TRULE_BUFFER   },
	{"non_nil"     , TRULE_NONNIL   },
};

// Todo, interning!
static void gen_typerule(Parser *par, TreeId memory, Token tok)
{
	for (int i = 0; i < COUNTOF(typerules); ++ i)
	{
		if (text_eq(typerules[i].name, tok.text))
		{
			TreeId v = tree_enforce(par, tok.line, memory, typerules[i].rule);
			next_instr(par, v);
			break;
		}
	}
#if 0
		case TRULE_NONNIL:
		{
			TreeId *args = 0;
			TreeId v;

			v = tree_binary(parser, line, EXPR_NEQ, NT_BOL, memory, tree_nil(parser, line));
			heap_array_add(args, v);

			v = tree_str(parser, line, "non_nil contract violation");
			heap_array_add(args, v);

			v = tree_callcoreapi(parser, line, BUILTIN_ASSERT, args);
			next_instr(parser, v);

		}
		break;
#endif
}

typedef struct
{
	u32 arity;
	b32 variadic;
}
FuncParams;

static FuncParams parse_function_params(Parser *par)
{
	u32 arity = 1;
	b32 variadic = false;

	Token tok = take_tok(par, TK_PAREN_LEFT);

	Param *params = 0;

	// add implicit 'this'
	add_this_param(par, tok.line);

	if (!peek_tok(par,TK_PAREN_RIGHT)) do
	{

		if (pick_tok(par, TK_DOT_DOT))
		{
			variadic = true;

			if (!peek_tok(par, TK_PAREN_RIGHT))
			{
				parser_dialog(par, par->tok.line, "expected ')' after '...', you may not have more parameters after '...'");
			}
			break;
		}


		Token name = take_tok(par, TK_WORD);

		TreeId pmem = tree_memory(par, name.line, tree_nop(par, name.line));
		next_instr(par, pmem);

		Token rule = {};

		if (pick_tok(par, TK_COLON))
		{
			rule = take_tok(par, TK_WORD);
		}

		// Todo, use arena!
		Param param = (Param){ pmem, rule };
		heap_array_add(params, param);
		++ arity;

		parser_bind(par, name.line
		, ENTITY_BIT_ASSIGNED | ENTITY_BIT_PARAMETER
		, name.text, pmem);

	} while (pick_tok(par,TK_COMMA));

	//
	// code for all contracts must be generated after
	// memory for all parameters has been allocated ...
	//
	for (int i = 1; i < arity; ++ i)
	{
		Param p = params[i-1];
		gen_typerule(par, p.tree, p.rule);
	}

	free_heap_array(params);

	take_tok(par,TK_PAREN_RIGHT);

	FuncParams func_params = {};
	func_params.arity = arity;
	func_params.variadic = variadic;
	return func_params;
}

static TreeId parse_function(Parser *parser) {
	Token tok = parser->tok;
	if (pick_tok(parser, TK_FUN)) {
	}
	// todo: experimental
	else if (pick_tok(parser, TK_FUNCTION)) {
	}
	else {
		NO_CODE;
	}

	TreeId enc = parser->enc;
	TreeId function;


	begin_block(parser);
	{
		// every function receives an inner scope
		function = tree_new(parser, tok.line, TREE_FUNCTION, NT_FUN);
		function->tree_funexpr.scope = parser->scope;
		function->tree_funexpr.enc   = enc;
		parser->enc = function;


		FuncParams params = parse_function_params(parser);
		function->tree_funexpr.arity = params.arity;
		function->tree_funexpr.variadic = params.variadic;

		// todo: remove! or optional?
		pick_tok(parser,TK_QMARK);

		if (peek_tok(parser,TK_CURLY_LEFT))
		{
			parse_stat(parser);

			// Todo, only if the block doesn't have a return already
			// Todo, "new" relies on this behavior, which is weird ...
			EntityId this_id = identify_name(parser, "this");
			ASSERT(this_id != -1);

			Entity this_en = parser->entities[this_id];
			TreeId this_rf = tree_proxy(parser, tok.line, this_en.tree);

			TreeId v = tree_return1(parser, tok.line, this_rf);
			next_instr(parser, v);
		}
		// direct return
		else if (pick_tok(parser, TK_HARD_ARROW))
		{
			TreeChain l = parse_expr_list(parser, 0);
			TreeId v = tree_return(parser, tok.line, l);
			next_instr(parser,v);
		}
		else {
			push_error(parser, ERROR_MISSING_FUNCTION_BODY
			, parser->tok.line, "missing function body, expected '{' or '-->'");
		}

		TreeId body = close_block(parser);
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
static TreeId parse_load(Parser *parser)
{
	Token tok = take_tok(parser,TK_LOAD);

	TreeChain z = parse_args(parser);

	TreeId v = tree_callcoreapi(parser, tok.line, BUILTIN_LOADFILE, z);
	return v;
}

//
// 'new' <meta-table> ( <argument-list> )
//
//	::=
//
// --> elf.set_meta({},Vector2):__new(x,y)
//
static TreeId parse_new(Parser *par)
{
	Token tok = take_tok(par, TK_NEW);


	TreeId c = parse_postfix(par, 0);

	// Todo, proper error handling!
	if (c->kind != TREE_CALL)
	{
		parser_dialog(par, tok.line, "invalid expression");
	}

	TreeId args = tree_get_first_call_arg(c);
	u32 nargs = tree_get_num_call_args(c);
	TreeId meta = tree_proxy1(par, tree_get_call_target(c));

	/*
	Todo, clean this up, what the hell!
	---
	check if we can reuse the table literal the user passed
	*/
	TreeId t;
	//	if ((nargs == 1) && (args->kind == TREE_NEW_TABLE))
	//	{
	//		t = args;
	//	}
	//	else
	{
		t = tree_table(par, tok.line, TREECHAIN_EMPTY);
	}

	/* todo: this is sort of inefficient, but if we don't issue
	the call instruction with a meta-field, the generator won't
	insert the 'this' parameter */
	t = tree_callcoreapi2(par, tok.line, BUILTIN_SETMETA, t, meta);

	TreeChain ch = { args, c->y, c->n - 1 };
	TreeId v = tree_meta_call(par, meta->line, t, "__new", ch);
	return v;
}

static TreeId tree_from_identifier(Parser *parser, Source line, char *name, b32 lval)
{
	TreeId v = Y_NULL;

	EntityId id = identify_name(parser, name);

	if (id != NO_ENTITY)
	{
		Entity entity = parser->entities[id];

		if (!lval)
		{
			if (~entity.status & ENTITY_BIT_ASSIGNED)
			{
				parser_dialog(parser, parser->tok.line, "warning: usage of possibly unassigned variable");
			}
		}


		// check if we have to capture this thing
		TreeId enc = parser->enc;

		if (entity.scope < enc->tree_funexpr.scope)
		{
			i32 index = -1;

			// Check if we've captured this already
			FOR_ARRAY(i, enc->tree_funexpr.capts)
			{
				if (enc->tree_funexpr.capts[i] == entity.tree)
				{
					index = i;
					break;
				}
			}

			// Capture it if not
			if (index == -1)
			{
				index = heap_array_length(enc->tree_funexpr.capts);
				heap_array_add(enc->tree_funexpr.capts, entity.tree);
			}

			v = tree_closure_value(parser, line, index);

			// goto next function
			enc = enc->tree_funexpr.enc;

			if (entity.scope < enc->tree_funexpr.scope)
			{
				parser_dialog(parser, line, "cannot capture?");
			}

		}
		else
		{
			v = tree_proxy(parser, line, entity.tree);
		}

	}
	else
	{
		// todo: we can only do this after we implement directories
		// because otherwise all of our libs break!
		// parser_dialog(parser, line, "undeclared identifier, if this is a global use the 'global' keyword or declare a global");
		v = tree_global_symbol(parser, line, name);
	}
	return v;
}



static TreeId nametorval(Parser *parser, Source line, char *name) {
	return tree_from_identifier(parser, line, name, false);
}

static TreeId nametolval(Parser *parser, Source line, char *name) {
	return tree_from_identifier(parser, line, name, true);
}


static void parse_dot_name(Parser *par, Stringer *s) {
	Token tok = take_tok(par, TK_DOT);
	do {
		if (tok.eol) break;

		tok = take_tok(par, TK_WORD);
		sb_writetextf(s, ".%s", tok.text);

		// this eol thing is weird to reason about...
		if (tok.eol) break;
	} while (pick_tok(par, TK_DOT));
}


static TreeId parse_unary(Parser *parser, bool unused) {
	Token tok = parser->tok;

	TreeId v = Y_NULL;
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
			v = parse_table(parser);
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

			TreeChain args = {};
			tree_chain_add(&args, v);

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
						tree_chain_add(&args, v);
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

// todo: multi-stores
// todo: switching to using '.' syntax to make could make this
// less ambiguous
static TreeId parse_table(Parser *par)
{
	Token tok = take_tok(par, TK_CURLY_LEFT);
	Token table_tok = tok;

	TreeChain args = {};

	TreeId x, y;

	while (!peek_tok(par, TK_NONE) && !peek_tok(par, TK_CURLY_RIGHT))
	{
		x = Y_NULL, y = Y_NULL;

		tok = par->tok;
		if (peek_tok(par, TK_WORD) && peek_prox_tok(par, TK_ASSIGN))
		{
			take_tok(par, TK_WORD);

			x = tree_str(par, tok.line, tok.text);
			if (notree(x)) goto _err_ii;

			take_tok(par, TK_ASSIGN);

			y = parse_expr(par, 0);
			if (notree(y)) goto _err_ii;

			check_tree(par, x->line, x);
			check_tree(par, y->line, y);
		}
		else if (pick_tok(par, TK_SQUARE_LEFT))
		{
			x = parse_expr(par,0);
			if (notree(x)) goto _err_ii;

			take_tok(par, TK_SQUARE_RIGHT);

			take_tok(par, TK_ASSIGN);

			y = parse_expr(par, 0);
			if (notree(y)) goto _err_ii;
		}
		else
		{
			x = y = parse_expr(par,0);
			if (notree(x)) goto _err_ii;

			if (pick_tok(par, TK_ASSIGN))
			{
				y = parse_expr(par, 0);
				if (notree(y)) goto _err_ii;
			}
			else
			{
				// no key
				x = Y_NULL;
			}
		}

		tok = par->tok;
		tree_chain_add(&args, tree_tuple2(par, tok.line, x, y));

		pick_tok(par, TK_COMMA);
	}

	take_tok(par, TK_CURLY_RIGHT);


	TreeId table = tree_table(par, table_tok.line, args);
	return table;

	_err:
	return Y_NULL;

	_err_ii:
	push_error_invalid_field_initializer(par, tok.line, y);
	return Y_NULL;
}

static TreeChain parse_expr_list(Parser *parser, u32 flags)
{
	TreeChain ch = {};
	do
	{
		TreeId v = parse_expr(parser, flags);
		if (notree(v)) goto esc;

		tree_chain_add(&ch, v);

	}
	while (pick_tok(parser, TK_COMMA));

	esc:
	return ch;
}

// Todo, just wrap in a tuple and make pipeline handle 1 element tuples homogeneously
static TreeId parse_tuple(Parser *par)
{
	Token tok = par->tok;
	TreeChain v = parse_expr_list(par, 0);
	TreeId r = v.head;
	if (v.tally != 1)
	{
		r = tree_tuple(par, tok.line, v);
	}
	return r;
}

// {x} | ( x { ... } ) | { <table-initializer-list> }
static TreeChain parse_args(Parser *par)
{
	TreeId x;
	TreeChain z = {};

	if (peek_tok(par, TK_CURLY_LEFT))
	{
		x = parse_table(par);
		tree_chain_add(&z, x);
	}
	else if (pick_tok(par, TK_PAREN_LEFT))
	{
		pick_tok(par, TK_COMMA);

		if (!peek_tok(par, TK_PAREN_RIGHT)) do
		{

			if (peek_tok(par, TK_PAREN_RIGHT))
			{
				push_error(par, ERROR_EXCESS_COMMA, par->tok_prev.line, "excess comma");
			}
			if (peek_tok(par, TK_COMMA))
			{
				push_error(par, ERROR_EXCESS_COMMA, 0, "excess comma");
			}

			x = parse_expr(par, 0);
			if (notree(x)) goto esc;

			// desugar tuple expressions
			if (x->kind == TREE_TUPLE)
			{
				// Todo, just append the tuple now that we have both x and y, which are the start and end parts
				for (TreeId i = x->x; i; )
				{
					TreeId k = i;
					i = i->next;
					k->next = 0;
					tree_chain_add(&z, i);
				}
			}
			else
			{
				tree_chain_add(&z, x);
			}

		}
		while (pick_tok(par, TK_COMMA));

		take_tok(par,TK_PAREN_RIGHT);
	}
	else
	{
		x = parse_expr(par, 0);
		if (notree(x)) goto esc;

		tree_chain_add(&z, x);
	}

	esc:
	return z;
}

static TreeId parse_field_postfix(Parser *par, TreeId x) {
	Token tok = take_tok(par, TK_DOT);

	// <expr>.(x,y) -> (<expr>.x, <expr>.y)
	if (pick_tok(par, TK_PAREN_LEFT))
	{
		TreeChain z = {};
		do
		{
			Token n = take_tok(par, TK_WORD);
			TreeId y = tree_str(par, n.cursor, n.text);

			TreeId v = tree_table_field(par, tok.cursor, x, y);
			tree_chain_add(&z, v);
		}
		while (pick_tok(par, TK_COMMA));

		x = tree_tuple(par, tok.cursor, z);
		take_tok(par, TK_PAREN_RIGHT);
	}
	// todo: for this return a new table with the fields x and y from 'table'
	// table.{x,y}
	else if (pick_tok(par, TK_CURLY_LEFT))
	{
		NO_CODE;
	}
	else
	{
		Token n = take_tok(par, TK_WORD);
		TreeId y = tree_str(par, n.cursor, n.text);
		x = tree_table_field(par, tok.cursor, x, y);
	}
	return x;
}

static TreeId parse_indirect_postfix(Parser *par, TreeId v)
{
	Token tok = take_tok(par, TK_SQUARE_LEFT);
	// A[B, C] -> A.B.C
	do
	{
		TreeId x = parse_expr(par, 0);
		if (iserror(x)) goto _err;

		// A [ B . (y, x) ] -> A [ B . y , B . x ]
		if (x->kind == TREE_TUPLE)
		{
			for (TreeId i = x->x; i; i = i->next)
			{
				v = tree_table_field(par, tok.line, v, i);
				if (iserror(v)) goto _err;
			}
		}
		else if (x->kind == TREE_RANGE)
		{
			v = tree_ranged_index(par,tok.line, v, x);
			if (iserror(v)) goto _err;
		}
		else
		{
			v = tree_table_field(par,tok.line, v, x);
			if (iserror(v)) goto _err;
		}
	}
	while(pick_tok(par,TK_COMMA));

	_err:
	// todo: attempt to get to the other ']' in case of failure
	take_tok(par,TK_SQUARE_RIGHT);
	return v;
}

// parse a postfix expression
static TreeId parse_postfix(Parser *parser, int flags) {

	Token tok = parser->tok;

	TreeId v = parse_unary(parser, flags);
	if (notree(v)) goto esc;

	while (parser->tok.type != TK_NONE && !parser->tok_prev.eol) {
		tok = parser->tok;

		switch (tok.type) {
			case TK_DOT: {
				v = parse_field_postfix(parser, v);
			} break;
			case TK_SQUARE_SQUARE_LEFT: {
				take_tok(parser,TK_SQUARE_SQUARE_LEFT);
				TreeId x = parse_expr(parser, 0);
				v = tree_index(parser,tok.line, v, x, 0);
				take_tok(parser,TK_SQUARE_SQUARE_RIGHT);
			} break;

			/* todo: make this nil safe, so [0,0] shouldn't
			fail if item at 0 is nil, we should have a separate
			instruction for getfield, which is like getfieldoptional
			or something to avoid having to generate additional code */
			case TK_SQUARE_LEFT:
			{
				v = parse_indirect_postfix(parser, v);
			}
			break;
			case TK_COLON:
			{
				next_tok(parser);
				Token name = take_tok(parser,TK_WORD);
				TreeId y = tree_str(parser, name.cursor, name.text);
				v = tree_meta_field(parser, tok.cursor, v, y);
			}
			break;
			case TK_CURLY_LEFT: case TK_PAREN_LEFT:
			{
				TreeChain z = parse_args(parser);
				v = tree_call(parser, tok.cursor, v, z);
			}
			break;

			default: goto esc;
		}
	}

	esc:;
	return v;
}

static TreeId parse_subexpr(Parser *parser, int flags, int rank)
{
	Token tok = parser->tok;

	TreeId x,y;

	if (peek_tok(parser, TK_DOT_DOT))
	{
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

		int prio = token_rank_from_type(tok.type);
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

		x = tree_binary(parser, tok.line, tree_kind_from_token_type(tok.type), NT_ANY, x, y);

		if (y == Y_NULL) goto esc;
	}

	esc:
	return x;
}

static TreeId parse_expr(Parser *parser, int flags) {
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


static TreeId parse_block(Parser *par) {

	begin_block(par);

	Token tok = par->tok;
	if (pick_tok(par, TK_CURLY_LEFT)) {

		while (parse_stat(par));

		take_tok_pair(par, TK_CURLY_RIGHT, tok);
	}
	else {
		// todo: uhm, why is this here? the caller should have called parse_stat
		parse_stat(par);
	}

	return close_block(par);
}

static void checkstoreto(Parser *par, Source line, TreeId tree){

	if (tree <= 0 || tree->type == NT_NON) {
		parser_dialog(par, line, "invalid l-value for store");
	}

	EntityId e = identify_tree(par, tree);

	if (e != NO_ENTITY) {
		if (par->entities[e].status & ENTITY_BIT_CONSTANT) {
			parser_dialog(par, line
			,	"reassignment of constant entity");
			parser_dialog(par, par->entities[e].line
			,	"see declaration");
		}
	}
}

// todo: this double evaluates the operands
// of the condition expression!
static void parse_assert(Parser *parser)
{
	Token tok = take_tok(parser, TK_M_ASSERT);

	TreeId cond = parse_expr(parser, 0);

	TreeChain args = {};
	tree_chain_add(&args, cond);

	Stringer sb = {};

	// todo: also, what's the point of doing any of this...
	switch (cond->kind)
	{
		case EXPR_NEQ:   sb_writetext(&sb, "(% != %) is false"); goto _bcase;
		case EXPR_EQ:    sb_writetext(&sb, "(% == %) is false"); goto _bcase;
		case EXPR_OR:    sb_writetext(&sb, "(% || %) is false"); goto _bcase;
		case EXPR_LT:    sb_writetext(&sb, "(% < %) is false"); goto _bcase;
		case EXPR_GT:    sb_writetext(&sb, "(% > %) is false"); goto _bcase;
		case EXPR_GTEQ:  sb_writetext(&sb, "(% >= %) is false"); goto _bcase;
		case EXPR_LTEQ:  sb_writetext(&sb, "(% <= %) is false"); goto _bcase;

		default: {

			parser_dialog(parser, tok.line, "expression not supported for #assert");
			goto esc;

			_bcase:
			// todo: leak! sb cannot be freed because trees still use
			// char *
			TreeId tformatstr = tree_str(parser, tok.line, sb.buf);

			TreeId tcallformat = tree_callcoreapi3(parser, tok.line
			, BUILTIN_FORMAT, tformatstr, cond->x, cond->y);

			tree_chain_add(&args, tcallformat);
		} break;
	}

	TreeId v = tree_callcoreapi(parser, tok.line, BUILTIN_ASSERT, args);
	next_instr(parser, v);
	esc:
	return;
}

// Todo, can we return a new tree instead of mutating the existing one?
static void rewrite_lhs(Parser *par, TreeId lhs)
{
	if (lhs->kind == TREE_TABLE_FIELD || lhs->kind == TREE_INDEX)
	{
		// already memory
		if (lhs->x->kind != TREE_MEMORY)
		{
			TreeId v = tree_memory(par, lhs->x->line, lhs->x);
			next_instr(par, v);

			lhs->x = v;
		}
	}
}

static TreeChain parse_rhs(Parser *parser, u32 num_lhs, TreeKind empty)
{
	TreeChain rhs = {};

	do
	{
		if (pick_tok(parser, TK_DOT_DOT))
		{
			// helper message
			if (rhs.tally >= num_lhs)
			{
				parser_dialog(parser, parser->tok.cursor
				, "redundant ellipsis, enough r-values already!");
			}

			// '...' with no previous y's just means fill everything with nil,
			// which is what happens by default
			if (rhs.tally)
			{
				TreeId r = rhs.tail;
				for (u32 i=rhs.tally; i<num_lhs; ++i)
				{
					tree_chain_add(&rhs, r);
				}
			}


			// helper message
			if (peek_tok(parser, TK_COMMA))
			{
				parser_dialog(parser, parser->tok.cursor
				, "'...': terminates the expression list");
			}
			break;
		}

		TreeId v = parse_expr(parser, 0);
		if (notree(v)) break;

		tree_chain_add(&rhs, v);

		if (rhs.tally > num_lhs)
		{
			parser_dialog(parser, parser->tok.cursor
			, "warning: excess right hand side value");
		}
	}
	while (pick_tok(parser, TK_COMMA));

	// fill with nil
	for (u32 i=rhs.tally; i<num_lhs; ++i)
	{
		tree_chain_add(&rhs, tree_nullary(parser, parser->tok.cursor, empty, NT_ANY));
	}
	return rhs;
}

// Todo, use linked list instead!
static Token *parse_decl_names(Parser *par)
{
	Token *s = 0;

	// (WORD ,) * :=
	// (WORD ,) * ::=
	while (peek_tok(par, TK_WORD) && peek_prox_tok(par, TK_COMMA))
	{
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

static TreeId *complete_lhs(Parser *parser, Token *names) {
	TreeId *l = 0;

	// convert all names to storage targets
	for (int i=0; i<heap_array_length(names); ++i) {
		char *name = names[i].text;

		TreeId v=nametolval(parser, names[i].line, name);
		if (v==Y_ERROR) goto _err;

		heap_array_add(l, v);
	}

	// parse additional expressions
	do {
		TreeId v = parse_expr(parser, 0);
		if (notree(v)) goto _err;

		heap_array_add(l, v);
	} while (pick_tok(parser, TK_COMMA));


	_err:
	return l;
}

static int parse_expr_stat(Parser *parser) {

	int success = false;

	Token tok = parser->tok;

	// Todo, remove dynamic array use!
	Token *names = parse_decl_names(parser);

	// this is a declaration
	if (peek_tok(parser, TK_BIND) || peek_tok(parser, TK_HARD_BIND))
	{
		if (heap_array_length(names) <= 0)
		{
			push_error(parser, ERROR_INVALID_DECL, tok.line
			, "invalid declaration, left hand side is not a name");
		}

		EntityBits status = ENTITY_BIT_ASSIGNED;

		if (pick_tok(parser, TK_HARD_BIND))
		{
			status |= ENTITY_BIT_CONSTANT;
		}
		else
		{
			take_tok(parser, TK_BIND);
		}

		TreeChain rhs = parse_rhs(parser, heap_array_length(names), TREE_NOP);
		ASSERT(rhs.tally >= heap_array_length(names));

		//
		// Todo:
		// Reason for memory tree taking an initializer is to
		// evaluate the initializer without first allocating
		// memory for the memory tree, which is what happens
		// if you do memory tree and then a store.
		// Getting rid of the initializer however, would make
		// this neater because the path for decls and stores
		// would merge.
		// ---
		// Could we make the generator, simply not allocate the memory
		// until it is first used or stored to?
		//
		// note that we only bind the lhs once we've parsed
		// the rhs!
		//
		TreeId next_rhs = rhs.head;
		for (int i = 0; i < heap_array_length(names); ++ i)
		{
			char *name = names[i].text;

			int rem = heap_array_length(names) - i;

			TreeId v = tree_memory2(parser, names[i].line, next_rhs, rem);
			next_instr(parser, v);

			parser_bind(parser, names[i].line, status, name, v);
			next_rhs = next_rhs->next;
		}

		success = true;
	}
	// not a decl
	else {

		TreeId *lhs = complete_lhs(parser, names);
		TreeChain rhs = {};

		tok = parser->tok;

		if (heap_array_length(lhs) <= 0) {
			goto _err;
		}

		if (pick_tok(parser,TK_ASSIGN)){
		}
		else if (pick_tok(parser,TK_NIL_ASSIGN)){
		}
		else if (token_rank_from_type(tok.type) > 0){
			next_tok(parser);
			take_tok(parser, TK_ASSIGN);
		}
		else {
			goto _regularexpr;
		}

		for (int i = 0; i < heap_array_length(lhs); ++ i) {
			checkstoreto(parser, tok.line, lhs[i]);
		}


		rhs = parse_rhs(parser, heap_array_length(lhs), EXPR_NIL);
		ASSERT(rhs.tally >= heap_array_length(lhs));


		// regular store
		if (tok.type == TK_ASSIGN)
		{
			TreeId next_rhs = rhs.head;
			for (int i=0; i<heap_array_length(lhs); ++i)
			{
				MEMORY_SCOPE(parser)
				{
					TreeId v = tree_store(parser, tok.line, lhs[i], next_rhs);
					next_instr(parser, v);
				}

				next_rhs = next_rhs->next;
			}
		}
		else if (tok.type == TK_NIL_ASSIGN)
		{
			TreeId next_rhs = rhs.head;
			for (int i=0; i<heap_array_length(lhs); ++i)
			{
				MEMORY_SCOPE(parser)
				{
					rewrite_lhs(parser, lhs[i]);

					TreeId c = tree_binary(parser, tok.line, EXPR_EQ, NT_ANY, lhs[i], tree_nil(parser, tok.line));
					TreeId s = tree_store(parser, tok.line, lhs[i], next_rhs);
					TreeId v = tree_if(parser, tok.line, c, s, 0);
					next_instr(parser, v);
				}
				next_rhs = next_rhs->next;
			}
		}
		else if (token_rank_from_type(tok.type) > 0)
		{
			//
			// if we got here it means that we attempted to
			// parse an expression that looked like a binary
			// operator, e.g '+''=', but it turned out to be
			// an assignment, so the parser returned the left
			// hand side expression but didn't take the token.
			//
			TreeId next_rhs = rhs.head;
			for (int i = 0; i < heap_array_length(lhs); ++ i)
			{
				MEMORY_SCOPE(parser)
				{
					rewrite_lhs(parser, lhs[i]);

					TreeId o = tree_binary(parser, tok.line, tree_kind_from_token_type(tok.type), NT_ANY, lhs[i], next_rhs);
					TreeId v = tree_store(parser, tok.line, lhs[i], o);
					next_instr(parser, v);
				}
				next_rhs = next_rhs->next;
			}
		}
		else
		{
			_regularexpr:
			// if they're regular expressions, just add them to the
			// block and the generator will figure out whether
			// to keep them or not
			FOR_ARRAY(i, lhs)
			{
				// top-level memory trees mean "allocate" memory, there's
				// no way to "reference" memory, either way, prune it.
				if (lhs[i]->kind != TREE_MEMORY)
				{
					next_instr(parser, lhs[i]);
				}
			}
		}

		success = true;
		free_heap_array(lhs);

		// not a decl branch
	}

	free_heap_array(names);

	_err:
	return success;
}

static int parse_stat(Parser *parser) {
	Token tok = parser->tok;

	int success = false;

	switch (tok.type) {

		case TK_NONE:
		case TK_CURLY_RIGHT:
		case TK_PAREN_RIGHT:
		case TK_THEN: case TK_ELSE: case TK_ELIF: {
		} break;

		case TK_M_ASSERT:
		{
			parse_assert(parser);
			success = true;
		}
		break;
		case TK_DEFER:
		{
			next_tok(parser);

			TreeId v = parse_block(parser);
			if (notree(v)) goto _err;

			heap_array_add(parser->block.defers, v);

			success = true;
		}
		break;
		case TK_RET:
		case TK_HARD_ARROW:
		{
			next_tok(parser);

			TreeChain l = parse_expr_list(parser, 0);
			if (iserror(l.head)) goto _err;

			TreeId v = tree_return(parser, tok.line, l);
			next_instr(parser, v);

			success = true;
		}
		break;
		case TK_BREAK:
		case TK_CONTINUE:
		{
			next_tok(parser);

			TreeId v = Y_NULL;

			if (!tok.eol)
			{
				v = parse_expr(parser, 0);
				if (iserror(v)) goto _err;
			}

			Loop *loop = checkloop(parser, tok.line, v);

			v = tree_goto(parser, tok.line);

			if (tok.type == TK_CONTINUE) {
				heap_array_add(loop->continues, v);
			}
			else {
				heap_array_add(loop->breaks, v);
			}

			next_instr(parser, v);
			success = true;
		}
		break;
		case TK_CURLY_LEFT:
		{
			TreeId v = parse_block(parser);
			if (iserror(v)) goto _err;
			next_instr(parser, v);
			success = true;
		} break;

		case TK_WHILE: {
			next_tok(parser);

			TreeId pred = parse_expr(parser, 0);
			if (iserror(pred)) goto _err;

			take_tok(parser, TK_QMARK);

			begin_loop(parser);

			TreeId body = parse_block(parser);
			if (iserror(body)) goto _err;

			close_loop(parser);

			// get the loop we just popped
			Loop loop = parser->loop_stack[parser->loop_index];

			TreeId v = tree_new(parser,tok.line,TREE_WHILE_LOOP,NT_NON);
			v->loop.pred = pred;
			v->loop.body = body;
			v->loop.c = loop.continues;
			v->loop.b = loop.breaks;
			next_instr(parser, v);

			success = true;
		} break;

		case TK_FOR: {
			success = parse_for(parser);
		} break;

		case TK_IF: {
			next_tok(parser);

			TreeId v = parse_if(parser);
			if (iserror(v)) goto _err;

			next_instr(parser, v);

			success = true;
		} break;

		default: {
			success = parse_expr_stat(parser);
		} break;
	}

	_err:
	return success;
}

static TreeId parse_if(Parser *par){
	Token tok = par->tok;

	TreeId v;

	// todo: why is this a block
	begin_block(par);
	{
		TreeId pred = parse_expr(par, 0);
		// todo: better hinting
		if (!peek_tok(par, TK_QMARK)) {
			push_error(par, ERROR_EXPECTED_TOKEN, pred->line, "expected '?' for 'if' statement");
		}
		take_tok(par, TK_QMARK);

		TreeId true_clause = parse_block(par);
		TreeId else_clause = Y_NULL;

		// todo: elif != else if
		if (pick_tok(par, TK_ELIF)) {
			else_clause = parse_if(par);
		}
		else if (pick_tok(par, TK_ELSE)) {
			else_clause = parse_block(par);
		}

		v = tree_if(par, tok.line, pred, true_clause, else_clause);
		next_instr(par, v);
	}
	v = close_block(par);
	return v;
}

static TreeId build_range_loop(Parser *parser, TreeId expr, TreeId index, TreeId *values, TreeId body)
{

	ASSERT(expr);
	ASSERT(index);
	ASSERT(values);
	ASSERT(body);


	Token tok = parser->tok;


	// caller must have created a loop!
	Loop *loop = checkloop(parser, tok.line, Y_NULL);


	TreeId v;

	ASSERT(expr->kind == TREE_RANGE || expr->kind == TREE_RANGE_INDEX);

	TreeId rangelo = Y_NULL, rangehi = Y_NULL;
	TreeId prebody = Y_NULL;

	if (expr->kind == TREE_RANGE_INDEX) {
		ASSERT(expr->x != Y_NULL);
		ASSERT(expr->y != Y_NULL);

		TreeId array = expr->x;
		TreeId range = expr->y;
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
	TreeId pred = tree_less_than(parser, tok.line, index, rangehi);

	// what to do after the loop body's
	TreeId probody = tree_add_int_store(parser, tok.line, index, heap_array_length(values));



	// todo: instead of generating all this crap, just have
	// label instructions! that we can reference!
	v = tree_new(parser, tok.line, TREE_WHILE_LOOP, NT_NON);
	v->loop.pred    = pred;
	v->loop.body    = body;
	v->loop.prebody = prebody;
	v->loop.probody = probody;
	v->loop.c       = loop->continues;
	v->loop.b       = loop->breaks;
	return v;
}

// todo: check for references to loop decls, which are likely errors!
static TreeId *parse_for_steps(Parser *par, int elem)
{
	TreeId *s = 0;

	do
	{
		Token tok = par->tok;

		TreeId v;
		if (pick_tok(par, TK_PAREN_LEFT))
		{
			v = parse_tuple(par);
			take_tok_pair(par, TK_PAREN_RIGHT, tok);
		}
		else
		{
			v = parse_tuple(par);
		}
		if (notree(v)) goto esc;

		heap_array_add(s, v);

	} while (pick_tok(par, TK_SEMI_COLON));

	esc:
	return s;
}

static int parse_for(Parser *parser) {
	int noerror = true;

	begin_block(parser);


	Token tok = take_tok(parser,TK_FOR);

	// todo: can we unify with regular statements?
	// a function that just returns a list of stores,
	// and then we desugar them? and that way is just
	// the one function?
	Token *names = parse_decl_names(parser);


	int status = ENTITY_BIT_ASSIGNED;

	if (pick_tok(parser, TK_HARD_BIND)) {

		status |= ENTITY_BIT_CONSTANT;
	}

	else if (pick_tok(parser, TK_BIND)) {
	}

	// todo: for compatibility!
	else if (pick_tok(parser, TK_ASSIGN)) {

		push_warning(parser, ERROR_SYNTAX_DEPRECATION, tok.line, "consider using ':=' instead!");
	}


	TreeId *steps = parse_for_steps(parser, heap_array_length(names));

	take_tok(parser, TK_QMARK);

	TreeId *values = 0;

	for (int i=0; i<heap_array_length(names); ++i) {
		Token name = names[i];

		TreeId v = tree_memory(parser, name.line, tree_nop(parser, name.line));
		next_instr(parser, v);

		heap_array_add(values, v);

		parser_bind(parser, name.line
		, ENTITY_BIT_REFERENCED|ENTITY_BIT_ASSIGNED|ENTITY_BIT_FORLOOP
		, name.text, v);
	}



	// todo: generate conditionally, per step?
	TreeId index_ = tree_memory(parser,tok.line,tree_nop(parser,tok.line));
	next_instr(parser,index_);

	// begin a new loop
	Loop *loop = begin_loop(parser);
	loop->index  = index_;
	loop->values = values;

	// todo: ensure success!
	TreeId body = parse_block(parser);


	TreeId v;
	// generate loops or stores for each step
	FOR_ARRAY(i, steps)
	{
		TreeId step = steps[i];

		// create a new block for each step
		begin_block(parser);

		// check if the step translates into a loop
		if (step->kind == TREE_RANGE || step->kind == TREE_RANGE_INDEX)
		{
			v = build_range_loop(parser, step, index_, values, body);
			next_instr(parser, v);
		}
		// DESUGAR EXPRESSION
		else if (step->kind == TREE_TUPLE)
		{
			TreeId index = index_;

			// todo: we shouldn't even have to do this!
			v = tree_store(parser, tok.line, index, tree_int(parser, tok.line, 0));
			next_instr(parser, v);

			ASSERT(v->n >= 2);

			TreeId k = step->x;
			for (u32 i = 0; i < heap_array_length(values); ++ i)
			{
				if (k)
				{
					v = k;
					k = k->next;
				}
				else
				{
					v = tree_nil(parser, tok.line);
					warning_excess_rvalue(parser, values[i]);
				}

				v = tree_store(parser, tok.line, values[i], v);
				next_instr(parser, v);
			}

			next_instr(parser, body);
		}
		// SINGLE EXPRESSION
		else
		{
			TreeId index = index_;
			TreeId value = values[0];

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

// todo: we have two paths for parsing stuff, instead reuse the tree parsing path
// and convert that to a constant...
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
				Token tok = parser->tok;
				if (pick_tok(parser, TK_WORD)) {
					pushtext(parser->R, tok.text);
				}
				else {
					ret = parse_constexpr(parser);
					// todo: instead attempt to do some error recovery
					if (notree(ret)) goto esc;
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

static int parse_json_value(Parser *parser)
{
	int success = true;

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
			success = false;
		} break;
	}
	return success;
}
