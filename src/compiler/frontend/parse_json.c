//
// See Copyright Notice In elf.h
//

static Ast parse_json_ast_value(Parser *parser);

static b32 json_token_is_null(Token token)
{
	return token.type == TOK_IDENTIFIER && strcmp(atom_data(token.atom), "null") == 0;
}

static Ast json_expect_separator_or_end(Parser *parser, Token_Type end, const char *message, b32 *done)
{
	if (pick_token(parser, end))
	{
		*done = true;
		return NULL_AST;
	}

	if (!pick_token(parser, TOK_COMMA))
	{
		*done = true;
		if (token_is_error(parser->tok)) {
			return ast_from_token_error(parser->tok);
		}
		return parser_error(parser, ERROR_EXPECTED_TOKEN, parser->tok.site, message);
	}
	return NULL_AST;
}

static Ast parse_json_ast_object(Parser *parser)
{
	Token start = take_token(parser, TOK_LEFT_BRACE);
	if (token_is_error(start)) {
		return ast_from_token_error(start);
	}

	u32 nargs = 0;
	if (pick_token(parser, TOK_RIGHT_BRACE))
	{
		Ast table = create_table_ast(parser, start.site, 0, 0);
		if (ast_is_error(table)) {
			return table;
		}
		table->table.json_shape = TABLE_JSON_SHAPE_OBJECT;
		return table;
	}

	b32 done = false;
	while (!done && !peek_token(parser, TOK_NONE))
	{
		Token key_token = take_token(parser, TOK_STRING);
		if (token_is_error(key_token)) {
			return ast_from_token_error(key_token);
		}
		Ast key = create_atom_ast(parser, key_token.site, key_token.atom);
		if (ast_is_error(key)) {
			return key;
		}

		Token colon = take_token(parser, TOK_COLON);
		if (token_is_error(colon)) {
			return ast_from_token_error(colon);
		}

		Ast value = parse_json_ast_value(parser);
		if (ast_is_error(value))
		{
			return value;
		}

		Ast entry = create_table_entry_ast(parser, key_token.site, key, value);
		if (ast_is_error(entry)) {
			return entry;
		}
		if (!push_ast(parser, entry)) {
			return ERROR_OOM_AST;
		}
		nargs += 1;

		Ast separator_error = json_expect_separator_or_end(parser, TOK_RIGHT_BRACE, "expected ',' or '}'", &done);
		if (separator_error) {
			return separator_error;
		}
	}
	if (!done)
	{
		return parser_error(parser, ERROR_EXPECTED_TOKEN, parser->tok.site, "expected '}'");
	}

	Ast *args = pop_ast_array(parser, nargs);
	if (nargs && !args) {
		return ERROR_OOM_AST;
	}
	Ast table = create_table_ast(parser, start.site, args, nargs);
	if (ast_is_error(table)) {
		return table;
	}
	table->table.json_shape = TABLE_JSON_SHAPE_OBJECT;
	return table;
}

static Ast parse_json_ast_array(Parser *parser)
{
	Token start = take_token(parser, TOK_SQUARE_LEFT);
	if (token_is_error(start)) {
		return ast_from_token_error(start);
	}

	u32 nargs = 0;
	if (pick_token(parser, TOK_SQUARE_RIGHT))
	{
		Ast table = create_table_ast(parser, start.site, 0, 0);
		if (ast_is_error(table)) {
			return table;
		}
		table->table.json_shape = TABLE_JSON_SHAPE_ARRAY;
		return table;
	}

	b32 done = false;
	while (!done && !peek_token(parser, TOK_NONE))
	{
		Ast value = parse_json_ast_value(parser);
		if (ast_is_error(value))
		{
			return value;
		}

		Ast entry = create_table_entry_ast(parser, value->site, 0, value);
		if (ast_is_error(entry)) {
			return entry;
		}
		if (!push_ast(parser, entry)) {
			return ERROR_OOM_AST;
		}
		nargs += 1;

		Ast separator_error = json_expect_separator_or_end(parser, TOK_SQUARE_RIGHT, "expected ',' or ']'", &done);
		if (separator_error) {
			return separator_error;
		}
	}
	if (!done)
	{
		return parser_error(parser, ERROR_EXPECTED_TOKEN, parser->tok.site, "expected ']'");
	}

	Ast *args = pop_ast_array(parser, nargs);
	if (nargs && !args) {
		return ERROR_OOM_AST;
	}
	Ast table = create_table_ast(parser, start.site, args, nargs);
	if (ast_is_error(table)) {
		return table;
	}
	table->table.json_shape = TABLE_JSON_SHAPE_ARRAY;
	return table;
}

static Ast parse_json_ast_number(Parser *parser)
{
	Token sign = {};
	b32 is_negative = false;
	if (peek_token(parser, TOK_SUB))
	{
		sign = consume_token(parser);
		is_negative = true;
	}

	Token token = parser->tok;
	switch (token.type)
	{
		case TOK_ERROR:
		{
			return ast_from_token_error(token);
		}

		case TOK_INTEGER:
		{
			consume_token(parser);
			if (is_negative) {
				return parse_negative_integer_literal(parser, sign, token);
			}
			return parse_positive_integer_literal(parser, token);
		}

		case TOK_NUMBER:
		{
			consume_token(parser);
			f64 value = token.number;
			return create_num_ast(parser, is_negative ? sign.site : token.site, is_negative ? -value : value);
		}

		default:
		{
			return parser_error(parser, ERROR_EXPECTED_TOKEN, token.site, "expected number");
		}
	}
}

static Ast parse_json_ast_value(Parser *parser)
{
	Token token = parser->tok;

	switch (token.type)
	{
		case TOK_ERROR:
		{
			return ast_from_token_error(token);
		}

		case TOK_LEFT_BRACE:
		{
			return parse_json_ast_object(parser);
		}

		case TOK_SQUARE_LEFT:
		{
			return parse_json_ast_array(parser);
		}

		case TOK_STRING:
		{
			consume_token(parser);
			return create_atom_ast(parser, token.site, token.atom);
		}

		case TOK_INTEGER:
		case TOK_NUMBER:
		case TOK_SUB:
		{
			return parse_json_ast_number(parser);
		}

		case TOK_TRUE:
		{
			consume_token(parser);
			return create_int_ast(parser, token.site, 1);
		}

		case TOK_FALSE:
		{
			consume_token(parser);
			return create_int_ast(parser, token.site, 0);
		}

		default:
		{
			if (json_token_is_null(token))
			{
				consume_token(parser);
				return create_nil_ast(parser, token.site);
			}

			return parser_error(parser, ERROR_INVALID_EXPRESSION, token.site, "expected JSON value");
		}
	}
}

static Ast parse_json_ast(Parser *parser)
{
	Ast ast = parse_json_ast_value(parser);
	if (ast_is_error(ast)) {
		return ast;
	}
	if (!peek_token(parser, TOK_NONE))
	{
		return parser_unexpected_token(parser, parser->tok);
	}
	return ast;
}
