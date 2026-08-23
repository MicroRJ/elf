//
//	See Copyright Notice In elf.h
//


#define I64_MAX_MAGNITUDE 0x7fffffffffffffffull
#define I64_MIN_MAGNITUDE 0x8000000000000000ull

static Ast parse_unary_expr(Parser *parser);
static Ast parse_expr(Parser *parser);
static Ast parse_subexpr(Parser *parser, u32 rank);
static Ast parse_postfix_expr(Parser *parser);
static Ast parse_table_expr(Parser *parser);
static Ast parse_ident_expr(Parser *parser);
static Ast parse_stat(Parser *parser);
static Ast parse_if_stat(Parser *parser);
static Ast parse_json_ast(Parser *parser);
static Ast parse_json_ast_value(Parser *parser);
static u32 parse_call_args(Parser *parser);


static AstType binary_ast_expr_type_from_token_type(TokenType type)
{
	switch (type)
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
		default:            return AST_NONE;
	}
}

static AstType compound_assign_ast_type_from_token_type(TokenType type)
{
	switch (type)
	{
		case TOK_ADD_ASSIGN: return AST_ADD_ASSIGN;
		case TOK_SUB_ASSIGN: return AST_SUB_ASSIGN;
		case TOK_MUL_ASSIGN: return AST_MUL_ASSIGN;
		case TOK_DIV_ASSIGN: return AST_DIV_ASSIGN;
		case TOK_MOD_ASSIGN: return AST_MOD_ASSIGN;
		case TOK_XOR_ASSIGN: return AST_XOR_ASSIGN;
		case TOK_SHL_ASSIGN: return AST_SHL_ASSIGN;
		case TOK_SHR_ASSIGN: return AST_SHR_ASSIGN;
		default:             return AST_NONE;
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
		default:                  return  0;
	}
}

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

static void parser_report(Parser *parser, Severity severity, Error error, SourceSite site, const char *message)
{
	(void)error;

	if (!site.data) {
		site = parser->tok.site;
	}

	const char *source_name = parser && parser->name ? string_data(parser->name) : "<unknown>";
	const char *severity_name = severity >= SEVERITY_FATAL ? "error" :
	severity == SEVERITY_WARNING ? "warning" : "note";

	if (site.line_index) {
		log_linef(LOG_LEVEL_ERROR, "%s [%u:%llu] parser %s: %s"
		,	source_name
		,	site.line_index
		,	source_slice_column(site)
		,	severity_name
		,	message);
	}
	else {
		log_linef(LOG_LEVEL_ERROR, "%s [?] parser %s: %s", source_name, severity_name, message);
	}

	print_source_slice_marker(site, parser->lexer.source);
	if (severity >= SEVERITY_FATAL)
	{
		parser->failed = true;
		elf_Status status = parser->error_status != ELF_STATUS_OK ? parser->error_status : ELF_STATUS_PARSE_ERROR;
		elf_diagnostic_set(parser->state, status, source_name, site, message);
	}
}

static void parser_error(Parser *parser, Error error, SourceSite site, const char *message)
{
	parser_report(parser, SEVERITY_FATAL, error, site, message);
}

static void parser_errorf(Parser *parser, Error error, SourceSite site, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	elf_Scratch scratch = elf_begin_scratch();
	char *message = elf_arena_pushfv(scratch.arena, format, args);
	va_end(args);
	elf_arena_push_zero(scratch.arena, 1);

	parser_error(parser, error, site, message);
	elf_end_scratch(scratch);
}

static void parser_unexpected_token(Parser *parser, Token token)
{
	parser_errorf(parser, ERROR_UNEXPECTED_TOKEN, token.site
	,	"unexpected token '%s'", token_type_name(token.type));
}

static void parser_expected_token(Parser *parser, Token token, TokenType expected)
{
	parser_errorf(parser, ERROR_EXPECTED_TOKEN, token.site
	,	"expected '%s', got '%s'", token_type_name(expected), token_type_name(token.type));
}

static void parser_invalid_table_entry(Parser *parser, SourceSite site)
{
	parser_error(parser, ERROR_INVALID_FIELD_INITIALIZER, site, "invalid table entry");
}

static b32 ast_is_missing_or_error(Ast ast)
{
	return !ast || ast_is_error(ast);
}

static Parser *elf_alloc_parser(elf_State *state, elf_Arena *arena)
{
	Parser *parser = elf_arena_push_zero(arena, sizeof(*parser));
	parser->state = state;
	parser->arena = arena;

	parser->ast = create_ast_context(arena);
	return parser;
}

static void reposition_parser(Parser *parser, char *cursor);

static void init_parser_atoms(Parser *parser)
{
	atom_table_init(&parser->atoms, parser->arena);

#define INTERN_KEYWORD_ATOM(NAME, TEXT) atom_from_data_id(&parser->atoms, TEXT, XFUSE(TOK_, NAME));
	KEYWORD_DEFINITIONS(INTERN_KEYWORD_ATOM)
#undef INTERN_KEYWORD_ATOM

#define INTERN_MACRO_ATOM(NAME, TEXT) atom_from_data_id(&parser->atoms, "#" TEXT, XFUSE(TOK_, NAME));
	MACRO_DEFINITIONS(INTERN_MACRO_ATOM)
#undef INTERN_MACRO_ATOM
}

static Parser *elf_create_parser(elf_State *state, elf_Arena *arena, const char *name, elf_StrSlice source)
{
	ASSERT(state != 0);
	ASSERT(name != 0);
	ASSERT(source.data != 0);
	Parser *parser = elf_alloc_parser(state, arena);
	parser->name = elf_string_from_data(state, name);
	init_parser_atoms(parser);
	parser->error_status = ELF_STATUS_PARSE_ERROR;
	lexer_init(&parser->lexer, state, parser->name, &parser->atoms, source);
	reposition_parser(parser, source.data);
	return parser;
}

static Token consume_token(Parser *par)
{
	Token token = lex_token(&par->lexer);
	par->tok_prev = par->tok;
	par->tok = par->tok_prox;
	par->tok_prox = token;
	return par->tok_prev;
}

static void reposition_parser(Parser *par, char *cursor)
{
	par->lexer.cursor = cursor;
	par->lexer.line_index = 1;
	par->lexer.line_start = cursor;
	consume_token(par);
	consume_token(par);
}

static inline b32 peek_token(Parser *parser, TokenType type)
{
	return parser->tok.type == type;
}

static inline b32 pick_token(Parser *parser, TokenType type)
{
	if (peek_token(parser, type)) {
		consume_token(parser);
		return true;
	}
	return false;
}

static Token take_token(Parser *parser, TokenType type)
{
	Token tok = parser->tok;

	if (!pick_token(parser, type)) {
		parser_expected_token(parser, parser->tok, type);
	}

	return tok;
}

static inline b32 peek_next_token(Parser *parser, TokenType type)
{
	return parser->tok_prox.type == type;
}


static void push_ast(Parser *par, Ast tree)
{
	ASSERT(par->ast.stack_index < par->ast.stack_size);
	par->ast.stack[par->ast.stack_index ++] = tree;
}

// Todo, instead return an AstArray directly ...
static Ast *pop_ast_array(Parser *par, u32 nargs)
{
	ASSERT(par->ast.stack_index >= nargs);
	par->ast.stack_index -= nargs;
	Ast *copy = elf_arena_push_copy(par->ast.arena, sizeof(*copy) * nargs, par->ast.stack + par->ast.stack_index);
	return copy;
}

static Ast elf_parse_file(Parser *parser)
{
	if (parser_has_failed(parser)) return ERROR_AST;
	Token tok = parser->tok;
	u32 nstats = 0;
	while (!peek_token(parser, TOK_NONE))
	{
		Token before = parser->tok;
		Ast_T *stat = parse_stat(parser);
		if (parser_has_failed(parser)) return ERROR_AST;
		if (ast_is_error(stat)) {
			return ERROR_AST;
		}
		if (!stat)
		{
			parser_unexpected_token(parser, before);
			return ERROR_AST;
		}
		if (before.site.data == parser->tok.site.data && before.type == parser->tok.type)
		{
			parser_error(parser, ERROR_INVALID_STATEMENT, before.site, "parser made no progress while reading statement");
			return ERROR_AST;
		}

		push_ast(parser, stat);
		++ nstats;
	}

	Ast *stats = pop_ast_array(parser, nstats);
	Ast body = create_block_ast(parser, tok.site, stats, nstats);

	Ast file_ast = create_file_ast(parser, tok.site, body);
	return file_ast;
}

static Ast parse_function(Parser *par)
{
	Token tok = par->tok;

	take_token(par, TOK_FUN);

	u32 nparams = 0;
	Ast variadic = 0;
	take_token(par, TOK_LEFT_PAREN);
	if (!peek_token(par, TOK_PAREN_RIGHT)) do
	{
		Token tok = par->tok;

		if (pick_token(par, TOK_ELLIPSIS))
		{
			variadic = create_ellipsis_ast(par, tok.site);
			if (pick_token(par, TOK_COMMA))
			{
				parser_error(par, ERROR_INVALID_EXPRESSION, tok.site, "variadic marker must be the final parameter");
				continue;
			}
			break;
		}
		else
		{
			Ast name = parse_ident_expr(par);
			Ast type = 0;
			Ast expr = 0;
			if (pick_token(par, TOK_COLON))
			{
				type = parse_expr(par);
			}
			if (pick_token(par, TOK_ASSIGN))
			{
				expr = parse_expr(par);
			}
			Ast param = create_param_ast(par, tok.site, name, type, expr);
			push_ast(par, param);
			++ nparams;
		}

	} while(pick_token(par, TOK_COMMA));

	take_token(par, TOK_PAREN_RIGHT);

	Ast *params = pop_ast_array(par, nparams);
	Ast body = parse_stat(par);
	if (ast_is_missing_or_error(body))
	{
		parser_error(par, ERROR_MISSING_FUNCTION_BODY, par->tok.site, "expected function body");
		return ERROR_AST;
	}

	Ast function = create_function_ast(par, tok.site, params, nparams, variadic, body);
	return function;
}

static b32 token_ends_expression(TokenType type)
{
	switch (type)
	{
		case TOK_NONE:
		case TOK_QMARK:
		case TOK_COMMA:
		case TOK_SEMICOLON:
		case TOK_PAREN_RIGHT:
		case TOK_RIGHT_BRACE:
		case TOK_SQUARE_RIGHT:
		case TOK_THEN:
		case TOK_ELSE:
		case TOK_ELIF:
		{
			return true;
		}
		default:
		{
			return false;
		}
	}
}

static Ast parse_tuple_expr(Parser *par)
{
	Token tok = par->tok;
	u32 nargs = 0;
	do
	{
		Ast expr = parse_expr(par);
		if (ast_is_error(expr)) {
			return ERROR_AST;
		}
		push_ast(par, expr);
		++ nargs;
	}
	while (pick_token(par, TOK_COMMA));

	Ast tuple = 0;
	if (nargs)
	{
		Ast *args = pop_ast_array(par, nargs);
		tuple = create_tuple_ast(par, tok.site, args, nargs);
	}
	return tuple;
}

static Ast parse_optional_tuple_expr(Parser *par)
{
	if (token_ends_expression(par->tok.type)) {
		return NULL_AST;
	}
	return parse_tuple_expr(par);
}

static Ast parse_for_identifier_tuple(Parser *parser)
{
	Token start = parser->tok;
	u32 stack_start = parser->ast.stack_index;
	u32 nargs = 0;

	do
	{
		if (!peek_token(parser, TOK_IDENTIFIER))
		{
			parser_expected_token(parser, parser->tok, TOK_IDENTIFIER);
			parser->ast.stack_index = stack_start;
			return ERROR_AST;
		}

		Ast name = parse_ident_expr(parser);
		push_ast(parser, name);
		++ nargs;
	}
	while (pick_token(parser, TOK_COMMA));

	Ast *names = pop_ast_array(parser, nargs);
	return create_tuple_ast(parser, start.site, names, nargs);
}

static Ast parse_get_mem_expr(Parser *parser)
{
	Token tok = take_token(parser, TOK_M_GET_MEM);

	take_token(parser, TOK_LEFT_PAREN);
	Ast expr = parse_expr(parser);
	take_token(parser, TOK_PAREN_RIGHT);

	return create_get_mem_ast(parser, tok.site, expr);
}

static Ast parse_ident_expr(Parser *par)
{
	Token tok = take_token(par, TOK_IDENTIFIER);
	return create_ident_ast(par, tok.site, tok.atom);
}

static Ast parse_positive_integer_literal(Parser *parser, Token token)
{
	if (token.integer_magnitude > I64_MAX_MAGNITUDE)
	{
		parser_errorf(parser, ERROR_INVALID_EXPRESSION, token.site
		,	"integer literal out of range for i64: %llu"
		,	token.integer_magnitude);
		return ERROR_AST;
	}

	return create_int_ast(parser, token.site, (i64)token.integer_magnitude);
}

static Ast parse_negative_integer_literal(Parser *parser, Token sign, Token token)
{
	if (token.integer_magnitude > I64_MIN_MAGNITUDE)
	{
		parser_errorf(parser, ERROR_INVALID_EXPRESSION, token.site
		,	"integer literal out of range for i64: -%llu"
		,	token.integer_magnitude);
		return ERROR_AST;
	}

	i64 value = 0;
	if (token.integer_magnitude == I64_MIN_MAGNITUDE) {
		value = -9223372036854775807LL - 1LL;
	}
	else {
		value = -(i64)token.integer_magnitude;
	}

	return create_int_ast(parser, sign.site, value);
}

static Ast parse_interpolated_string(Parser *parser)
{
	Token start = take_token(parser, TOK_STRING_START);
	u32 stack_start = parser->ast.stack_index;
	u32 nparts = 0;

	push_ast(parser, create_atom_ast(parser, start.site, start.atom));
	nparts += 1;

	for (;;)
	{
		Ast expr = parse_expr(parser);
		if (ast_is_missing_or_error(expr))
		{
			parser_error(parser, ERROR_INVALID_EXPRESSION, parser->tok.site,
			"expected expression inside interpolated string");
			parser->ast.stack_index = stack_start;
			return ERROR_AST;
		}
		push_ast(parser, expr);
		nparts += 1;

		Token part = parser->tok;
		if (part.type != TOK_STRING_PART && part.type != TOK_STRING_END)
		{
			parser_error(parser, ERROR_EXPECTED_TOKEN, part.site,
			"expected the end of an interpolated expression");
			parser->ast.stack_index = stack_start;
			return ERROR_AST;
		}

		consume_token(parser);
		push_ast(parser, create_atom_ast(parser, part.site, part.atom));
		nparts += 1;

		if (part.type == TOK_STRING_END) {
			break;
		}
	}

	Ast *parts = pop_ast_array(parser, nparts);
	return create_interpolated_string_ast(parser, start.site, parts, nparts);
}

static Ast parse_unary_expr(Parser *parser)
{
	Token tok = parser->tok;

	Ast value = 0;
	switch (tok.type)
	{
		case TOK_ELLIPSIS:
		{
			consume_token(parser);
			value = create_ellipsis_ast(parser, tok.site);
		}
		break;
		case TOK_IDENTIFIER:
		{
			value = parse_ident_expr(parser);
		}
		break;
		case TOK_NIL:
		{
			consume_token(parser);
			value = create_nil_ast(parser, tok.site);
		}
		break;
		case TOK_TRUE:
		{
			consume_token(parser);
			value = create_int_ast(parser, tok.site, 1);
		}
		break;
		case TOK_FALSE:
		{
			consume_token(parser);
			value = create_int_ast(parser, tok.site, 0);
		}
		break;
		case TOK_LETTER:
		case TOK_INTEGER:
		{
			consume_token(parser);
			value = parse_positive_integer_literal(parser, tok);
		}
		break;
		case TOK_NUMBER:
		{
			consume_token(parser);
			value = create_num_ast(parser, tok.site, tok.number);
		}
		break;
		case TOK_STRING:
		{
			consume_token(parser);
			value = create_atom_ast(parser, tok.site, tok.atom);
		}
		break;
		case TOK_STRING_START:
		{
			value = parse_interpolated_string(parser);
		}
		break;
		case TOK_TILDE:
		{
			consume_token(parser);
			value = parse_subexpr(parser, 10000);
			value = create_unary_ast(parser, tok.site, AST_BITWISE_NOT, value);
		}
		break;
		case TOK_SUB:
		{
			consume_token(parser);
			if (peek_token(parser, TOK_INTEGER))
			{
				Token integer = consume_token(parser);
				value = parse_negative_integer_literal(parser, tok, integer);
			}
			else if (peek_token(parser, TOK_NUMBER))
			{
				Token number = consume_token(parser);
				value = create_num_ast(parser, tok.site, -number.number);
			}
			else
			{
				value = parse_subexpr(parser, 10000);
				value = create_binary_ast(parser, tok.site, AST_SUB, create_int_ast(parser, tok.site, 0), value);
			}
		}
		break;
		case TOK_ADD:
		{
			consume_token(parser);
			value = parse_subexpr(parser, 10000);
		}
		break;
		case TOK_JSON:
		{
			consume_token(parser);
			value = parse_json_ast_value(parser);
		}
		break;
		case TOK_LEFT_BRACE:
		{
			value = parse_table_expr(parser);
		}
		break;

		case TOK_LEFT_PAREN:
		{
			consume_token(parser);
			if (peek_token(parser, TOK_PAREN_RIGHT))
			{
				parser_error(parser, ERROR_INVALID_EXPRESSION, tok.site, "expected expression inside parentheses");
				value = ERROR_AST;
			}
			else
			{
				value = parse_expr(parser);
			}
			take_token(parser, TOK_PAREN_RIGHT);
		}
		break;
		case TOK_FUN:
		{
			value = parse_function(parser);
		}
		break;
		case TOK_RECURSE:
		{
			consume_token(parser);
			value = create_nullary_ast(parser, tok.site, AST_RECURSE);
		}
		break;
		case TOK_M_GET_MEM:
		{
			value = parse_get_mem_expr(parser);
		}
		break;

		default:
		{
			parser_unexpected_token(parser, tok);
			value = ERROR_AST;
		}
		break;
	}

	esc:
	return value;
}

static Ast parse_table_entry(Parser *parser)
{
	Token start = parser->tok;
	Ast key = NULL_AST;
	Ast value = NULL_AST;
	b32 key_required = false;

	if (peek_token(parser, TOK_IDENTIFIER) && peek_next_token(parser, TOK_ASSIGN))
	{
		Token name = consume_token(parser);
		take_token(parser, TOK_ASSIGN);

		key = create_atom_ast(parser, name.site, name.atom);
		value = parse_expr(parser);
		key_required = true;
	}
	else if (pick_token(parser, TOK_SQUARE_LEFT))
	{
		key = parse_expr(parser);
		take_token(parser, TOK_SQUARE_RIGHT);
		take_token(parser, TOK_ASSIGN);

		value = parse_expr(parser);
		key_required = true;
	}
	else
	{
		value = parse_expr(parser);
		if (pick_token(parser, TOK_ASSIGN))
		{
			key = value;
			value = parse_expr(parser);
			key_required = true;
		}
	}

	if (ast_is_missing_or_error(value) || (key_required && ast_is_missing_or_error(key)))
	{
		parser_invalid_table_entry(parser, start.site);
		return ERROR_AST;
	}

	return create_table_entry_ast(parser, start.site, key, value);
}

static Ast parse_table_expr(Parser *parser)
{
	Token table_token = take_token(parser, TOK_LEFT_BRACE);

	u32 nargs = 0;
	while (!peek_token(parser, TOK_NONE) && !peek_token(parser, TOK_RIGHT_BRACE))
	{
		Ast entry = parse_table_entry(parser);
		if (ast_is_error(entry)) return ERROR_AST;

		push_ast(parser, entry);
		++ nargs;

		pick_token(parser, TOK_COMMA);
	}

	take_token(parser, TOK_RIGHT_BRACE);

	Ast *args = pop_ast_array(parser, nargs);
	Ast table = create_table_ast(parser, table_token.site, args, nargs);
	return table;
}

static u32 parse_call_args(Parser *par)
{
	u32 nargs = 0;

	if (peek_token(par, TOK_LEFT_BRACE))
	{
		Ast expr = parse_table_expr(par);
		if (ast_is_missing_or_error(expr)) goto esc;

		push_ast(par, expr);
		++ nargs;
	}
	else if (pick_token(par, TOK_LEFT_PAREN))
	{
		pick_token(par, TOK_COMMA);

		if (!peek_token(par, TOK_PAREN_RIGHT))
		{
			do
			{
				if (peek_token(par, TOK_PAREN_RIGHT))
				{
					parser_error(par, ERROR_EXCESS_COMMA, par->tok_prev.site, "excess comma");
				}
				if (peek_token(par, TOK_COMMA))
				{
					parser_error(par, ERROR_EXCESS_COMMA, par->tok.site, "excess comma");
				}

				Ast x = parse_expr(par);
				if (ast_is_missing_or_error(x)) goto esc;

				push_ast(par, x);
				++ nargs;
			}
			while (pick_token(par, TOK_COMMA));
		}

		take_token(par, TOK_PAREN_RIGHT);
	}
	else
	{
		Ast expr = parse_expr(par);
		if (ast_is_missing_or_error(expr)) goto esc;
		push_ast(par, expr);
		++ nargs;
	}

	esc:
	return nargs;
}

static Ast parse_field_postfix(Parser *par, Ast left)
{
	Token tok = take_token(par, TOK_DOT);

	if (pick_token(par, TOK_LEFT_PAREN))
	{
		u32 nargs = 0;
		do
		{
			Token name = take_token(par, TOK_IDENTIFIER);
			Ast right = create_atom_ast(par, name.site, name.atom);
			Ast value = create_field_ast(par, tok.site, left, right);
			push_ast(par, value);
			++ nargs;
		}
		while (pick_token(par, TOK_COMMA));

		Ast *args = pop_ast_array(par, nargs);
		left = create_tuple_ast(par, tok.site, args, nargs);
		take_token(par, TOK_PAREN_RIGHT);
	}
	else if (pick_token(par, TOK_LEFT_BRACE))
	{
		parser_error(par, ERROR_INVALID_EXPRESSION, tok.site, "field table projection is not implemented");
		return ERROR_AST;
	}
	else if (pick_token(par, TOK_SQUARE_LEFT))
	{
		Ast right = parse_expr(par);
		if (ast_is_missing_or_error(right)) {
			return ERROR_AST;
		}
		take_token(par, TOK_SQUARE_RIGHT);
		left = create_field_ast(par, tok.site, left, right);
	}
	else
	{
		Token name = take_token(par, TOK_IDENTIFIER);
		Ast right = create_atom_ast(par, name.site, name.atom);
		left = create_field_ast(par, tok.site, left, right);
	}
	return left;
}

// array_index_postfix ::= "[" index_expr ("," index_expr)* "]"
// index_expr           ::= expr | expr? "..." expr?
// Comma-separated indexes are chained: value[x, y] is value[x][y].
static Ast parse_array_index_postfix(Parser *par, Ast value)
{
	Token tok = take_token(par, TOK_SQUARE_LEFT);

	do
	{
		Ast index = parse_expr(par);
		if (ast_is_missing_or_error(index)) goto _err;
		if (index->kind == AST_ELLIPSIS)
		{
			Ast right = 0;
			if (!peek_token(par, TOK_SQUARE_RIGHT) && !peek_token(par, TOK_COMMA))
			{
				right = parse_expr(par);
				if (ast_is_missing_or_error(right)) goto _err;
			}
			index = create_binary_ast(par, index->site, AST_RANGE, 0, right);
		}

		if (index->kind == AST_TUPLE)
		{
			parser_error(par, ERROR_INVALID_EXPRESSION, index->site, "tuple indexes are not implemented");
			goto _err;
		}
		else if (index->kind == AST_RANGE)
		{
			value = create_range_index_ast(par, tok.site, value, index);
			if (ast_is_error(value)) goto _err;
		}
		else
		{
			value = create_index_ast(par, tok.site, value, index);
			if (ast_is_error(value)) goto _err;
		}
	}
	while (pick_token(par, TOK_COMMA));

	take_token(par, TOK_SQUARE_RIGHT);
	return value;

	_err:
	take_token(par, TOK_SQUARE_RIGHT);
	return ERROR_AST;
}

static Ast parse_postfix_expr(Parser *parser)
{
	Token tok = parser->tok;

	Ast value = parse_unary_expr(parser);
	if (ast_is_error(value)) goto esc;

	while (parser->tok.type != TOK_NONE && !parser->tok.line_break_before)
	{
		tok = parser->tok;

		switch (tok.type)
		{
			case TOK_DOT:
			{
				value = parse_field_postfix(parser, value);
			}
			break;

			case TOK_SQUARE_LEFT:
			{
				value = parse_array_index_postfix(parser, value);
			}
			break;

			case TOK_COLON:
			{
				consume_token(parser);
				Token name = take_token(parser, TOK_IDENTIFIER);
				Ast y = create_atom_ast(parser, name.site, name.atom);
				value = create_meta_field_ast(parser, tok.site, value, y);
			}
			break;

			case TOK_LEFT_BRACE:
			case TOK_LEFT_PAREN:
			{
				u32 nargs = parse_call_args(parser);
				Ast *args = pop_ast_array(parser, nargs);
				value = create_call_ast(parser, tok.site, value, args, nargs);
			}
			break;

			default: goto esc;
		}
	}

	esc:
	return value;
}

static Ast parse_subexpr(Parser *parser, u32 upper_precedence)
{
	Token tok = parser->tok;

	Ast x = parse_postfix_expr(parser);
	if (ast_is_error(x)) goto esc;

	for (;;)
	{
		tok = parser->tok;
		if (tok.line_break_before) break;

		AstType ast_type = binary_ast_expr_type_from_token_type(tok.type);
		if (ast_type == AST_NONE) break;

		u32 inner_precedence = precedence_from_binary_ast_expr_type(ast_type);
		if (inner_precedence <= upper_precedence) break;

		consume_token(parser);

		Ast y = 0;
		if (ast_type != AST_RANGE ||
		(!peek_token(parser, TOK_SQUARE_RIGHT) && !peek_token(parser, TOK_COMMA)))
		{
			y = parse_subexpr(parser, inner_precedence);
		}
		if (ast_is_missing_or_error(y) && !(ast_type == AST_RANGE && !y))
		{
			parser_error(parser, ERROR_INVALID_EXPRESSION, tok.site, "expected expression after binary operator");
			x = ERROR_AST;
			break;
		}

		x = create_binary_ast(parser, tok.site, ast_type, x, y);
	}

	esc:
	return x;
}

static Ast parse_expr(Parser *parser)
{
	return parse_subexpr(parser, 0);
}

static Ast parse_expr_stat(Parser *parser)
{
	Ast x = parse_tuple_expr(parser);
	if (ast_is_error(x)) {
		return ERROR_AST;
	}
	if (!x) {
		parser_unexpected_token(parser, parser->tok);
		return ERROR_AST;
	}

	Token tok = parser->tok;
	AstType compound_assign_type = compound_assign_ast_type_from_token_type(tok.type);
	if (compound_assign_type != AST_NONE)
	{
		consume_token(parser);

		Ast y = parse_tuple_expr(parser);
		if (ast_is_missing_or_error(y)) {
			return ERROR_AST;
		}
		return create_binary_ast(parser, tok.site, compound_assign_type, x, y);
	}

	switch (parser->tok.type)
	{
		case TOK_BIND:
		case TOK_HARD_BIND:
		{
			consume_token(parser);

			Ast y = parse_tuple_expr(parser);
			if (ast_is_missing_or_error(y)) {
				return ERROR_AST;
			}
			u32 tags = tok.type == TOK_HARD_BIND ? AST_DECL_TAG_CONSTANT : AST_DECL_TAG_NONE;
			x = create_decl_ast(parser, tok.site, tags, x, y);
		}
		break;
		case TOK_ASSIGN:
		{
			consume_token(parser);

			Ast y = parse_tuple_expr(parser);
			if (ast_is_missing_or_error(y)) {
				return ERROR_AST;
			}
			x = create_assign_ast(parser, tok.site, x, y);
		}
		break;
		case TOK_NIL_ASSIGN:
		{
			consume_token(parser);

			Ast y = parse_tuple_expr(parser);
			if (ast_is_missing_or_error(y)) {
				return ERROR_AST;
			}
			x = create_binary_ast(parser, tok.site, AST_NIL_ASSIGN, x, y);
		}
		break;
		default:
		{
		}
		break;
	}
	return x;
}

static Ast parse_stat(Parser *parser)
{
	Token tok = parser->tok;
	Ast tree = 0;

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

		case TOK_DEFER:
		{
			consume_token(parser);
			tree = parse_stat(parser);
			if (ast_is_missing_or_error(tree)) {
				return ERROR_AST;
			}
			tree = create_defer_ast(parser, tok.site, tree);
		}
		break;

		case TOK_RET:
		{
			consume_token(parser);

			Ast expr = 0;
			if (!parser->tok.line_break_before)
			{
				expr = parse_optional_tuple_expr(parser);
				if (ast_is_error(expr)) {
					return ERROR_AST;
				}
			}
			tree = create_return_ast(parser, tok.site, expr);
		}
		break;

		case TOK_BREAK:
		{
			consume_token(parser);
			Ast expr = 0;
			if (!parser->tok.line_break_before)
			{
				expr = parse_optional_tuple_expr(parser);
				if (ast_is_error(expr)) {
					return ERROR_AST;
				}
			}
			tree = create_break_ast(parser, tok.site, expr);
		}
		break;

		case TOK_CONTINUE:
		{
			consume_token(parser);
			Ast expr = 0;
			if (!parser->tok.line_break_before)
			{
				expr = parse_optional_tuple_expr(parser);
				if (ast_is_error(expr)) {
					return ERROR_AST;
				}
			}
			tree = create_continue_ast(parser, tok.site, expr);
		}
		break;

		case TOK_LEFT_BRACE:
		{
			u32 nstats = 0;
			take_token(parser, TOK_LEFT_BRACE);
			while (!peek_token(parser, TOK_NONE) && !peek_token(parser, TOK_RIGHT_BRACE))
			{
				Token before = parser->tok;
				Ast_T *stat = parse_stat(parser);
				if (ast_is_error(stat)) {
					return ERROR_AST;
				}
				if (!stat)
				{
					parser_unexpected_token(parser, before);
					return ERROR_AST;
				}
				if (before.site.data == parser->tok.site.data && before.type == parser->tok.type)
				{
					parser_error(parser, ERROR_INVALID_STATEMENT, before.site, "parser made no progress while reading block statement");
					return ERROR_AST;
				}

				push_ast(parser, stat);
				++ nstats;
			}
			take_token(parser, TOK_RIGHT_BRACE);

			Ast *stats = pop_ast_array(parser, nstats);
			tree = create_block_ast(parser, tok.site, stats, nstats);
		}
		break;

		case TOK_WHILE:
		{
			consume_token(parser);

			Ast pred = parse_expr(parser);
			if (ast_is_missing_or_error(pred)) return ERROR_AST;

			take_token(parser, TOK_QMARK);

			Ast body = parse_stat(parser);
			if (ast_is_missing_or_error(body)) return ERROR_AST;

			tree = create_while_ast(parser, tok.site, pred, body);
		}
		break;

		case TOK_FOR:
		{
			consume_token(parser);

			Ast name = parse_for_identifier_tuple(parser);
			if (ast_is_missing_or_error(name)) {
				return ERROR_AST;
			}
			u32 decl_tags = AST_DECL_TAG_NONE;
			if (pick_token(parser, TOK_BIND))
			{
			}
			else
			{
				take_token(parser, TOK_HARD_BIND);
				decl_tags |= AST_DECL_TAG_CONSTANT;
			}

			Ast init_expr = parse_tuple_expr(parser);
			if (ast_is_missing_or_error(init_expr)) {
				return ERROR_AST;
			}
			Ast init = create_decl_ast(parser, tok.site, decl_tags, name, init_expr);

			if (pick_token(parser, TOK_QMARK))
			{
				if (init_expr->tuple.nargs != 1)
				{
					parser_error(parser, ERROR_INVALID_EXPRESSION, init_expr->site,
					"range for loops require exactly one expression on the right-hand side");
					return ERROR_AST;
				}

				Ast range = init_expr->tuple.args[0];
				if (!range || (range->kind != AST_RANGE && range->kind != AST_RANGE_INDEX))
				{
					parser_error(parser, ERROR_INVALID_EXPRESSION, init_expr->site,
					"range for loops require a range or ranged index expression");
					return ERROR_AST;
				}

				Ast body = parse_stat(parser);
				if (ast_is_missing_or_error(body)) {
					return ERROR_AST;
				}

				tree = create_for_range_ast(parser, tok.site, init, body);
			}
			else
			{
				take_token(parser, TOK_SEMICOLON);

				Ast pred = parse_expr(parser);
				if (ast_is_missing_or_error(pred)) {
					return ERROR_AST;
				}
				take_token(parser, TOK_SEMICOLON);

				Ast step = parse_expr_stat(parser);
				if (ast_is_missing_or_error(step)) {
					return ERROR_AST;
				}
				take_token(parser, TOK_QMARK);

				Ast body = parse_stat(parser);
				if (ast_is_missing_or_error(body)) {
					return ERROR_AST;
				}

				tree = create_for_ast(parser, tok.site, init, pred, step, body);
			}
		}
		break;

		case TOK_IF:
		{
			consume_token(parser);
			tree = parse_if_stat(parser);
		}
		break;

		default:
		{
			tree = parse_expr_stat(parser);
		}
		break;
	}

	return tree;
}

static Ast parse_if_stat(Parser *parser)
{
	Token tok = parser->tok;
	Ast pred = parse_expr(parser);
	if (ast_is_missing_or_error(pred)) {
		return ERROR_AST;
	}
	if (!pick_token(parser, TOK_QMARK)) {
		parser_error(parser, ERROR_EXPECTED_TOKEN, pred->site, "expected '?' for 'if' statement");
		return ERROR_AST;
	}

	Ast true_clause = parse_stat(parser);
	if (ast_is_missing_or_error(true_clause)) {
		return ERROR_AST;
	}

	Ast else_clause = 0;
	if (pick_token(parser, TOK_ELIF)) {
		else_clause = parse_if_stat(parser);
	}
	else if (pick_token(parser, TOK_ELSE)) {
		else_clause = parse_stat(parser);
	}
	if (ast_is_error(else_clause)) {
		return ERROR_AST;
	}

	Ast stat = create_if_ast(parser, tok.site, pred, true_clause, else_clause);
	return stat;
}

static b32 eval_constexpr_ast(Parser *parser, Ast ast, elf_Value *out)
{
	if (!ast)
	{
		*out = value_nil();
		return true;
	}

	switch (ast->kind)
	{
		case AST_TUPLE:
		{
			if (ast->tuple.nargs != 1)
			{
				parser_error(parser, ERROR_INVALID_EXPRESSION, ast->site, "constant expression must produce one value");
				return false;
			}
			return eval_constexpr_ast(parser, ast->tuple.args[0], out);
		}

		case AST_NIL_LITERAL:
		{
			*out = value_nil();
			return true;
		}

		case AST_INTEGER_LITERAL:
		{
			*out = value_from_integer(ast->integer_value);
			return true;
		}

		case AST_NUMBER_LITERAL:
		{
			*out = value_from_number(ast->number_value);
			return true;
		}

		case AST_STRING_LITERAL:
		{
			*out = value_from_string(elf_string_from_atom(parser->state, ast->atom));
			return true;
		}

		case AST_TABLE:
		{
			elf_Table *table = elf_new_table_rogue(parser->state);
			for (u32 i = 0; i < ast->table.nargs; ++ i)
			{
				Ast entry_ast = ast->table.args[i];
				if (!entry_ast || entry_ast->kind != AST_TABLE_ENTRY)
				{
					parser_error(parser, ERROR_INVALID_EXPRESSION, ast->site, "invalid table constant");
					return false;
				}

				elf_Value value = {};
				if (!eval_constexpr_ast(parser, entry_ast->table_entry.value, &value))
				{
					return false;
				}

				if (entry_ast->table_entry.key)
				{
					elf_Value key = {};
					if (!eval_constexpr_ast(parser, entry_ast->table_entry.key, &key))
					{
						return false;
					}
					elf_table_set(parser->state, table, key, value);
				}
				else
				{
					elf_array_add(parser->state, table, value);
				}
			}

			*out = value_from_table(table);
			return true;
		}

		default:
		{
			parser_errorf(parser, ERROR_INVALID_EXPRESSION, ast->site
			,	"'%s' is not a constant expression", ast_type_name(ast->kind));
			return false;
		}
	}
}

static int push_constexpr_value(Parser *parser, Ast ast)
{
	if (ast_is_error(ast) || parser_has_failed(parser)) return false;

	parser->error_status = ELF_STATUS_EVALUATION_ERROR;
	elf_Value value = {};
	b32 ok = eval_constexpr_ast(parser, ast, &value);
	if (!ok || parser_has_failed(parser)) return false;
	push_value(parser->state, value);
	return true;
}

static int parse_constexpr(Parser *parser)
{
	if (parser_has_failed(parser)) return false;
	Ast ast = parse_tuple_expr(parser);
	if (parser_has_failed(parser)) return false;
	if (!peek_token(parser, TOK_NONE))
	{
		parser_unexpected_token(parser, parser->tok);
		return false;
	}
	return push_constexpr_value(parser, ast);
}

