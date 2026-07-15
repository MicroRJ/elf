static Parser lexer_test_parser(elf_State *state, const char *source)
{
	Parser parser = {};
	parser.state = state;
	parser.name = elf_atom_from_data(state, "lexer_tests");
	elf_StrSlice source_buffer = {(char *)source, (u64)strlen(source)};
	lexer_init(&parser.lexer, state, parser.name, source_buffer);
	return parser;
}

static void lexer_prime(Parser *parser)
{
}

static Token lexer_next(Parser *parser)
{
	return lex_token(&parser->lexer);
}

static void expect_token_type(Token token, TokenType type, const char *label)
{
	if (token.type != type) {
		fprintf(stderr, "FAIL: %s expected token %s, got %s\n",
			label, token_type_name(type), token_type_name(token.type));
		test_failures += 1;
	}
}

static void expect_token_atom(Token token, const char *text, const char *label)
{
	if (!token.atom) {
		fprintf(stderr, "FAIL: %s expected atom '%s', got no atom\n", label, text);
		test_failures += 1;
		return;
	}

	const char *data = elf_atom_data(token.atom);
	u32 size = elf_atom_size(token.atom);
	u32 expected_size = (u32)strlen(text);

	if (size != expected_size || memcmp(data, text, expected_size) != 0) {
		fprintf(stderr, "FAIL: %s expected atom '%s', got '%.*s'\n", label, text, size, data);
		test_failures += 1;
	}
}

static void expect_token_atom_bytes(Token token, const char *data, u32 size, const char *label)
{
	if (!token.atom) {
		fprintf(stderr, "FAIL: %s expected atom bytes, got no atom\n", label);
		test_failures += 1;
		return;
	}

	const char *actual = elf_atom_data(token.atom);
	u32 actual_size = elf_atom_size(token.atom);
	if (actual_size != size || memcmp(actual, data, size) != 0) {
		fprintf(stderr, "FAIL: %s expected %u atom bytes, got %u\n", label, size, actual_size);
		test_failures += 1;
	}
}

static void expect_token_line(Token token, u32 index, const char *text, const char *label)
{
	u32 expected_size = (u32)strlen(text);

	u32 line_size = 0;
	while (token.site.line_start[line_size] &&
		token.site.line_start[line_size] != '\n' &&
		token.site.line_start[line_size] != '\r')
	{
		line_size += 1;
	}

	if (token.site.line_index != index ||
		line_size != expected_size ||
		memcmp(token.site.line_start, text, expected_size) != 0)
	{
		fprintf(stderr, "FAIL: %s expected line %u '%s', got line %u '%.*s'\n",
			label,
			index,
			text,
			token.site.line_index,
			(i32)line_size,
			token.site.line_start);
		test_failures += 1;
	}
}

static void expect_token_site(Parser *parser, Token token, u64 offset, const char *label)
{
	u64 actual = (u64)(token.site.data - parser->lexer.source.data);

	if (actual != offset) {
		fprintf(stderr, "FAIL: %s expected site offset %llu, got %llu\n",
			label,
			offset,
			actual);
		test_failures += 1;
	}
}

static void expect_token_site_size(Token token, u32 size, const char *label)
{
	if (token.site.size != size) {
		fprintf(stderr, "FAIL: %s expected site size %u, got %u\n",
			label,
			size,
			token.site.size);
		test_failures += 1;
	}
}

static void expect_token_int(Token token, u64 expected, const char *label)
{
	if (token.integer_magnitude != expected) {
		fprintf(stderr, "FAIL: %s expected integer %llu, got %llu\n", label, expected, token.integer_magnitude);
		test_failures += 1;
	}
}

static void expect_token_number(Token token, f64 expected, const char *label)
{
	f64 delta = token.number - expected;
	if (delta < 0) {
		delta = -delta;
	}

	if (delta > 0.000001) {
		fprintf(stderr, "FAIL: %s expected number %f, got %f\n", label, expected, token.number);
		test_failures += 1;
	}
}

static void test_lexer_keywords_and_identifiers(elf_State *state)
{
	Parser parser = lexer_test_parser(state, "true false if fun recurse true_value");
	lexer_prime(&parser);

	expect_token_type(lexer_next(&parser), TOK_TRUE, "lex true keyword");
	expect_token_type(lexer_next(&parser), TOK_FALSE, "lex false keyword");
	expect_token_type(lexer_next(&parser), TOK_IF, "lex if keyword");
	expect_token_type(lexer_next(&parser), TOK_FUN, "lex fun keyword");
	expect_token_type(lexer_next(&parser), TOK_RECURSE, "lex recurse keyword");

	Token identifier = lexer_next(&parser);
	expect_token_type(identifier, TOK_IDENTIFIER, "lex identifier near keyword");
	expect_token_atom(identifier, "true_value", "identifier atom payload");

	elf_Atom *keyword = elf_atom_from_data(state, "true");
	if (keyword->id != TOK_TRUE) {
		test_fail("keyword atom keeps token id");
	}
}

static void test_lexer_macros(elf_State *state)
{
	Parser parser = lexer_test_parser(state, "#int #get_mem #file_name");
	lexer_prime(&parser);

	expect_token_type(lexer_next(&parser), TOK_M_INT, "lex #int macro");
	expect_token_type(lexer_next(&parser), TOK_M_GET_MEM, "lex #get_mem macro");

	Token file_name = lexer_next(&parser);
	expect_token_type(file_name, TOK_STRING, "lex #file_name macro");
	expect_token_atom(file_name, "lexer_tests", "#file_name atom payload");

	elf_Atom *macro = elf_atom_from_data(state, "#int");
	if (macro->id != TOK_M_INT) {
		test_fail("macro atom keeps token id");
	}

	Parser invalid = lexer_test_parser(state, "# 123 #. abc");
	lexer_prime(&invalid);

	Token bare_hash = lexer_next(&invalid);
	expect_token_type(bare_hash, TOK_IDENTIFIER, "bare # reports invalid macro token");
	expect_token_site_size(bare_hash, 1, "bare # consumes only hash");

	Token number = lexer_next(&invalid);
	expect_token_type(number, TOK_INTEGER, "token after bare # is preserved");
	expect_token_int(number, 123, "number after bare # value");

	Token punct_hash = lexer_next(&invalid);
	expect_token_type(punct_hash, TOK_IDENTIFIER, "# before punctuation reports invalid macro token");
	expect_token_site_size(punct_hash, 1, "# before punctuation consumes only hash");

	Token dot = lexer_next(&invalid);
	expect_token_type(dot, TOK_DOT, "punctuation after invalid # is preserved");

	Token identifier = lexer_next(&invalid);
	expect_token_type(identifier, TOK_IDENTIFIER, "identifier after invalid macro is preserved");
	expect_token_atom(identifier, "abc", "identifier after invalid macro payload");
}

static void test_lexer_strings(elf_State *state)
{
	Parser plain = lexer_test_parser(state, "\"hello\" \"\"");
	lexer_prime(&plain);

	Token plain_string = lexer_next(&plain);
	expect_token_type(plain_string, TOK_STRING, "lex plain string");
	expect_token_atom(plain_string, "hello", "plain string atom payload");

	Token empty_string = lexer_next(&plain);
	expect_token_type(empty_string, TOK_STRING, "lex empty string");
	expect_token_atom(empty_string, "", "empty string atom payload");

	Parser escaped = lexer_test_parser(state, "\"a\\n\\\\b\"");
	lexer_prime(&escaped);

	Token escaped_string = lexer_next(&escaped);
	expect_token_type(escaped_string, TOK_STRING, "lex escaped string");
	expect_token_atom(escaped_string, "a\n\\b", "escaped string atom payload");

	Parser escape_set = lexer_test_parser(state,
		"\"\\\"\\'\\\\\\/\\0\\a\\b\\f\\n\\r\\t\\v\\x41\\u263A\\U0001F600\\uD83D\\uDE00\"");
	lexer_prime(&escape_set);

	Token escape_set_string = lexer_next(&escape_set);
	expect_token_type(escape_set_string, TOK_STRING, "lex full escape set string");
	{
		const char expected[] = {
			'"', '\'', '\\', '/', '\0', '\a', '\b', '\f', '\n', '\r', '\t', '\v', 'A',
			(char)0xE2, (char)0x98, (char)0xBA,
			(char)0xF0, (char)0x9F, (char)0x98, (char)0x80,
			(char)0xF0, (char)0x9F, (char)0x98, (char)0x80,
		};
		expect_token_atom_bytes(escape_set_string, expected, sizeof(expected), "full escape set string payload");
	}

	Parser escaped_chars = lexer_test_parser(state, "'\\n' '\\x41' '\\u263A' '\\U0001F600'");
	lexer_prime(&escaped_chars);

	Token newline_char = lexer_next(&escaped_chars);
	expect_token_type(newline_char, TOK_LETTER, "lex escaped newline char");
	expect_token_int(newline_char, '\n', "escaped newline char value");

	Token hex_char = lexer_next(&escaped_chars);
	expect_token_type(hex_char, TOK_LETTER, "lex escaped hex char");
	expect_token_int(hex_char, 'A', "escaped hex char value");

	Token unicode_char = lexer_next(&escaped_chars);
	expect_token_type(unicode_char, TOK_LETTER, "lex escaped unicode char");
	expect_token_int(unicode_char, 0x263A, "escaped unicode char value");

	Token wide_unicode_char = lexer_next(&escaped_chars);
	expect_token_type(wide_unicode_char, TOK_LETTER, "lex escaped wide unicode char");
	expect_token_int(wide_unicode_char, 0x1F600, "escaped wide unicode char value");

	Parser separate = lexer_test_parser(state, "\"a\"\n\"b\"");
	lexer_prime(&separate);

	Token first_string = lexer_next(&separate);
	expect_token_type(first_string, TOK_STRING, "lex first non-joined string");
	expect_token_atom(first_string, "a", "first non-joined string atom payload");

	Token second_string = lexer_next(&separate);
	expect_token_type(second_string, TOK_STRING, "lex second non-joined string");
	expect_token_atom(second_string, "b", "second non-joined string atom payload");

	Parser block = lexer_test_parser(state, "\"\"\"a\nb\"\"\"");
	lexer_prime(&block);

	Token block_string = lexer_next(&block);
	expect_token_type(block_string, TOK_STRING, "lex string block");
	expect_token_atom(block_string, "a\nb", "string block atom payload");

	Parser format = lexer_test_parser(state, "f\"hello %{name}\" f\"plain\"");
	lexer_prime(&format);

	Token format_string = lexer_next(&format);
	expect_token_type(format_string, TOK_FORMAT_STRING, "lex format string");
	expect_token_atom(format_string, "hello %{name}", "format string atom payload");

	Token plain_format = lexer_next(&format);
	expect_token_type(plain_format, TOK_STRING, "lex format prefix without formatting as string");
	expect_token_atom(plain_format, "plain", "format prefix plain atom payload");
}

static void test_lexer_line_slices(elf_State *state)
{
	Parser parser = lexer_test_parser(state, "alpha\n  beta\r\n\"gamma\"");
	lexer_prime(&parser);

	Token alpha = lexer_next(&parser);
	expect_token_type(alpha, TOK_IDENTIFIER, "lex line slice first token");
	expect_token_site(&parser, alpha, 0, "first token site");
	expect_token_site_size(alpha, 5, "first token site size");
	expect_token_line(alpha, 1, "alpha", "first token line slice");

	Token beta = lexer_next(&parser);
	expect_token_type(beta, TOK_IDENTIFIER, "lex line slice second token");
	expect_token_site(&parser, beta, 8, "second token site");
	expect_token_site_size(beta, 4, "second token site size");
	expect_token_line(beta, 2, "  beta", "second token line slice");

	Token gamma = lexer_next(&parser);
	expect_token_type(gamma, TOK_STRING, "lex line slice string token");
	expect_token_site(&parser, gamma, 14, "string token site");
	expect_token_site_size(gamma, 7, "string token site size");
	expect_token_line(gamma, 3, "\"gamma\"", "string token line slice");
}

static void test_lexer_line_tracking_through_skipped_text(elf_State *state)
{
	const char *source = "alpha\n/* block\n comment */\n\"\"\"one\ntwo\"\"\"\nnext";
	Parser parser = lexer_test_parser(state, source);
	lexer_prime(&parser);

	Token alpha = lexer_next(&parser);
	expect_token_type(alpha, TOK_IDENTIFIER, "lex tracked first token");
	expect_token_site(&parser, alpha, 0, "tracked first token site");
	expect_token_line(alpha, 1, "alpha", "tracked first token line");

	Token block = lexer_next(&parser);
	expect_token_type(block, TOK_STRING, "lex tracked string block token");
	expect_token_site(&parser, block, 27, "tracked string block site");
	expect_token_site_size(block, 13, "tracked string block site size");
	expect_token_line(block, 4, "\"\"\"one", "tracked string block starting line");
	expect_token_atom(block, "one\ntwo", "tracked string block payload");

	Token next = lexer_next(&parser);
	expect_token_type(next, TOK_IDENTIFIER, "lex tracked token after string block");
	expect_token_site(&parser, next, 41, "tracked token after string block site");
	expect_token_line(next, 6, "next", "tracked token after string block line");
}

static void test_lexer_numbers(elf_State *state)
{
	Parser parser = lexer_test_parser(state, "123 0x10 0b101 18446744073709551615 .25 12.5");
	lexer_prime(&parser);

	Token decimal = lexer_next(&parser);
	expect_token_type(decimal, TOK_INTEGER, "lex decimal integer");
	expect_token_int(decimal, 123, "decimal integer value");

	Token hex = lexer_next(&parser);
	expect_token_type(hex, TOK_INTEGER, "lex hex integer");
	expect_token_int(hex, 16, "hex integer value");

	Token binary = lexer_next(&parser);
	expect_token_type(binary, TOK_INTEGER, "lex binary integer");
	expect_token_int(binary, 5, "binary integer value");

	Token u64_max = lexer_next(&parser);
	expect_token_type(u64_max, TOK_INTEGER, "lex u64 max integer magnitude");
	expect_token_int(u64_max, 18446744073709551615ull, "u64 max integer magnitude");

	Token leading_fraction = lexer_next(&parser);
	expect_token_type(leading_fraction, TOK_NUMBER, "lex leading fractional number");
	expect_token_number(leading_fraction, 0.25, "leading fractional number value");

	Token fraction = lexer_next(&parser);
	expect_token_type(fraction, TOK_NUMBER, "lex fractional number");
	expect_token_number(fraction, 12.5, "fractional number value");
}

static void test_lexer_operators(elf_State *state)
{
	Parser parser = lexer_test_parser(state, ":= ::= -> ?? !! ... += -= *= /= %= ^= <<= >>=");
	lexer_prime(&parser);

	expect_token_type(lexer_next(&parser), TOK_BIND, "lex :=");
	expect_token_type(lexer_next(&parser), TOK_HARD_BIND, "lex ::=");
	expect_token_type(lexer_next(&parser), TOK_ARROW, "lex ->");
	expect_token_type(lexer_next(&parser), TOK_NIL_OR, "lex ??");
	expect_token_type(lexer_next(&parser), TOK_NIL_AND, "lex !!");
	expect_token_type(lexer_next(&parser), TOK_ELLIPSIS, "lex ...");
	expect_token_type(lexer_next(&parser), TOK_ADD_ASSIGN, "lex +=");
	expect_token_type(lexer_next(&parser), TOK_SUB_ASSIGN, "lex -=");
	expect_token_type(lexer_next(&parser), TOK_MUL_ASSIGN, "lex *=");
	expect_token_type(lexer_next(&parser), TOK_DIV_ASSIGN, "lex /=");
	expect_token_type(lexer_next(&parser), TOK_MOD_ASSIGN, "lex %=");
	expect_token_type(lexer_next(&parser), TOK_XOR_ASSIGN, "lex ^=");
	expect_token_type(lexer_next(&parser), TOK_SHL_ASSIGN, "lex <<=");
	expect_token_type(lexer_next(&parser), TOK_SHR_ASSIGN, "lex >>=");
}

static void run_lexer_tests(elf_State *state)
{
	test_lexer_keywords_and_identifiers(state);
	test_lexer_macros(state);
	test_lexer_strings(state);
	test_lexer_line_slices(state);
	test_lexer_line_tracking_through_skipped_text(state);
	test_lexer_numbers(state);
	test_lexer_operators(state);
}
