//
// See Copyright Notice In elf.h
//

static AstRef parse_json_ast_value(Parser *parser);

static b32 json_token_is_null(Token token)
{
	return token.type == TOK_IDENTIFIER && strcmp(atom_data(token.atom), "null") == 0;
}

static void json_expect_separator_or_end(Parser *parser, TokenType end, const char *message, b32 *done)
{
	if (pick_token(parser, end))
	{
		*done = true;
		return;
	}

	if (!pick_token(parser, TOK_COMMA))
	{
		parser_error(parser, ERROR_EXPECTED_TOKEN, parser->tok.site, message);
		*done = true;
	}
}

static AstRef parse_json_ast_object(Parser *parser)
{
	Token start = take_token(parser, TOK_LEFT_BRACE);

	u32 nargs = 0;
	if (pick_token(parser, TOK_RIGHT_BRACE))
	{
		return create_table_ast(parser, start.site, 0, 0);
	}

	b32 done = false;
	while (!done && !peek_token(parser, TOK_NONE))
	{
		Token key_token = take_token(parser, TOK_STRING);
		AstRef key = create_atom_ast(parser, key_token.site, key_token.atom);

		take_token(parser, TOK_COLON);

		AstRef value = parse_json_ast_value(parser);
		if (ast_is_error(value))
		{
			return ERROR_AST;
		}

		AstRef entry = create_table_entry_ast(parser, key_token.site, key, value);
		push_ast(parser, entry);
		nargs += 1;

		json_expect_separator_or_end(parser, TOK_RIGHT_BRACE, "expected ',' or '}'", &done);
	}

	AstRef *args = pop_ast_array(parser, nargs);
	return create_table_ast(parser, start.site, args, nargs);
}

static AstRef parse_json_ast_array(Parser *parser)
{
	Token start = take_token(parser, TOK_SQUARE_LEFT);

	u32 nargs = 0;
	if (pick_token(parser, TOK_SQUARE_RIGHT))
	{
		return create_table_ast(parser, start.site, 0, 0);
	}

	b32 done = false;
	while (!done && !peek_token(parser, TOK_NONE))
	{
		AstRef value = parse_json_ast_value(parser);
		if (ast_is_error(value))
		{
			return ERROR_AST;
		}

		AstRef entry = create_table_entry_ast(parser, value->site, 0, value);
		push_ast(parser, entry);
		nargs += 1;

		json_expect_separator_or_end(parser, TOK_SQUARE_RIGHT, "expected ',' or ']'", &done);
	}

	AstRef *args = pop_ast_array(parser, nargs);
	return create_table_ast(parser, start.site, args, nargs);
}

static AstRef parse_json_ast_number(Parser *parser)
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
			parser_error(parser, ERROR_EXPECTED_TOKEN, token.site, "expected number");
			return ERROR_AST;
		}
	}
}

static AstRef parse_json_ast_value(Parser *parser)
{
	Token token = parser->tok;

	switch (token.type)
	{
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

			parser_error(parser, ERROR_INVALID_EXPRESSION, token.site, "expected JSON value");
			return ERROR_AST;
		}
	}
}

static AstRef parse_json_ast(Parser *parser)
{
	AstRef ast = parse_json_ast_value(parser);
	if (!peek_token(parser, TOK_NONE))
	{
		parser_unexpected_token(parser, parser->tok);
		return ERROR_AST;
	}
	return ast;
}

static int parse_json_value(Parser *parser)
{
	AstRef ast = parse_json_ast(parser);
	return push_constexpr_value(parser, ast);
}
