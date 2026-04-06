//
//	See Copyright Notice In elf.h
//
// Todo, make global declarations explicit!
// Todo, remove dynamic array usage!
//

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static AstRef elf_parse_unary_expr(Parser *parser);
static AstRef parse_expr(Parser *parser, int flags);
static AstRef parse_subexpr(Parser *parser, int flags, int rank);
static AstRef ELF_ParsePostfixExpr(Parser *parser, int flags);
static AstRef elf_parse_table_expr(Parser *parser);
static AstRef parse_ident_expr(Parser *par);
static AstRef parse_stat(Parser *parser);
// the result is pushed onto the stack
// static int parse_json_object(Parser *parser);
// static int parse_json_value(Parser *parser);
// static int parse_constexpr(Parser *parser);
static u32 elf_parse_expr_list(Parser *parser);
static AstRef elf_parse_if_stat(Parser *parser);
static bool parse_for(Parser *parser);
static u32 elf_parse_call_args(Parser *par);
// static void PushBlockStat(Parser *parser, AstRef id);
// static void add_this_param(Parser *parser, Source line);


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
		case TOK_ELLIPSIS:   return AST_RANGE;
		case TOK_LOG_AND:   return AST_AND;
		case TOK_LOG_OR:    return AST_OR;
		case TOK_NIL_OR:    return AST_NIL_OR;
		case TOK_NIL_AND:   return AST_NIL_AND;
		case TOK_ADD:       return AST_ADD;
		case TOK_SUB:       return AST_SUB;
		case TOK_DIV:       return AST_DIV;
		case TOK_MUL:       return AST_MUL;
		case TOK_POW:       return AST_POW;
		case TOK_MOD:       return AST_MOD;
		case TOK_NEQ:       return AST_NOT_EQ;
		case TOK_EQ:        return AST_EQ;
		case TOK_GT:        return AST_GREATER_THAN_EQ;
		case TOK_GTEQ:      return AST_GREATER_THAN_EQ;
		case TOK_LT:        return AST_LESS_THAN;
		case TOK_LTEQ:      return AST_LESS_THAN_EQ;
		case TOK_SHL:       return AST_SHIFT_LEFT;
		case TOK_SHR:       return AST_SHIFT_RIGHT;
		case TOK_BIT_XOR:   return AST_BITWISE_XOR;
		case TOK_BIT_OR:    return AST_BITWISE_OR;
		case TOK_BIT_AND:   return AST_BITWISE_AND;
		default:           return AST_NONE;
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
	// ERROR_UNREFERENCED_ENTITY,
	ERROR_EXCESS_RVALUE,
	ERROR_INVALID_STATEMENT,

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

static void warning_excess_rvalue(Parser *parser, AstRef v)
{
	push_error(parser, ERROR_EXCESS_RVALUE, v->site, "excess rvalue");
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

static void push_error_block_ended_expr_ignored(Parser *parser, AstRef v)
{
	// todo: generate hint using the current block!
	push_warning(parser, ERROR_UNREACHABLE_STAT, v->site, "statement ignored because the block ended");
}

static void push_error_invalid_field_initializer(Parser *par, Source line, AstRef x)
{
	push_error(par, ERROR_INVALID_FIELD_INITIALIZER, line, "invalid field initializer");
}

static void check_tree(Parser *parser, Source line, AstRef v)
{
	if (v <= Y_NULL){
		push_error(parser, ERROR_INVALID_EXPRESSION, v->site, "invalid expression");
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void elf_destroy_parser(Parser *parser)
{
	// ...
}

static Parser *elf_alloc_parser(elf_State *state, elf_Arena *arena)
{
	u32 tree_stack_size = 4096;
	Parser *parser = elf_arena_push_zero(arena, sizeof(*parser));
	parser->state = state;
	parser->arena = arena;

	parser->tree_stack = elf_arena_push_zero(arena, sizeof(*parser->tree_stack) * tree_stack_size);
	parser->tree_stack_size = tree_stack_size;
	parser->tree_stack_index = 0;

	u32 function_buffer_size = 1024;
	parser->functions = elf_arena_push_zero(arena, sizeof(*parser->functions) * function_buffer_size);
	return parser;
}

static void elf_reposition_parser(Parser *parser, char *cursor);

static Parser *elf_create_parser(elf_State *state, elf_Arena *arena, const char *name, const char *data)
{
	ASSERT(state != 0);
	ASSERT(name != 0);
	ASSERT(data != 0);
	Parser *parser = elf_alloc_parser(state, arena);
	ASSERT(strlen(name) < sizeof(parser->name));
	elf_copy_memory(parser->name, name, strlen(name) + 1);
	parser->source = (char *) data;
	elf_reposition_parser(parser, (char *) data);
	return parser;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static Token consume_token(Parser *par)
{
	Token tok = lex_token(par);
	return tok;
}

static void elf_reposition_parser(Parser *parser, char *cursor)
{
	parser->cursor = cursor;
	// so that I don't forget to do this!
	consume_token(parser);
	consume_token(parser);
}

static inline bool peek_tok(Parser *par, int type) {
	return par->tok.type == type;
}

static inline bool pick_token(Parser *par, int type) {
	if (peek_tok(par, type)) {
		consume_token(par);
		return true;
	}
	return false;
}

static Token take_token(Parser *parser, int type) {
	Token tok = parser->tok;

	if (!pick_token(parser, type)) {
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
	return peek_tok_inl(parser,k) && (consume_token(parser), 1);
}

// todo: to be deprecated! explicit new line token!
static Token get_token_inline(Parser *parser, int type) {
	Token tok = parser->tok;

	if (!pick_tok_inl(parser, type)) {
		push_error_expected_token(parser, parser->tok, type);
	}

	return tok;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void push_ast(Parser *par, AstRef tree)
{
	ASSERT(par->tree_stack_index < par->tree_stack_size);
	par->tree_stack[par->tree_stack_index ++] = tree;
}

// Todo, what if instead here we actually returned an AstArray *?
// struct AstArray { u64 size; Ast *asts[] };
static AstRef *pop_ast_array(Parser *par, u32 ntrees)
{
	ASSERT(par->tree_stack_index >= ntrees);
	par->tree_stack_index -= ntrees;
	AstRef *copy = elf_arena_push_copy(par->arena, sizeof(*copy) * ntrees, par->tree_stack + par->tree_stack_index);
	return copy;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static AstRef elf_parse_file(Parser *par)
{
	Token tok = par->tok;
	u32 nstats = 0;
	while (!peek_tok(par, TOK_NONE))
	{
		Ast *stat = parse_stat(par);
		if (notree(stat)) break;

		push_ast(par, stat);
		++ nstats;
	}

	AstRef *stats = pop_ast_array(par, nstats);
	AstRef body = create_block_ast(par, tok.site, stats, nstats);

	AstRef file_ast = create_file_ast(par, tok.site, body);
	return file_ast;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static AstRef elf_parse_function(Parser *par)
{
	Token tok = par->tok;

	// Todo, figure this out ...
	if (pick_token(par, TOK_FUN) || pick_token(par, TOK_FUNCTION)) {
	}
	else NO_CODE;

	u32 nparams = 0;
	take_token(par, TOK_PAREN_LEFT);
	if (!peek_tok(par, TOK_PAREN_RIGHT)) do
	{
		Token tok = par->tok;
		if (pick_token(par, TOK_ELLIPSIS))
		{
			AstRef param = elf_new_ellipsis_tree(par, tok.site);
			push_ast(par, param);
			++ nparams;
		}
		else
		{
			AstRef name = parse_ident_expr(par);
			AstRef type = 0;
			AstRef expr = 0;
			if (pick_token(par, TOK_COLON)) {
				type = parse_expr(par, 0);
			}
			if (pick_token(par, TOK_ASSIGN)) {
				expr = parse_expr(par, 0);
			}
			AstRef param = create_param_ast(par, tok.site, name, type, expr);
			push_ast(par, param);
			++ nparams;
		}
	}
	while (pick_token(par, TOK_COMMA));
	take_token(par, TOK_PAREN_RIGHT);

	AstRef *params = pop_ast_array(par, nparams);
	AstRef body = parse_stat(par);

	AstRef function = create_function_ast(par, tok.site, params, nparams, body);
	return function;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static AstRef parse_tuple_expr(Parser *par)
{
	Token tok = par->tok;
	u32 nargs = 0;
	do
	{
		AstRef expr = parse_expr(par, 0);
		if (!expr) break;
		push_ast(par, expr);
		++ nargs;
	}
	while (pick_token(par, TOK_COMMA));

	AstRef tuple = 0;
	if (nargs)
	{
		AstRef *args = pop_ast_array(par, nargs);
		tuple = create_tuple_ast(par, tok.site, args, nargs);
	}
	return tuple;
}

static AstRef parse_semicolon_expr(Parser *par)
{
	AstRef x = parse_tuple_expr(par);
	while (peek_tok(par, TOK_SEMI_COLON))
	{
		Token tok = consume_token(par);
		AstRef y = parse_tuple_expr(par);
		x = elf_new_semi_colon_expr_tree(par, tok.site, x, y);
	}
	return x;
}

// Todo, deprecate?!
//
// 'load' ( <file-name> )
//
//  elf.load_file(<file-name>)
//
static AstRef elf_parse_load_stat(Parser *par)
{
	Token tok = take_token(par, TOK_LOAD);

	u32 nargs = elf_parse_call_args(par);
	AstRef *args = pop_ast_array(par, nargs);

	AstRef v = tree_callcoreapi(par, tok.line, BUILTIN_LOADFILE, args, nargs);
	return v;
}

//
// 'new' <meta-table> ( <argument-list> )
//
//	::=
//
// --> elf.set_meta({},Vector2):__new(x,y)
//
static AstRef parse_new(Parser *par)
{
	Token tok = take_token(par, TOK_NEW);

	AstRef call_expr = ELF_ParsePostfixExpr(par, 0);
	// Todo, proper error handling!
	if (call_expr->kind != AST_CALL) {
		parser_dialog(par, tok.line, "invalid expression");
	}
	// Todo, just have a bultin for doing this!
	AstRef meta = call_expr->ast_call_expr.expr;
	AstRef *args = call_expr->ast_call_expr.args;
	u32 nargs = call_expr->ast_call_expr.nargs;

	AstRef table = elf_new_table_expr_tree(par, tok.line, 0, 0);
	table = tree_callcoreapi2(par, tok.line, BUILTIN_SETMETA, table, meta);
	AstRef v = ELF_NewMetaCallTree(par, meta->site, table, "__new", args, nargs);
	return v;
}



static void parse_dot_name(Parser *par, Stringer *s) {
	Token tok = take_token(par, TOK_DOT);
	do {
		if (tok.eol) break;

		tok = take_token(par, TOK_IDENTIFIER);
		sb_writetextf(s, ".%s", tok.text);

		// this eol thing is weird to reason about...
		if (tok.eol) break;
	} while (pick_token(par, TOK_DOT));
}


static AstRef parse_ident_expr(Parser *par)
{
	Token tok = take_token(par, TOK_IDENTIFIER);
	return create_ident_ast(par, tok.site, tok.text);
}

static AstRef elf_parse_unary_expr(Parser *parser)
{
	Token tok = parser->tok;

	AstRef v = 0;
	switch (tok.type)
	{
		case TOK_ELLIPSIS:
		{
			consume_token(parser);
			v = elf_new_ellipsis_tree(parser, tok.site);
		}
		break;
		case TOK_IDENTIFIER:
		{
			v = parse_ident_expr(parser);
		}
		break;
		case TOK_NIL:
		{
			consume_token(parser);
			v = tree_nil(parser,tok.line);
		}
		break;
		case TOK_TRUE:{
			consume_token(parser);
			v = tree_int(parser,tok.line,1);
		}
		break;
		case TOK_FALSE:
		{
			consume_token(parser);
			v = tree_int(parser,tok.line,0);
		}
		break;
		case TOK_LETTER:
		case TOK_INTEGER:
		{
			consume_token(parser);
			v = tree_int(parser,tok.line,tok.integer);
		}
		break;
		case TOK_NUMBER:
		{
			consume_token(parser);
			v = tree_num(parser,tok.line,tok.number);
		}
		break;
		case TOK_STRING:
		{
			consume_token(parser);
			v = create_str_ast(parser,tok.line,tok.text);
		}
		break;



#if 0
		case TOK_M_INDEX:
		{
			consume_token(parser);

			// todo: handle operand '#index(<loop-name>)'
			Loop *loop = checkloop(parser, tok.line, Y_NULL);

			v = loop->index;
			ASSERT(v != Y_NULL);
		}
		break;

		case TOK_M_VALUE:
		{
			consume_token(parser);

			// todo: handle operand '#index(<loop-name>)'
			Loop *loop = checkloop(parser, tok.line, Y_NULL);

			int index = 0;

			if (peek_tok(parser, TOK_SQUARE_LEFT)) {

				Token index_token = take_token(parser, TOK_INTEGER);
				index = index_token.integer;

				take_token(parser, TOK_SQUARE_RIGHT);
			}

			if (index >= heap_array_length(loop->values)) {
				reporterror(parser->R, -1, "invalid index");
			}

			v = loop->values[index];

			ASSERT(v != Y_NULL);
		}
		break;

		case TOK_M_GETMEM:
		{
			consume_token(parser);
			v=parse_subexpr(parser,0,10000);
			v=elf_new_unary_tree(parser,tok.line,TREE_DEBUG_GET_MEMORY,v);
		}
		break;

		case TOK_M_GETEXPR:
		{
			consume_token(parser);
			v=parse_subexpr(parser,0,10000);
			v=elf_new_unary_tree(parser,tok.line,TREE_DEBUG_GET_EXPRESSION_NAME,v);
		}
		break;
#endif

		case TOK_TILDE:
		{
			consume_token(parser);
			v = parse_subexpr(parser, 0, 10000);
			v = elf_new_unary_tree(parser, tok.site, AST_BITWISE_NOT, v);
		}
		break;

		// the global keyword tells the parser that a name is a global
		case TOK_GLOBAL: {
			consume_token(parser);
			if (pick_token(parser, TOK_PAREN_LEFT)){
				Token name=take_token(parser, TOK_IDENTIFIER);
				v=tree_global_symbol(parser, name.line, name.text);
				take_token(parser, TOK_PAREN_LEFT);
			} else {
				Token name=take_token(parser, TOK_IDENTIFIER);
				v=tree_global_symbol(parser, name.line, name.text);
			}
		} break;

		case TOK_SUB:
		{
			consume_token(parser);
			v=parse_subexpr(parser,0,10000);
			v=create_binary_expr_ast(parser,tok.line,AST_SUB,tree_int(parser,tok.line,0),v);
		}
		break;
		case TOK_ADD:
		{
			consume_token(parser);
			v = parse_subexpr(parser,0,10000);
		}
		break;
#if 0
		case TOK_JSON:
		{
			//
			//	todo: we need some constant pool, for now we
			// add to globals...
			//

			Token tok = take_token(parser,TOK_JSON);

			int noerr = parse_json_object(parser);
			if (noerr) {

				// todo:
				V json = popvalue(parser->inter);
				int index = dynamic_array_allocate(parser->inter->globals->array, 1);
				parser->inter->globals->array[index] = json;


				v = tree_global(parser, tok.line, index);
			}
		}
		break;
#endif
		case TOK_NEW: {
			v = parse_new(parser);
		} break;
		case TOK_LOAD: {
			v = elf_parse_load_stat(parser);
		} break;

		case TOK_CURLY_LEFT: {
			v = elf_parse_table_expr(parser);
		} break;

#if 0
		case TOK_ELF:
		{
			char sym[256] = {};

			// elf is a keyword!
			consume_token(parser);

			if (!peek_tok_inl(parser, TOK_DOT))
			{
				parser_dialog(parser, tok.site, "incomplete symbol, expected '.' on the same line as 'elf'. Did you mean to use 'elf'? This is a reserved keyword and it refers to the elf directory.");
			}
			strcat(sym,"elf");

			take_token(parser, TOK_DOT);

			do
			{
				get_token_inline(parser, TOK_IDENTIFIER);
				strcat(sym,".");
				strcat(sym,parser->tok_prev.text);
			}
			while(pick_tok_inl(parser,TOK_DOT));

			v = tree_global_symbol(parser,tok.line,sym);
		}
		break;
#endif

		case TOK_PAREN_LEFT: {
			consume_token(parser);
			if (!peek_tok(parser,TOK_PAREN_RIGHT)) {
				v=parse_expr(parser,0);
			}
			take_token(parser,TOK_PAREN_RIGHT);
		} break;

		case TOK_FUNCTION:
		case TOK_FUN:
		{
			v = elf_parse_function(parser);
		}
		break;

#if 0
		case TOK_FORMAT_STRING: {
			consume_token(parser);

			// todo: leak!
			// the format string can only get shorter
			char *heapbuf = malloc(strlen(tok.text) + 1);
			char *write = heapbuf;

			v = create_str_ast(parser,tok.line,heapbuf);

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
						elf_reposition_parser(parser, format);
						if (!pick_token(parser, TOK_CURLY_LEFT)) {
							parser_dialog(parser, 0, "expected '{'");
						}
						v = parse_expr(parser, 0);
						tree_chain_add(&args, v);
						if (!pick_token(parser, TOK_CURLY_RIGHT)) {
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
#endif
		// it's ok
		default: ;
	}

	esc:
	return v;
}

// Todo, multi-stores
static AstRef elf_parse_table_expr(Parser *par)
{
	Token tok = take_token(par, TOK_CURLY_LEFT);
	Token table_tok = tok;

	AstRef x, y;

	u32 nargs = 0;
	while (!peek_tok(par, TOK_NONE) && !peek_tok(par, TOK_CURLY_RIGHT))
	{
		x = Y_NULL, y = Y_NULL;

		tok = par->tok;
		if (peek_tok(par, TOK_IDENTIFIER) && peek_prox_tok(par, TOK_ASSIGN))
		{
			take_token(par, TOK_IDENTIFIER);

			x = create_str_ast(par, tok.line, tok.text);
			if (notree(x)) goto _err_ii;

			take_token(par, TOK_ASSIGN);

			y = parse_expr(par, 0);
			if (notree(y)) goto _err_ii;

			check_tree(par, x->site, x);
			check_tree(par, y->site, y);
		}
		else if (pick_token(par, TOK_SQUARE_LEFT))
		{
			x = parse_expr(par,0);
			if (notree(x)) goto _err_ii;

			take_token(par, TOK_SQUARE_RIGHT);

			take_token(par, TOK_ASSIGN);

			y = parse_expr(par, 0);
			if (notree(y)) goto _err_ii;
		}
		else
		{
			x = y = parse_expr(par,0);
			if (notree(x)) goto _err_ii;

			if (pick_token(par, TOK_ASSIGN))
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
		AstRef pair = elf_new_table_entry_tree(par, tok.site, x, y);
		push_ast(par, pair);

		// Todo, are we going to make this required or no?
		pick_token(par, TOK_COMMA);
	}

	take_token(par, TOK_CURLY_RIGHT);

	AstRef *args = pop_ast_array(par, nargs);
	AstRef table = elf_new_table_expr_tree(par, table_tok.site, args, nargs);
	return table;

	_err:
	return Y_NULL;

	_err_ii:
	push_error_invalid_field_initializer(par, tok.line, y);
	return Y_NULL;
}

static u32 elf_parse_call_args(Parser *par)
{
	u32 nargs = 0;

	if (peek_tok(par, TOK_CURLY_LEFT))
	{
		AstRef expr = elf_parse_table_expr(par);
		push_ast(par, expr);
	}
	else if (pick_token(par, TOK_PAREN_LEFT))
	{
		pick_token(par, TOK_COMMA);

		if (!peek_tok(par, TOK_PAREN_RIGHT)) do
		{

			if (peek_tok(par, TOK_PAREN_RIGHT))
			{
				push_error(par, ERROR_EXCESS_COMMA, par->tok_prev.line, "excess comma");
			}
			if (peek_tok(par, TOK_COMMA))
			{
				push_error(par, ERROR_EXCESS_COMMA, 0, "excess comma");
			}

			AstRef x = parse_expr(par, 0);
			if (notree(x)) goto esc;

			// Todo, what the heck were we doing here?
			// --------------------------------------
			// desugar tuple expressions
			//	if (x->kind == AST_TUPLE)
			//	{
			//		// Todo, just append the tuple now that we have both x and y, which are the start and end parts
			//		for (AstRef i = x->x; i; )
			//		{
			//			AstRef k = i;
			//			i = i->next;
			//			k->next = 0;
			//			push_ast(par, i);
			//			++ nargs;
			//		}
			//	}
			//	else
			{
				push_ast(par, x);
				++ nargs;
			}

		}
		while (pick_token(par, TOK_COMMA));

		take_token(par,TOK_PAREN_RIGHT);
	}
	else
	{
		AstRef expr = parse_expr(par, 0);
		if (notree(expr)) goto esc;
		push_ast(par, expr);
		++ nargs;
	}

	esc:
	return nargs;
}

static AstRef parse_field_postfix(Parser *par, AstRef x)
{
	Token tok = take_token(par, TOK_DOT);

	// Todo, why do we do this?
	// <expr>.(x,y) -> (<expr>.x, <expr>.y)
	if (pick_token(par, TOK_PAREN_LEFT))
	{
		u32 nargs = 0;
		do
		{
			Token n = take_token(par, TOK_IDENTIFIER);
			AstRef y = create_str_ast(par, n.site, n.text);

			AstRef v = elf_new_field_expr_tree(par, tok.site, x, y);
			push_ast(par, v);
			++ nargs;
		}
		while (pick_token(par, TOK_COMMA));

		AstRef *args = pop_ast_array(par, nargs);
		x = create_tuple_ast(par, tok.site, args, nargs);
		take_token(par, TOK_PAREN_RIGHT);
	}
	// todo: for this return a new table with the fields x and y from 'table'
	// table.{x,y}
	else if (pick_token(par, TOK_CURLY_LEFT))
	{
		NO_CODE;
	}
	else
	{
		AstRef y = parse_ident_expr(par);
		x = elf_new_field_expr_tree(par, tok.site, x, y);
	}
	return x;
}

static AstRef parse_indirect_postfix(Parser *par, AstRef v)
{
	Token tok = take_token(par, TOK_SQUARE_LEFT);
	// A[B, C] -> A.B.C
	do
	{
		AstRef x = parse_expr(par, 0);
		if (iserror(x)) goto _err;

		// A [ B . (y, x) ] -> A [ B . y , B . x ]
		if (x->kind == AST_TUPLE)
		{
			__debugbreak();
			//	for (AstRef i = x->x; i; i = i->next)
			//	{
			//		v = elf_new_field_expr_tree(par, tok.line, v, i);
			//		if (iserror(v)) goto _err;
			//	}
		}
		else if (x->kind == AST_RANGE)
		{
			v = tree_ranged_index(par,tok.line, v, x);
			if (iserror(v)) goto _err;
		}
		else
		{
			v = elf_new_field_expr_tree(par,tok.line, v, x);
			if (iserror(v)) goto _err;
		}
	}
	while(pick_token(par,TOK_COMMA));

	_err:
	// todo: attempt to get to the other ']' in case of failure
	take_token(par,TOK_SQUARE_RIGHT);
	return v;
}

static AstRef ELF_ParsePostfixExpr(Parser *parser, int flags)
{

	Token tok = parser->tok;

	AstRef v = elf_parse_unary_expr(parser);
	if (notree(v)) goto esc;

	while (parser->tok.type != TOK_NONE && !parser->tok_prev.eol) {
		tok = parser->tok;

		switch (tok.type) {

			case TOK_DOT:
			{
				v = parse_field_postfix(parser, v);
			}
			break;
			case TOK_SQUARE_SQUARE_LEFT:
			{
				take_token(parser,TOK_SQUARE_SQUARE_LEFT);
				AstRef x = parse_expr(parser, 0);
				v = tree_index(parser,tok.line, v, x);
				take_token(parser,TOK_SQUARE_SQUARE_RIGHT);
			}
			break;
			/* todo: make this nil safe, so [0,0] shouldn't
			fail if item at 0 is nil, we should have a separate
			instruction for getfield, which is like getfieldoptional
			or something to avoid having to generate additional code */
			case TOK_SQUARE_LEFT:
			{
				v = parse_indirect_postfix(parser, v);
			}
			break;
			case TOK_COLON:
			{
				consume_token(parser);
				Token name = take_token(parser,TOK_IDENTIFIER);
				AstRef y = create_str_ast(parser, name.site, name.text);
				v = tree_meta_field(parser, tok.site, v, y);
			}
			break;

			case TOK_CURLY_LEFT:
			case TOK_PAREN_LEFT:
			{
				u32 nargs = elf_parse_call_args(parser);
				AstRef *args = pop_ast_array(parser, nargs);
				v = create_call_ast(parser, tok.site, v, args, nargs);
			}
			break;

			default: goto esc;
		}
	}

	esc:;
	return v;
}

static AstRef parse_subexpr(Parser *parser, int flags, int rank)
{
	Token tok = parser->tok;

	AstRef x,y;

	if (peek_tok(parser, TOK_ELLIPSIS))
	{
		x = Y_NULL;
		// empty range
		// ... <y>
		goto parsey;
	}
	else
	{
		x = ELF_ParsePostfixExpr(parser, 0);
		if (x == Y_NULL) goto esc;

		//	if (x->type == NT_NON) {
		//		parser_dialog(parser, x->site, "invalid data type");
		//		goto esc;
		//	}
	}

	for(;;) {
		parsey:

		tok = parser->tok;

		int prio = token_rank_from_type(tok.type);
		if (prio <= rank) goto esc;

		// assign is not an expression, quit now and let
		// the caller handle it
		if (parser->tok_prox.type == TOK_ASSIGN) {
			goto esc;
		}

		consume_token(parser);

		y = parse_subexpr(parser, 0, prio);
		if (tok.type != TOK_ELLIPSIS) {
			//	if (!y || y->type == NT_NON) {
			//		parser_dialog(parser, tok.line, "invalid right operand");
			//		goto esc;
			//	}
		}

		x = create_binary_expr_ast(parser, tok.line, tree_kind_from_token_type(tok.type), x, y);

		if (y == Y_NULL) goto esc;
	}

	esc:
	return x;
}


static AstRef parse_expr(Parser *parser, int flags)
{
	// Todo, what are these checks for?! We shouldn't need this!
	switch (parser->tok.type)
	{
		case TOK_NONE:
		case TOK_FOR:
		case TOK_WHILE:
		case TOK_COMMA:
		case TOK_PAREN_RIGHT: case TOK_CURLY_RIGHT: case TOK_SQUARE_RIGHT:
		{
			return Y_NULL;
		}
		default: ;
	}

	return parse_subexpr(parser, 0, 0);
}

static AstRef parse_expr_stat(Parser *parser)
{
	AstRef x = parse_tuple_expr(parser);
	Token tok = parser->tok;
	switch (parser->tok.type)
	{
		case TOK_BIND:
		case TOK_HARD_BIND:
		{
			consume_token(parser);

			AstRef y = parse_tuple_expr(parser);
			x = elf_new_decl_stat_tree(parser, tok.site, 0, x, y);
		}
		break;
		case TOK_ASSIGN:
		{
			consume_token(parser);

			AstRef y = parse_tuple_expr(parser);
			x = create_assign_ast(parser, tok.site, x, y);
		}
		break;
#if 0
		case TOK_MOD_ASSIGN:
		case TOK_MUL_ASSIGN:
		case TOK_DIV_ASSIGN:
		case TOK_ADD_ASSIGN:
		case TOK_SUB_ASSIGN:
		case TOK_SHL_ASSIGN:
		case TOK_SHR_ASSIGN:
		case TOK_XOR_ASSIGN:
		{
			// Todo,
			AstRef y = parse_tuple_expr(parser);
			x = create_assign_ast(parser, tok.site, x, y);
		}
		break;
#endif
		default:
		{
			// ...
		}
		break;
	}
	return x;
}


static AstRef parse_stat(Parser *parser)
{

	Token tok = parser->tok;
	AstRef tree = 0;

	switch (tok.type)
	{
		case TOK_NONE:
		case TOK_CURLY_RIGHT:
		case TOK_PAREN_RIGHT:
		case TOK_THEN:
		case TOK_ELSE:
		case TOK_ELIF:
		{
		}
		break;

#if 0
		case TOK_M_ASSERT:
		{
			parse_assert(parser);
			success = true;
		}
		break;
#endif

		case TOK_DEFER:
		{
			consume_token(parser);
			tree = parse_stat(parser);
			tree = elf_new_defer_stat_tree(parser, tok.site, tree);
		}
		break;

		case TOK_RET:
		case TOK_HARD_ARROW:
		{
			consume_token(parser);

			AstRef expr = parse_tuple_expr(parser);
			tree = elf_new_return_stat_tree(parser, tok.site, expr);
		}
		break;

		case TOK_BREAK:
		{
			consume_token(parser);
			AstRef expr = 0;
			if (!tok.eol) expr = parse_expr(parser, 0);
			tree = elf_new_break_stat_tree(parser, tok.site, expr);
		}
		break;

		case TOK_CONTINUE:
		{
			consume_token(parser);
			AstRef expr = 0;
			if (!tok.eol) expr = parse_expr(parser, 0);
			tree = elf_new_continue_stat_tree(parser, tok.site, expr);
		}
		break;

		case TOK_CURLY_LEFT:
		{
			u32 nstats = 0;
			take_token(parser, TOK_CURLY_LEFT);
			while (!peek_tok(parser, TOK_NONE) && !peek_tok(parser, TOK_CURLY_RIGHT))
			{
				Ast *stat = parse_stat(parser);
				if (notree(stat)) break;

				push_ast(parser, stat);
				++ nstats;
			}
			take_token(parser, TOK_CURLY_RIGHT);

			AstRef *stats = pop_ast_array(parser, nstats);
			tree = create_block_ast(parser, tok.site, stats, nstats);
		}
		break;

		case TOK_WHILE:
		{
			consume_token(parser);

			AstRef pred = parse_expr(parser, 0);
			if (iserror(pred)) goto _err;

			take_token(parser, TOK_QMARK);

			AstRef body = parse_stat(parser);
			if (iserror(body)) goto _err;

			tree = elf_new_while_stat_tree(parser, tok.site, pred, body);
		}
		break;

		case TOK_FOR:
		{
			consume_token(parser);

			AstRef name = parse_tuple_expr(parser);
			if (pick_token(parser, TOK_BIND)) {
			}
			else take_token(parser, TOK_HARD_BIND);

			AstRef expr = parse_semicolon_expr(parser);
			AstRef decl = elf_new_decl_stat_tree(parser, tok.site, 0, name, expr);
			take_token(parser, TOK_QMARK);
			AstRef body = parse_stat(parser);

			tree = elf_new_for_stat_tree(parser, tok.site, decl, body);
		}
		break;

		case TOK_IF:
		{
			consume_token(parser);
			tree = elf_parse_if_stat(parser);
		}
		break;

		default:
		{
			tree = parse_expr_stat(parser);
		}
		break;
	}

	_err:
	return tree;
}

static AstRef elf_parse_if_stat(Parser *par)
{
	Token tok = par->tok;
	AstRef pred = parse_expr(par, 0);
	if (!pick_token(par, TOK_QMARK)) {
		push_error(par, ERROR_EXPECTED_TOKEN, pred->site, "expected '?' for 'if' statement");
	}

	AstRef true_clause = parse_stat(par);
	AstRef else_clause = 0;

	// Todo, elif != else if
	if (pick_token(par, TOK_ELIF)) {
		else_clause = elf_parse_if_stat(par);
	}
	else if (pick_token(par, TOK_ELSE)) {
		else_clause = parse_stat(par);
	}

	AstRef stat = elf_new_if_stat_tree(par, tok.line, pred, true_clause, else_clause);
	return stat;
}


// Todo, remove this!
static int parse_constexpr(Parser *parser) {
	return -1;
}

#if 0
// Todo, remove this!
static void checkstk(Parser *parser, int state) {
	int index = parser->R->stack_ptr - parser->R->stack;
	if (index != state) {
		parser_dialog(parser, parser->tok.line, "internal error, invalid stack state");
		reporterror(parser->R, NO_BYTE, "internal error, invalid stack state");
	}
}

// Todo, remove this, also make json be part of the AST!
static int parse_constexpr(Parser *parser) {
	Token tok = parser->tok;
	int ret = -1;
	int sign = 1;
	switch (tok.type) {
		case TOK_ADD: case TOK_SUB: {
			sign = (tok.type == TOK_ADD) * 2 - 1;
			consume_token(parser);
			if (peek_tok(parser, TOK_NUMBER)) goto numcase;
			else if (peek_tok(parser, TOK_INTEGER)) goto intcase;
			parser_dialog(parser, parser->tok.line, "expected integer or number after '-'");
			goto errorcase;
		} break;
		case TOK_TRUE: {
			consume_token(parser);
			pushint(parser->R, true);
			ret = 1;
		} break;
		case TOK_FALSE: {
			consume_token(parser);
			pushint(parser->R, false);
			ret = 1;
		} break;
		case TOK_NUMBER: { numcase:
			consume_token(parser);
			pushnum(parser->R, tok.number * sign);
			ret = 1;
		} break;
		case TOK_INTEGER: { intcase:
			consume_token(parser);
			pushint(parser->R, tok.integer * sign);
			ret = 1;
		} break;
		case TOK_NIL: {
			consume_token(parser);
			push_nil(parser->R);
			ret = 1;
		} break;
		case TOK_STRING: {
			consume_token(parser);
			pushtext(parser->R, tok.text);
			ret = 1;
		} break;
		case TOK_CURLY_LEFT: {
			consume_token(parser);

			Tab tab = push_new_table(parser->R);
			int tablestk = stack2index(parser->R) - 1;


			while(!peek_tok(parser,TOK_NONE) && !peek_tok(parser,TOK_CURLY_RIGHT)) {

				// push key (or value)
				// interpret words as raw key names not as entities
				Token tok = parser->tok;
				if (pick_token(parser, TOK_IDENTIFIER)) {
					pushtext(parser->R, tok.text);
				}
				else {
					ret = parse_constexpr(parser);
					// todo: instead attempt to do some error recovery
					if (notree(ret)) goto esc;
				}

				if (pick_token(parser, TOK_ASSIGN)) {

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

				pick_token(parser,TOK_COMMA);
			}
			take_token(parser,TOK_CURLY_RIGHT);

			checkstk(parser, tablestk + 1);
			ret = 1;

			goto esc;

		} break;
		default: {
			errorcase:
			push_nil(parser->R);
			parser_dialog(parser,tok.line,"not a constant expression");
		} break;
	}

	esc:
	return ret;
}

static int parse_json_array(Parser *parser){
	int noerr = true;
	push_new_table(parser->inter);

	take_token(parser, TOK_SQUARE_LEFT);

	if (!pick_token(parser,TOK_SQUARE_RIGHT)) do {

		// todo: attempt to recover and ensure the
		// stack is proper still
		noerr = parse_json_value(parser);
		if (!noerr) goto esc;

		elf_arrayadd(parser->inter);

	} while (pick_token(parser, TOK_COMMA));

	take_token(parser, TOK_SQUARE_RIGHT);

	esc:
	return noerr;
}

static int parse_json_object(Parser *parser) {
	int noerr = true;
	push_new_table(parser->inter);

	take_token(parser,TOK_CURLY_LEFT);
	if (!peek_tok(parser,TOK_CURLY_RIGHT)) do {

		Token tok = take_token(parser, TOK_STRING);
		pushtext(parser->inter, tok.text);

		take_token(parser, TOK_COLON);

		noerr = parse_json_value(parser);
		if (noerr != true) goto esc;

		elf_setfield(parser->inter);
	} while (pick_token(parser,TOK_COMMA));


	take_token(parser,TOK_CURLY_RIGHT);
	esc:
	return noerr;
}

static int parse_json_value(Parser *parser)
{
	int success = true;

	Token tok = parser->tok;
	switch (tok.type) {
		case TOK_STRING: {
			consume_token(parser);
			pushtext(parser->inter, tok.text);
		} break;
		case TOK_INTEGER: {
			consume_token(parser);
			pushint(parser->inter, tok.integer);
		} break;
		case TOK_NUMBER: {
			consume_token(parser);
			pushnum(parser->inter, tok.number);
		} break;
		case TOK_CURLY_LEFT: {
			parse_json_object(parser);
		} break;
		case TOK_SQUARE_LEFT: {
			parse_json_array(parser);
		} break;
		default: {
			push_nil(parser->inter);
			parser_dialog(parser, tok.line, "invalid json value");
			success = false;
		} break;
	}
	return success;
}
#endif