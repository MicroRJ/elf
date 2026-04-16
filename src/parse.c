//
//	See Copyright Notice In elf.h
//
// Todo, make global declarations explicit!
// Todo, remove dynamic array usage!
//

#define tok2s(type) (static__str_from_token_type[type])


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static AstRef parse_unary_expr(Parser *parser);
static AstRef parse_expr(Parser *parser, int flags);
static AstRef parse_subexpr(Parser *parser, int flags, int rank);
static AstRef parse_postfix_expr(Parser *parser, int flags);
static AstRef parse_table_expr(Parser *parser);
static AstRef parse_ident_expr(Parser *par);
static AstRef parse_stat(Parser *parser);
static u32 elf_parse_expr_list(Parser *parser);
static AstRef elf_parse_if_stat(Parser *parser);
static bool parse_for(Parser *parser);
static u32 elf_parse_call_args(Parser *par);


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static AstType binary_ast_expr_type_from_token_type(TokenType tok)
{
	switch (tok)
	{
		case TOK_ELLIPSIS:  return AST_RANGE;
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
		case TOK_GT:        return AST_GREATER_THAN;
		case TOK_GTEQ:      return AST_GREATER_THAN_EQ;
		case TOK_LT:        return AST_LESS_THAN;
		case TOK_LTEQ:      return AST_LESS_THAN_EQ;
		case TOK_SHL:       return AST_SHIFT_LEFT;
		case TOK_SHR:       return AST_SHIFT_RIGHT;
		case TOK_BIT_XOR:   return AST_BITWISE_XOR;
		case TOK_BIT_OR:    return AST_BITWISE_OR;
		case TOK_BIT_AND:   return AST_BITWISE_AND;
		case TOK_COMMA:     return AST_COMMA_EXPR;
		default:            return AST_NONE;
	}
}

static AstType precedence_from_binary_ast_expr_type(AstType type)
{
	switch (type)
	{
		case AST_POW:             return 13 + 1;
		case AST_MUL:
		case AST_DIV:
		case AST_MOD:             return 12 + 1;
		case AST_ADD:
		case AST_SUB:             return 11 + 1;
		case AST_SHIFT_LEFT:
		case AST_SHIFT_RIGHT:     return 10 + 1;
		case AST_LESS_THAN:
		case AST_LESS_THAN_EQ:
		case AST_GREATER_THAN:
		case AST_GREATER_THAN_EQ: return  9 + 1;
		case AST_EQ:
		case AST_NOT_EQ:          return  8 + 1;
		case AST_BITWISE_AND:     return  7 + 1;
		case AST_BITWISE_XOR:     return  6 + 1;
		case AST_BITWISE_OR:      return  5 + 1;
		case AST_AND:
		case AST_NIL_AND:         return  4 + 1;
		case AST_OR:
		case AST_NIL_OR:          return  3 + 1;
		case AST_RANGE:           return  2 + 1;

		case AST_COMMA_EXPR:      return  1 + 1;

		case AST_ADD_ASSIGN:      return  1;
		case AST_SUB_ASSIGN:      return  1;
		case AST_MUL_ASSIGN:      return  1;
		case AST_DIV_ASSIGN:      return  1;
		case AST_MOD_ASSIGN:      return  1;
		case AST_XOR_ASSIGN:      return  1;
		case AST_SHL_ASSIGN:      return  1;
		case AST_SHR_ASSIGN:      return  1;
		case AST_NIL_ASSIGN:      return  1;
		case AST_ASSIGN:          return  1;

		default:                  return  0;
	}
}

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

#define push_error(par, err, line, fmt, ...) push_parse_error_(par, SEVERITY_FATAL, err, line, fmt, __VA_ARGS__)
#define push_warning(par, err, line, fmt, ...) push_parse_error_(par, SEVERITY_WARNING, err, line, fmt, __VA_ARGS__)
#define push_note(par, err, line, fmt, ...) push_parse_error_(par, SEVERITY_NOTE, err, line, fmt, __VA_ARGS__)

static void push_parse_error_(Parser *par, Severity severity, Error error, Source line, const char *format, ...)
{
	if (!line) line = par->tok.site;

	va_list vargs;
	va_start(vargs, format);
	char *message = temporary_format_v(format, vargs);
	va_end(vargs);

	parser_dialog(par, line, message);
	if (severity >= SEVERITY_FATAL) {
		reporterror(par->state, -1, message);
	}
}

static void warning_excess_rvalue(Parser *parser, AstRef v)
{
	push_error(parser, ERROR_EXCESS_RVALUE, v->site, "excess rvalue");
}

static void push_error_unexpected_token(Parser *parser, Token tok)
{
	push_error(parser, ERROR_UNEXPECTED_TOKEN, tok.site, "unexpected token");
}

static void push_error_expected_token(Parser *parser, Token tok, int expected)
{
	const char *message = tpf("got token: '%s', expected: '%s'", tok2s(tok.type), tok2s(expected));

	push_error(parser, ERROR_EXPECTED_TOKEN, tok.site, message);
}

static void push_error_expected_token_pair(Parser *parser, Token tok, int expected, Token pair)
{
	const char *message = tpf("got token: '%s', expected: '%s'", tok2s(tok.type), tok2s(expected));

	// todo: push note
	parser_dialog(parser, pair.site, "note: pair started here");

	push_error(parser, ERROR_EXPECTED_TOKEN, tok.site, message);
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
	u32 ast_stack_size = 4096;
	Parser *parser = elf_arena_push_zero(arena, sizeof(*parser));
	parser->state = state;
	parser->arena = arena;

	parser->ast_stack = elf_arena_push_zero(arena, sizeof(*parser->ast_stack) * ast_stack_size);
	parser->ast_stack_size = ast_stack_size;
	parser->ast_stack_index = 0;
	return parser;
}

static void elf_reposition_parser(Parser *parser, char *cursor);

static Parser *elf_create_parser(elf_State *state, elf_Arena *arena, const char *name, const char *data)
{
	ASSERT(state != 0);
	ASSERT(name != 0);
	ASSERT(data != 0);
	Parser *parser = elf_alloc_parser(state, arena);
	parser->name = elf_arena_push_string_data_copy(arena, 0, name);
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

static void elf_reposition_parser(Parser *par, char *cursor)
{
	par->cursor = cursor;
	consume_token(par);
	consume_token(par);
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

static Token take_token(Parser *par, TokenType type) {
	Token tok = par->tok;

	if (!pick_token(par, type)) {
		push_error_expected_token(par, par->tok, type);
	}

	return tok;
}

static inline bool peek_prox_tok(Parser *par, TokenType type) {
	return par->tok_prox.type == type;
}

// static bool peek_tok_inl(Parser *par, TokenType type) {
// 	return peek_tok(par, type) && par->tok_prev.eol != 1;
// }

// static bool pick_tok_inl(Parser *par, TokenType type) {
// 	return peek_tok_inl(par, type) && (consume_token(par), 1);
// }


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void push_ast(Parser *par, AstRef tree)
{
	ASSERT(par->ast_stack_index < par->ast_stack_size);
	par->ast_stack[par->ast_stack_index ++] = tree;
}

// Todo, instead return an AstArray directly ...
static AstRef *pop_ast_array(Parser *par, u32 nargs)
{
	ASSERT(par->ast_stack_index >= nargs);
	par->ast_stack_index -= nargs;
	AstRef *copy = elf_arena_push_copy(par->arena, sizeof(*copy) * nargs, par->ast_stack + par->ast_stack_index);
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
		if (check_error_ast(stat)) break;

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

static AstRef parse_function(Parser *par)
{
	Token tok = par->tok;

	// Todo, figure this out ...
	if (pick_token(par, TOK_FUN) || pick_token(par, TOK_FUNCTION)) {
	}
	else NO_CODE;

	u32 nparams = 0;
	take_token(par, TOK_LEFT_PAREN);
	if (!peek_tok(par, TOK_PAREN_RIGHT)) do
	{
		Token tok = par->tok;
		if (pick_token(par, TOK_ELLIPSIS))
		{
			AstRef param = create_ellipsis_ast(par, tok.site);
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
	while (peek_tok(par, TOK_SEMICOLON))
	{
		Token tok = consume_token(par);
		AstRef y = parse_tuple_expr(par);
		x = create_for_loop_expr_ast(par, tok.site, x, y);
	}
	return x;
}

// Todo, deprecate?!
//
// 'load' ( <file-name> )
//
//  elf.load_file(<file-name>)
//
static AstRef parse_load_expr(Parser *par)
{
	Token tok = take_token(par, TOK_LOAD);

	u32 nargs = elf_parse_call_args(par);
	AstRef *args = pop_ast_array(par, nargs);

	AstRef v = tree_callcoreapi(par, tok.site, BUILTIN_LOADFILE, args, nargs);
	return v;
}

//
// 'new' <meta-table> ( <argument-list> )
//
//	::=
//
// --> elf.set_meta({},Vector2):__new(x,y)
//
static AstRef parse_new_expr(Parser *par)
{
	Token tok = take_token(par, TOK_NEW);

	AstRef call_expr = parse_postfix_expr(par, 0);
	// Todo, proper error handling!
	if (call_expr->kind != AST_CALL) {
		parser_dialog(par, tok.site, "invalid expression");
	}
	// Todo, just have a bultin for doing this!
	AstRef meta = call_expr->ast_call_expr.expr;
	AstRef *args = call_expr->ast_call_expr.args;
	u32 nargs = call_expr->ast_call_expr.nargs;

	AstRef table = elf_new_table_expr_tree(par, tok.site, 0, 0);
	table = tree_callcoreapi2(par, tok.site, BUILTIN_SETMETA, table, meta);
	AstRef v = create_meta_call_ast(par, meta->site, table, "__new", args, nargs);
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

static AstRef parse_unary_expr(Parser *parser)
{
	Token tok = parser->tok;

	AstRef v = 0;
	switch (tok.type)
	{
		case TOK_ELLIPSIS:
		{
			consume_token(parser);
			v = create_ellipsis_ast(parser, tok.site);
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
			v = create_nil_ast(parser, tok.site);
		}
		break;
		case TOK_TRUE:
		{
			consume_token(parser);
			v = create_int_ast(parser, tok.site,1);
		}
		break;
		case TOK_FALSE:
		{
			consume_token(parser);
			v = create_int_ast(parser, tok.site,0);
		}
		break;
		case TOK_LETTER:
		case TOK_INTEGER:
		{
			consume_token(parser);
			v = create_int_ast(parser, tok.site,tok.integer);
		}
		break;
		case TOK_NUMBER:
		{
			consume_token(parser);
			v = create_num_ast(parser, tok.site, tok.number);
		}
		break;
		case TOK_STRING:
		{
			consume_token(parser);
			v = create_str_ast(parser, tok.site, tok.text);
		}
		break;
		case TOK_TILDE:
		{
			consume_token(parser);
			v = parse_subexpr(parser, 0, 10000);
			v = create_unary_expr_ast(parser, tok.site, AST_BITWISE_NOT, v);
		}
		break;
		case TOK_SUB:
		{
			consume_token(parser);
			// Todo, dedicated ast
			v = parse_subexpr(parser, 0, 10000);
			v = create_binary_expr_ast(parser, tok.site, AST_SUB, create_int_ast(parser, tok.site, 0), v);
		}
		break;
		case TOK_ADD:
		{
			consume_token(parser);
			v = parse_subexpr(parser, 0, 10000);
		}
		break;
		case TOK_NEW:
		{
			v = parse_new_expr(parser);
		}
		break;
		case TOK_LOAD:
		{
			v = parse_load_expr(parser);
		}
		break;
		case TOK_LEFT_BRACE:
		{
			v = parse_table_expr(parser);
		}
		break;

		case TOK_LEFT_PAREN:
		{
			consume_token(parser);
			if (!peek_tok(parser, TOK_PAREN_RIGHT))
			{
				v = parse_expr(parser,0);
			}
			take_token(parser, TOK_PAREN_RIGHT);
		}
		break;
		case TOK_FUNCTION:
		case TOK_FUN:
		{
			v = parse_function(parser);
		}
		break;

		default: ;
	}

	esc:
	return v;
}

// Todo, multi-stores
static AstRef parse_table_expr(Parser *par)
{
	Token tok = take_token(par, TOK_LEFT_BRACE);
	Token table_tok = tok;

	AstRef x, y;

	u32 nargs = 0;
	while (!peek_tok(par, TOK_NONE) && !peek_tok(par, TOK_RIGHT_BRACE))
	{
		x = Y_NULL, y = Y_NULL;

		tok = par->tok;
		if (peek_tok(par, TOK_IDENTIFIER) && peek_prox_tok(par, TOK_ASSIGN))
		{
			take_token(par, TOK_IDENTIFIER);

			x = create_str_ast(par, tok.site, tok.text);
			if (check_error_ast(x)) goto _err_ii;

			take_token(par, TOK_ASSIGN);

			y = parse_expr(par, 0);
			if (check_error_ast(y)) goto _err_ii;

			check_tree(par, x->site, x);
			check_tree(par, y->site, y);
		}
		else if (pick_token(par, TOK_SQUARE_LEFT))
		{
			x = parse_expr(par,0);
			if (check_error_ast(x)) goto _err_ii;

			take_token(par, TOK_SQUARE_RIGHT);

			take_token(par, TOK_ASSIGN);

			y = parse_expr(par, 0);
			if (check_error_ast(y)) goto _err_ii;
		}
		else
		{
			x = y = parse_expr(par,0);
			if (check_error_ast(x)) goto _err_ii;

			if (pick_token(par, TOK_ASSIGN))
			{
				y = parse_expr(par, 0);
				if (check_error_ast(y)) goto _err_ii;
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

	take_token(par, TOK_RIGHT_BRACE);

	AstRef *args = pop_ast_array(par, nargs);
	AstRef table = elf_new_table_expr_tree(par, table_tok.site, args, nargs);
	return table;

	_err:
	return Y_NULL;

	_err_ii:
	push_error_invalid_field_initializer(par, tok.site, y);
	return Y_NULL;
}

static u32 elf_parse_call_args(Parser *par)
{
	u32 nargs = 0;

	if (peek_tok(par, TOK_LEFT_BRACE))
	{
		AstRef expr = parse_table_expr(par);
		push_ast(par, expr);
	}
	else if (pick_token(par, TOK_LEFT_PAREN))
	{
		pick_token(par, TOK_COMMA);

		if (!peek_tok(par, TOK_PAREN_RIGHT)) do
		{

			if (peek_tok(par, TOK_PAREN_RIGHT))
			{
				push_error(par, ERROR_EXCESS_COMMA, par->tok_prev.site, "excess comma");
			}
			if (peek_tok(par, TOK_COMMA))
			{
				push_error(par, ERROR_EXCESS_COMMA, 0, "excess comma");
			}

			AstRef x = parse_expr(par, 0);
			if (check_error_ast(x)) goto esc;

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
		if (check_error_ast(expr)) goto esc;
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
	if (pick_token(par, TOK_LEFT_PAREN))
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
	else if (pick_token(par, TOK_LEFT_BRACE))
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
			//		v = elf_new_field_expr_tree(par, tok.site, v, i);
			//		if (iserror(v)) goto _err;
			//	}
		}
		else if (x->kind == AST_RANGE)
		{
			v = tree_ranged_index(par,tok.site, v, x);
			if (iserror(v)) goto _err;
		}
		else
		{
			v = elf_new_field_expr_tree(par,tok.site, v, x);
			if (iserror(v)) goto _err;
		}
	}
	while(pick_token(par,TOK_COMMA));

	_err:
	// todo: attempt to get to the other ']' in case of failure
	take_token(par,TOK_SQUARE_RIGHT);
	return v;
}

static AstRef parse_postfix_expr(Parser *parser, int flags)
{

	Token tok = parser->tok;

	AstRef v = parse_unary_expr(parser);
	if (check_error_ast(v)) goto esc;

	// Todo, instead, just check the line numbers dude ...
	while (parser->tok.type != TOK_NONE && !parser->tok_prev.eol)
	{
		tok = parser->tok;

		switch (tok.type) {

			case TOK_DOT:
			{
				v = parse_field_postfix(parser, v);
			}
			break;
			//	case TOK_SQUARE_SQUARE_LEFT:
			//	{
			//		take_token(parser,TOK_SQUARE_SQUARE_LEFT);
			//		AstRef x = parse_expr(parser, 0);
			//		v = tree_index(parser, tok.site, v, x);
			//		take_token(parser,TOK_SQUARE_SQUARE_RIGHT);
			//	}
			//	break;
			case TOK_SQUARE_LEFT:
			{
				v = parse_indirect_postfix(parser, v);
			}
			break;
			case TOK_COLON:
			{
				consume_token(parser);
				Token name = take_token(parser, TOK_IDENTIFIER);
				AstRef y = create_str_ast(parser, name.site, name.text);
				v = create_meta_field_ast(parser, tok.site, v, y);
			}
			break;
			case TOK_LEFT_BRACE:
			case TOK_LEFT_PAREN:
			{
				u32 nargs = elf_parse_call_args(parser);
				AstRef *args = pop_ast_array(parser, nargs);
				v = create_call_ast(parser, tok.site, v, args, nargs);
			}
			break;
			default: goto esc;
		}
	}

	esc:
	return v;
}

static AstRef parse_subexpr(Parser *parser, int flags, int upper_precedence)
{
	Token tok = parser->tok;

	AstRef x = parse_postfix_expr(parser, 0);
	if (check_error_ast(x)) goto esc;

	for(;;)
	{
		tok = parser->tok;

		AstType ast_type = binary_ast_expr_type_from_token_type(tok.type);
		if (ast_type == AST_NONE) break;

		u32 inner_precedence = precedence_from_binary_ast_expr_type(ast_type);
		if (inner_precedence <= upper_precedence) break;

		consume_token(parser);

		AstRef y = parse_subexpr(parser, 0, inner_precedence);
		x = create_binary_expr_ast(parser, tok.site, ast_type, x, y);
		if (check_error_ast(y)) break;
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
		case TOK_PAREN_RIGHT:
		case TOK_RIGHT_BRACE:

		case TOK_SQUARE_RIGHT:
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
		case TOK_RIGHT_BRACE:
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
		case TOK_LONG_ARROW:
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

		case TOK_LEFT_BRACE:
		{
			u32 nstats = 0;
			take_token(parser, TOK_LEFT_BRACE);
			while (!peek_tok(parser, TOK_NONE) && !peek_tok(parser, TOK_RIGHT_BRACE))
			{
				Ast *stat = parse_stat(parser);
				if (check_error_ast(stat)) break;

				push_ast(parser, stat);
				++ nstats;
			}
			take_token(parser, TOK_RIGHT_BRACE);

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

			tree = create_while_ast(parser, tok.site, pred, body);
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

	AstRef stat = elf_new_if_stat_tree(par, tok.site, pred, true_clause, else_clause);
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
		parser_dialog(parser, parser->tok.site, "internal error, invalid stack state");
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
			parser_dialog(parser, parser->tok.site, "expected integer or number after '-'");
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
		case TOK_LEFT_BRACE: {
			consume_token(parser);

			Tab tab = push_new_table(parser->R);
			int tablestk = stack2index(parser->R) - 1;


			while(!peek_tok(parser,TOK_NONE) && !peek_tok(parser,TOK_RIGHT_BRACE)) {

				// push key (or value)
				// interpret words as raw key names not as entities
				Token tok = parser->tok;
				if (pick_token(parser, TOK_IDENTIFIER)) {
					pushtext(parser->R, tok.text);
				}
				else {
					ret = parse_constexpr(parser);
					// todo: instead attempt to do some error recovery
					if (check_error_ast(ret)) goto esc;
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
			take_token(parser,TOK_RIGHT_BRACE);

			checkstk(parser, tablestk + 1);
			ret = 1;

			goto esc;

		} break;
		default: {
			errorcase:
			push_nil(parser->R);
			parser_dialog(parser, tok.site,"not a constant expression");
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

	take_token(parser,TOK_LEFT_BRACE);
	if (!peek_tok(parser,TOK_RIGHT_BRACE)) do {

		Token tok = take_token(parser, TOK_STRING);
		pushtext(parser->inter, tok.text);

		take_token(parser, TOK_COLON);

		noerr = parse_json_value(parser);
		if (noerr != true) goto esc;

		elf_setfield(parser->inter);
	} while (pick_token(parser,TOK_COMMA));


	take_token(parser,TOK_RIGHT_BRACE);
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
		case TOK_LEFT_BRACE: {
			parse_json_object(parser);
		} break;
		case TOK_SQUARE_LEFT: {
			parse_json_array(parser);
		} break;
		default: {
			push_nil(parser->inter);
			parser_dialog(parser, tok.site, "invalid json value");
			success = false;
		} break;
	}
	return success;
}
#endif