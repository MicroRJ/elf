static Com_Source *lexer_test_add_source(Compiler *compiler, elf_StrSlice name, elf_StrSlice source_text)
{
	ASSERT(compiler);
	ASSERT(name.size <= UINT32_MAX);
	ASSERT(source_text.size <= UINT32_MAX);

	Atom *name_atom = atom_from_data_size(compiler->atoms, name.data, (u32)name.size);
	Com_Source *source = arena_push_zero(compiler->arena, sizeof(*source), _Alignof(Com_Source));
	ASSERT(name_atom);
	ASSERT(source);

	source->compiler = compiler;
	source->name = name_atom;
	source->path = name_atom;
	source->source = (dy_String) {source_text.data, source_text.size};
	if (compiler->last_source)
	{
		compiler->last_source->next = source;
	}
	else
	{
		compiler->first_source = source;
	}
	compiler->last_source = source;
	return source;
}

static Parser lexer_test_parser_slice(elf_State *state, const char *source, u32 size)
{
	Parser parser = {};
	elf_StrSlice source_buffer = {(char *)source, size};
	Compiler *compiler = compiler_create(&state->arena);
	parser.source = lexer_test_add_source(compiler, (elf_StrSlice) {(char *)"lexer_tests", sizeof("lexer_tests") - 1}, source_buffer);
	parser.result_arena = &state->arena;
	parser.scratch_arena = arena_create(1024 * 1024);
	ASSERT(lexer_tokenize(parser.source, &parser.scratch_arena, &parser.tokens) == COM_STATUS_OK);
	return parser;
}

static Parser lexer_test_parser(elf_State *state, const char *source)
{
	return lexer_test_parser_slice(state, source, (u32)strlen(source));
}

static Token lexer_next(Parser *parser)
{
	ASSERT(parser->token_index < parser->tokens.count);
	Token token = parser->tokens.items[parser->token_index];
	if (parser->token_index + 1 < parser->tokens.count) {
		parser->token_index += 1;
	}
	return token;
}

static void expect_token_type(Token token, Token_Type type, const char *label)
{
	if (token.type != type) {
		TEST_FAIL("%s expected token %s, got %s", label, token_type_name(type), token_type_name(token.type));
	}
}

static Token lexer_next_interpolation_start(Parser *parser, const char *label)
{
	expect_token_type(lexer_next(parser), TOK_INTERPOLATION_START, label);
	Token text = lexer_next(parser);
	expect_token_type(text, TOK_STRING, label);
	return text;
}

static Token lexer_next_interpolation_end(Parser *parser, const char *label)
{
	Token text = lexer_next(parser);
	expect_token_type(text, TOK_STRING, label);
	expect_token_type(lexer_next(parser), TOK_INTERPOLATION_END, label);
	return text;
}

static void expect_token_atom(Token token, const char *text, const char *label)
{
	if (!token.atom) {
		TEST_FAIL("%s expected string '%s', got no string", label, text);
		return;
	}

	const char *data = atom_data(token.atom);
	u32 size = atom_size(token.atom);
	u32 expected_size = (u32)strlen(text);

	if (size != expected_size || memcmp(data, text, expected_size) != 0) {
		TEST_FAIL("%s expected string '%s', got '%.*s'", label, text, size, data);
	}
}

static void expect_token_atom_bytes(Token token, const char *data, u32 size, const char *label)
{
	if (!token.atom) {
		TEST_FAIL("%s expected string bytes, got no string", label);
		return;
	}

	const char *actual = atom_data(token.atom);
	u32 actual_size = atom_size(token.atom);
	if (actual_size != size || memcmp(actual, data, size) != 0) {
		TEST_FAIL("%s expected %u string bytes, got %u", label, size, actual_size);
	}
}

static void expect_token_line(Parser *parser, Token token, u32 index, const char *text, const char *label)
{
	u32 expected_size = (u32)strlen(text);
	const char *line_start = parser->source->source.data + token.site.line_offset;
	const char *source_end = parser->source->source.data + parser->source->source.size;

	u32 line_size = 0;
	while (line_start + line_size < source_end &&
		line_start[line_size] != '\n' && line_start[line_size] != '\r')
	{
		line_size += 1;
	}

	if (token.site.line_index != index ||
		line_size != expected_size ||
		memcmp(line_start, text, expected_size) != 0)
	{
		TEST_FAIL("%s expected line %u '%s', got line %u '%.*s'", label, index, text, token.site.line_index, (i32)line_size, line_start);
	}
}

static void expect_token_site(Parser *parser, Token token, u64 offset, const char *label)
{
	u64 actual = token.site.offset;

	if (actual != offset) {
		TEST_FAIL("%s expected site offset %llu, got %llu", label, (unsigned long long)offset, (unsigned long long)actual);
	}
}

static void expect_token_site_size(Token token, u32 size, const char *label)
{
	if (token.site.size != size) {
		TEST_FAIL("%s expected site size %u, got %u", label, size, token.site.size);
	}
}

static void expect_token_line_break(Token token, b32 expected, const char *label)
{
	if (!!token.line_break_before != !!expected) {
		TEST_FAIL("%s expected line_break_before %u, got %u", label, !!expected, !!token.line_break_before);
	}
}

static void expect_token_int(Token token, u64 expected, const char *label)
{
	if (token.integer_magnitude != expected) {
		TEST_FAIL("%s expected integer %llu, got %llu", label, (unsigned long long)expected, (unsigned long long)token.integer_magnitude);
	}
}

static void expect_token_number(Token token, f64 expected, const char *label)
{
	f64 delta = token.number - expected;
	if (delta < 0) {
		delta = -delta;
	}

	if (delta > 0.000001) {
		TEST_FAIL("%s expected number %f, got %f", label, expected, token.number);
	}
}

static void test_lexer_keywords_and_identifiers(elf_State *state)
{
	Parser parser = lexer_test_parser(state, "true false if fun export import recurse this this_value load int str arr");

	expect_token_type(lexer_next(&parser), TOK_TRUE, "lex true keyword");
	expect_token_type(lexer_next(&parser), TOK_FALSE, "lex false keyword");
	expect_token_type(lexer_next(&parser), TOK_IF, "lex if keyword");
	expect_token_type(lexer_next(&parser), TOK_FUN, "lex fun keyword");
	expect_token_type(lexer_next(&parser), TOK_EXPORT, "lex export keyword");
	expect_token_type(lexer_next(&parser), TOK_IMPORT, "lex import keyword");
	expect_token_type(lexer_next(&parser), TOK_RECURSE, "lex recurse keyword");

	Token this_identifier = lexer_next(&parser);
	expect_token_type(this_identifier, TOK_IDENTIFIER, "this is an ordinary identifier");
	expect_token_atom(this_identifier, "this", "this identifier atom payload");

	Token identifier = lexer_next(&parser);
	expect_token_type(identifier, TOK_IDENTIFIER, "lex identifier near keyword");
	expect_token_atom(identifier, "this_value", "identifier atom payload");

	Token load = lexer_next(&parser);
	expect_token_type(load, TOK_IDENTIFIER, "load is an ordinary identifier");
	expect_token_atom(load, "load", "load identifier atom payload");

	Token integer_type = lexer_next(&parser);
	expect_token_type(integer_type, TOK_IDENTIFIER, "int type name remains an identifier");
	if (!integer_type.atom || integer_type.atom->id != ATOM_TYPE_NAME_INT) {
		test_fail("int type name atom keeps its type id");
	}

	Token string_type = lexer_next(&parser);
	expect_token_type(string_type, TOK_IDENTIFIER, "str type name remains an identifier");
	if (!string_type.atom || string_type.atom->id != ATOM_TYPE_NAME_STR) {
		test_fail("str type name atom keeps its type id");
	}

	Token array_type = lexer_next(&parser);
	expect_token_type(array_type, TOK_IDENTIFIER, "arr type name remains an identifier");
	if (!array_type.atom || array_type.atom->id != ATOM_TYPE_NAME_ARR) {
		test_fail("arr type name atom keeps its type id");
	}

	Atom *keyword = atom_from_data(parser.source->compiler->atoms, "true");
	if (keyword->id != TOK_TRUE) {
		test_fail("keyword atom keeps token id");
	}
}

static void test_lexer_macros(elf_State *state)
{
	Parser parser = lexer_test_parser(state, "#int #get_mem #file_name");

	expect_token_type(lexer_next(&parser), TOK_M_INT, "lex #int macro");
	expect_token_type(lexer_next(&parser), TOK_M_GET_MEM, "lex #get_mem macro");

	Token file_name = lexer_next(&parser);
	expect_token_type(file_name, TOK_STRING, "lex #file_name macro");
	expect_token_atom(file_name, "lexer_tests", "#file_name string payload");

	Atom *macro = atom_from_data(parser.source->compiler->atoms, "#int");
	if (macro->id != TOK_M_INT) {
		test_fail("macro atom keeps token id");
	}

	Parser invalid = lexer_test_parser(state, "# 123 #. abc");

	Token bare_hash = lexer_next(&invalid);
	expect_token_type(bare_hash, TOK_ERROR, "bare # returns an error token");
	if (bare_hash.status != COM_STATUS_REPORTED) {
		test_fail("invalid macro retains its source error");
	}
	expect_token_site_size(bare_hash, 1, "bare # consumes only hash");
	if (invalid.source->report.error_count != 1) {
		test_fail("invalid macro reports one error");
	}
	if (!invalid.source->report.first || invalid.source->report.first->code != ERROR_EXPECTED_MACRO_NAME) {
		test_fail("invalid macro records its compiler error code");
	}
}

static void test_lexer_strings(elf_State *state)
{
	Parser plain = lexer_test_parser(state, "\"hello\" \"\"");

	Token plain_string = lexer_next(&plain);
	expect_token_type(plain_string, TOK_STRING, "lex plain string");
	expect_token_atom(plain_string, "hello", "plain string payload");

	Token empty_string = lexer_next(&plain);
	expect_token_type(empty_string, TOK_STRING, "lex empty string");
	expect_token_atom(empty_string, "", "empty string payload");

	Parser interpolation_text = lexer_test_parser(state, "\"${name}\"");

	Token interpolation_text_string = lexer_next(&interpolation_text);
	expect_token_type(interpolation_text_string, TOK_STRING, "plain string keeps interpolation text");
	expect_token_atom(interpolation_text_string, "${name}", "plain string interpolation text payload");

	Parser escaped = lexer_test_parser(state, "\"a\\n\\\\b\"");

	Token escaped_string = lexer_next(&escaped);
	expect_token_type(escaped_string, TOK_STRING, "lex escaped string");
	expect_token_atom(escaped_string, "a\n\\b", "escaped string payload");

	Parser escape_set = lexer_test_parser(state,
		"\"\\\"\\'\\\\\\/\\0\\a\\b\\f\\n\\r\\t\\v\\x41\\u263A\\U0001F600\\uD83D\\uDE00\"");

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

	Token first_string = lexer_next(&separate);
	expect_token_type(first_string, TOK_STRING, "lex first non-joined string");
	expect_token_atom(first_string, "a", "first non-joined string payload");

	Token second_string = lexer_next(&separate);
	expect_token_type(second_string, TOK_STRING, "lex second non-joined string");
	expect_token_atom(second_string, "b", "second non-joined string payload");

	Parser multiline = lexer_test_parser(state, "\"a\nb\"");
	expect_token_type(lexer_next(&multiline), TOK_ERROR, "reject newline in string");
	if (!multiline.source->report.first || multiline.source->report.first->code != ERROR_NEWLINE_IN_STRING) {
		test_fail("newline in string reports its compiler error code");
	}

	Parser backtick = lexer_test_parser(state, "`text`");
	expect_token_type(lexer_next(&backtick), TOK_ERROR, "reject backtick string");
	if (!backtick.source->report.first || backtick.source->report.first->code != ERROR_UNEXPECTED_CHARACTER) {
		test_fail("backtick string reports an unexpected character");
	}

	Parser format = lexer_test_parser(state, "f\"hello ${name}\" f\"plain\"");

	Token string_start = lexer_next_interpolation_start(&format, "lex string start");
	expect_token_atom(string_start, "hello ", "hello");
	Token identifer = lexer_next(&format);
	expect_token_type(identifer, TOK_IDENTIFIER, "lex interpolation identifer");
	expect_token_atom(identifer, "name", "name");
	Token string_end = lexer_next_interpolation_end(&format, "lex interpolation string end");
	expect_token_atom(string_end, "", "");

	Token plain_format = lexer_next(&format);
	expect_token_type(plain_format, TOK_STRING, "lex format prefix without formatting as string");
	expect_token_atom(plain_format, "plain", "format prefix plain string payload");
}

static void test_lexer_format_string_parts(elf_State *state)
{
	Parser multiple = lexer_test_parser(state, "f\"a ${one} b ${two} c\"");

	Token multiple_start = lexer_next_interpolation_start(&multiple, "lex multiple interpolation string start");
	expect_token_atom(multiple_start, "a ", "multiple interpolation leading text");

	Token one = lexer_next(&multiple);
	expect_token_type(one, TOK_IDENTIFIER, "lex first interpolation expression");
	expect_token_atom(one, "one", "first interpolation identifier");

	Token multiple_part = lexer_next(&multiple);
	expect_token_type(multiple_part, TOK_STRING, "lex text between interpolations");
	expect_token_atom(multiple_part, " b ", "multiple interpolation middle text");

	Token two = lexer_next(&multiple);
	expect_token_type(two, TOK_IDENTIFIER, "lex second interpolation expression");
	expect_token_atom(two, "two", "second interpolation identifier");

	Token multiple_end = lexer_next_interpolation_end(&multiple, "lex multiple interpolation string end");
	expect_token_atom(multiple_end, " c", "multiple interpolation trailing text");
	expect_token_type(lexer_next(&multiple), TOK_NONE, "multiple interpolation consumes complete string");

	Parser empty_parts = lexer_test_parser(state, "f\"${first}${second}\"");

	Token empty_start = lexer_next_interpolation_start(&empty_parts, "lex interpolation at start of string");
	expect_token_atom(empty_start, "", "empty leading format text");
	expect_token_atom(lexer_next(&empty_parts), "first", "first adjacent interpolation identifier");

	Token empty_part = lexer_next(&empty_parts);
	expect_token_type(empty_part, TOK_STRING, "lex adjacent interpolation boundary");
	expect_token_atom(empty_part, "", "empty text between interpolations");
	expect_token_atom(lexer_next(&empty_parts), "second", "second adjacent interpolation identifier");

	Token empty_end = lexer_next_interpolation_end(&empty_parts, "lex interpolation at end of string");
	expect_token_atom(empty_end, "", "empty trailing format text");

	Parser many_parts = lexer_test_parser(state,
		"f\"${a}${b}${c}${d}${e}${f}${g}${h}${i}${j}\"");
	u32 many_part_count = 0;
	while (lexer_next(&many_parts).type != TOK_NONE) many_part_count += 1;
	if (many_part_count != 23) {
		test_fail("formatted string grows the lexer token buffer");
	}

	Parser string_expr = lexer_test_parser(state, "f\"value ${\"text\"} done\"");

	lexer_next_interpolation_start(&string_expr, "lex string expression format start");
	Token inner_string = lexer_next(&string_expr);
	expect_token_type(inner_string, TOK_STRING, "lex ordinary string inside interpolation");
	expect_token_atom(inner_string, "text", "ordinary string interpolation payload");
	Token string_expr_end = lexer_next_interpolation_end(&string_expr, "lex string expression format end");
	expect_token_atom(string_expr_end, " done", "string expression trailing text");

	Parser nested_braces = lexer_test_parser(state, "f\"value ${call({1, 2})} done\"");

	lexer_next_interpolation_start(&nested_braces, "lex nested brace format start");
	expect_token_type(lexer_next(&nested_braces), TOK_IDENTIFIER, "lex call in interpolation");
	expect_token_type(lexer_next(&nested_braces), TOK_LEFT_PAREN, "lex call opening parenthesis in interpolation");
	expect_token_type(lexer_next(&nested_braces), TOK_LEFT_BRACE, "lex nested opening brace in interpolation");
	expect_token_type(lexer_next(&nested_braces), TOK_INTEGER, "lex first nested table value");
	expect_token_type(lexer_next(&nested_braces), TOK_COMMA, "lex nested table comma");
	expect_token_type(lexer_next(&nested_braces), TOK_INTEGER, "lex second nested table value");
	expect_token_type(lexer_next(&nested_braces), TOK_RIGHT_BRACE, "lex nested closing brace in interpolation");
	expect_token_type(lexer_next(&nested_braces), TOK_PAREN_RIGHT, "lex call closing parenthesis in interpolation");
	Token nested_end = lexer_next_interpolation_end(&nested_braces, "lex nested brace format end");
	expect_token_atom(nested_end, " done", "nested brace format trailing text");

	Parser deeper_braces = lexer_test_parser(state, "f\"value ${{{1}}} done\"");

	lexer_next_interpolation_start(&deeper_braces, "lex deeper brace format start");
	expect_token_type(lexer_next(&deeper_braces), TOK_LEFT_BRACE, "lex outer nested opening brace");
	expect_token_type(lexer_next(&deeper_braces), TOK_LEFT_BRACE, "lex inner nested opening brace");
	expect_token_type(lexer_next(&deeper_braces), TOK_INTEGER, "lex deeply nested table value");
	expect_token_type(lexer_next(&deeper_braces), TOK_RIGHT_BRACE, "lex inner nested closing brace");
	expect_token_type(lexer_next(&deeper_braces), TOK_RIGHT_BRACE, "lex outer nested closing brace");
	Token deeper_end = lexer_next_interpolation_end(&deeper_braces, "lex deeper brace format end");
	expect_token_atom(deeper_end, " done", "deeper brace format trailing text");

	Parser operator_expr = lexer_test_parser(state, "f\"remainder ${value % 2}\"");

	lexer_next_interpolation_start(&operator_expr, "lex operator expression format start");
	expect_token_type(lexer_next(&operator_expr), TOK_IDENTIFIER, "lex operator expression identifier");
	expect_token_type(lexer_next(&operator_expr), TOK_MOD, "lex format character as modulus inside interpolation");
	expect_token_type(lexer_next(&operator_expr), TOK_INTEGER, "lex operator expression operand");
	lexer_next_interpolation_end(&operator_expr, "lex operator expression format end");

	Parser comment_brace = lexer_test_parser(state, "f\"value ${call(/* } */ item)} done\"");

	lexer_next_interpolation_start(&comment_brace, "lex comment brace format start");
	expect_token_type(lexer_next(&comment_brace), TOK_IDENTIFIER, "lex call before comment containing brace");
	expect_token_type(lexer_next(&comment_brace), TOK_LEFT_PAREN, "lex opening parenthesis before comment containing brace");
	expect_token_type(lexer_next(&comment_brace), TOK_IDENTIFIER, "ignore closing brace inside interpolation comment");
	expect_token_type(lexer_next(&comment_brace), TOK_PAREN_RIGHT, "lex closing parenthesis after interpolation comment");
	Token comment_end = lexer_next_interpolation_end(&comment_brace, "lex comment brace format end");
	expect_token_atom(comment_end, " done", "comment brace format trailing text");

	Parser restored = lexer_test_parser(state, "f\"formatted ${value}\" } next");

	lexer_next_interpolation_start(&restored, "lex restored mode format start");
	expect_token_type(lexer_next(&restored), TOK_IDENTIFIER, "lex restored mode interpolation value");
	lexer_next_interpolation_end(&restored, "lex restored mode format end");
	expect_token_type(lexer_next(&restored), TOK_RIGHT_BRACE, "restore normal mode after formatted string");
	Token restored_next = lexer_next(&restored);
	expect_token_type(restored_next, TOK_IDENTIFIER, "lex token after restored normal mode");
	expect_token_atom(restored_next, "next", "token after restored normal mode payload");
	expect_token_type(lexer_next(&restored), TOK_NONE, "restored mode source consumed completely");

	Parser nested_format = lexer_test_parser(state, "f\"outer ${f\"inner ${value}\"} tail\"");

	Token outer_start = lexer_next_interpolation_start(&nested_format, "lex outer format string start");
	expect_token_atom(outer_start, "outer ", "outer format string leading text");
	Token inner_start = lexer_next_interpolation_start(&nested_format, "lex nested format string start");
	expect_token_atom(inner_start, "inner ", "nested format string leading text");
	expect_token_type(lexer_next(&nested_format), TOK_IDENTIFIER, "lex nested format interpolation value");
	Token inner_end = lexer_next_interpolation_end(&nested_format, "lex nested format string end");
	expect_token_atom(inner_end, "", "nested format string trailing text");
	Token outer_end = lexer_next_interpolation_end(&nested_format, "restore outer format string mode");
	expect_token_atom(outer_end, " tail", "outer format string trailing text");
	expect_token_type(lexer_next(&nested_format), TOK_NONE, "nested format source consumes outer delimiter");
}

static void test_lexer_line_slices(elf_State *state)
{
	Parser parser = lexer_test_parser(state, "alpha\n  beta\r\n\"gamma\"");

	Token alpha = lexer_next(&parser);
	expect_token_type(alpha, TOK_IDENTIFIER, "lex line slice first token");
	expect_token_site(&parser, alpha, 0, "first token site");
	expect_token_site_size(alpha, 5, "first token site size");
	expect_token_line(&parser, alpha, 1, "alpha", "first token line slice");

	Token beta = lexer_next(&parser);
	expect_token_type(beta, TOK_IDENTIFIER, "lex line slice second token");
	expect_token_site(&parser, beta, 8, "second token site");
	expect_token_site_size(beta, 4, "second token site size");
	expect_token_line(&parser, beta, 2, "  beta", "second token line slice");

	Token gamma = lexer_next(&parser);
	expect_token_type(gamma, TOK_STRING, "lex line slice string token");
	expect_token_site(&parser, gamma, 14, "string token site");
	expect_token_site_size(gamma, 7, "string token site size");
	expect_token_line(&parser, gamma, 3, "\"gamma\"", "string token line slice");
}

static void test_lexer_line_tracking_through_skipped_text(elf_State *state)
{
	const char *source = "alpha\n/* block\n comment */\n\"one\\ntwo\"\nnext";
	Parser parser = lexer_test_parser(state, source);

	Token alpha = lexer_next(&parser);
	expect_token_type(alpha, TOK_IDENTIFIER, "lex tracked first token");
	expect_token_site(&parser, alpha, 0, "tracked first token site");
	expect_token_line(&parser, alpha, 1, "alpha", "tracked first token line");

	Token string = lexer_next(&parser);
	expect_token_type(string, TOK_STRING, "lex tracked string token");
	expect_token_site(&parser, string, 27, "tracked string site");
	expect_token_site_size(string, 10, "tracked string site size");
	expect_token_line(&parser, string, 4, "\"one\\ntwo\"", "tracked string line");
	expect_token_atom(string, "one\ntwo", "tracked string payload");

	Token next = lexer_next(&parser);
	expect_token_type(next, TOK_IDENTIFIER, "lex tracked token after string");
	expect_token_site(&parser, next, 38, "tracked token after string site");
	expect_token_line(&parser, next, 5, "next", "tracked token after string line");
}

static void test_lexer_line_breaks(elf_State *state)
{
	const char *source = "alpha beta\n gamma /* same */ delta /* multi\n comment */ epsilon // line\n zeta";
	Parser parser = lexer_test_parser(state, source);

	Token alpha = lexer_next(&parser);
	expect_token_line_break(alpha, false, "first token has no leading line break");

	Token beta = lexer_next(&parser);
	expect_token_line_break(beta, false, "spaces do not create a line break");

	Token gamma = lexer_next(&parser);
	expect_token_line_break(gamma, true, "newline marks the next token");

	Token delta = lexer_next(&parser);
	expect_token_line_break(delta, false, "same-line block comment does not create a line break");

	Token epsilon = lexer_next(&parser);
	expect_token_line_break(epsilon, true, "multiline block comment creates a line break");

	Token zeta = lexer_next(&parser);
	expect_token_line_break(zeta, true, "line comment newline marks the next token");

	Parser string_parser = lexer_test_parser(state, "\"one\\ntwo\":trim\nnext");

	Token string = lexer_next(&string_parser);
	expect_token_type(string, TOK_STRING, "lex escaped newline string for line break test");
	expect_token_line_break(string, false, "string has no leading line break");

	Token colon = lexer_next(&string_parser);
	expect_token_type(colon, TOK_COLON, "lex postfix after multiline token");
	expect_token_line_break(colon, false, "escaped newline does not break following postfix");

	Token trim = lexer_next(&string_parser);
	expect_token_type(trim, TOK_IDENTIFIER, "lex field after multiline token");
	expect_token_line_break(trim, false, "same-line field has no leading line break");

	Token next = lexer_next(&string_parser);
	expect_token_type(next, TOK_IDENTIFIER, "lex token on line after multiline token");
	expect_token_line_break(next, true, "newline after multiline token marks next token");
}

static void test_lexer_numbers(elf_State *state)
{
	Parser parser = lexer_test_parser(state,
		"123 0x10 0b101 18446744073709551615 .25 12.5 1e3 1E3 1.5e2 1.5e-2 .5e+1 0e999999 0x1e3");

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

	Token integer_exponent = lexer_next(&parser);
	expect_token_type(integer_exponent, TOK_NUMBER, "lex integer coefficient exponent");
	expect_token_number(integer_exponent, 1000, "integer coefficient exponent value");
	expect_token_site_size(integer_exponent, 3, "integer coefficient exponent token size");

	Token uppercase_exponent = lexer_next(&parser);
	expect_token_type(uppercase_exponent, TOK_NUMBER, "lex uppercase exponent");
	expect_token_number(uppercase_exponent, 1000, "uppercase exponent value");

	Token fractional_exponent = lexer_next(&parser);
	expect_token_type(fractional_exponent, TOK_NUMBER, "lex fractional coefficient exponent");
	expect_token_number(fractional_exponent, 150, "fractional coefficient exponent value");

	Token negative_exponent = lexer_next(&parser);
	expect_token_type(negative_exponent, TOK_NUMBER, "lex negative exponent");
	expect_token_number(negative_exponent, 0.015, "negative exponent value");
	expect_token_site_size(negative_exponent, 6, "negative exponent token size");

	Token positive_exponent = lexer_next(&parser);
	expect_token_type(positive_exponent, TOK_NUMBER, "lex explicit positive exponent");
	expect_token_number(positive_exponent, 5, "explicit positive exponent value");

	Token zero_large_exponent = lexer_next(&parser);
	expect_token_type(zero_large_exponent, TOK_NUMBER, "lex zero with large exponent");
	expect_token_number(zero_large_exponent, 0, "zero with large exponent value");

	Token hex_with_e = lexer_next(&parser);
	expect_token_type(hex_with_e, TOK_INTEGER, "hexadecimal e remains a digit");
	expect_token_int(hex_with_e, 0x1e3, "hexadecimal e digit value");

	const char *malformed_sources[] = {"1e", "2e+", "3E-"};
	for (u32 i = 0; i < ARRAY_COUNT(malformed_sources); ++i)
	{
		Parser malformed = lexer_test_parser(state, malformed_sources[i]);
		expect_token_type(lexer_next(&malformed), TOK_ERROR, "malformed exponent returns an error token");
		if (malformed.source->report.error_count != 1) {
			test_fail("malformed exponent reports missing digits");
		}
	}
}

static void test_lexer_operators(elf_State *state)
{
	Parser parser = lexer_test_parser(state, ":= ::= :: -> ?? !! ... += -= *= /= %= ^= <<= >>=");

	expect_token_type(lexer_next(&parser), TOK_BIND, "lex :=");
	expect_token_type(lexer_next(&parser), TOK_HARD_BIND, "lex ::=");
	expect_token_type(lexer_next(&parser), TOK_STATIC_BIND, "lex ::");
	expect_token_type(lexer_next(&parser), TOK_SUB, "lex - in ->");
	expect_token_type(lexer_next(&parser), TOK_GT, "lex > in ->");
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

static void test_lexer_compiler_source(elf_State *state)
{
	Parser parser = lexer_test_parser(state, "value");
	Compiler *compiler = parser.source->compiler;
	Com_Source *source = compiler->first_source;
	if (source->name != atom_from_data(compiler->atoms, "lexer_tests")) {
		test_fail("compiler source name uses shared atom table");
	}
	if (source->path != source->name) {
		test_fail("root source initially uses its name as canonical path");
	}
	if (source != parser.source) {
		test_fail("lexer reads its compiler source");
	}
}

static void test_lexer_bounded_source(elf_State *state)
{
	char string_source[] = {'"', 'o', 'k', '"'};
	Parser string_parser = lexer_test_parser_slice(state, string_source, sizeof(string_source));
	Token string = lexer_next(&string_parser);
	expect_token_type(string, TOK_STRING, "lex bounded string");
	expect_token_atom(string, "ok", "bounded string payload");
	expect_token_type(lexer_next(&string_parser), TOK_NONE, "bounded string reaches slice end");

	char operator_source[] = {'.'};
	Parser operator_parser = lexer_test_parser_slice(state, operator_source, sizeof(operator_source));
	expect_token_type(lexer_next(&operator_parser), TOK_DOT, "bounded operator lookahead");
	expect_token_type(lexer_next(&operator_parser), TOK_NONE, "bounded operator reaches slice end");

	char comment_source[] = {'/', '*'};
	Parser comment_parser = lexer_test_parser_slice(state, comment_source, sizeof(comment_source));
	expect_token_type(lexer_next(&comment_parser), TOK_ERROR, "bounded unterminated comment returns an error token");
	if (comment_parser.source->report.error_count != 1) {
		test_fail("bounded unterminated comment reports an error");
	}
}

static void test_lexer_out_of_memory(elf_State *state)
{
	Arena arena = arena_create(0);
	Compiler *compiler = compiler_create(&arena);
	elf_StrSlice source = {(char *)"oom_identifier", 14};
	Com_Source *compiler_source = lexer_test_add_source(compiler, (elf_StrSlice) {(char *)"lexer_oom", sizeof("lexer_oom") - 1}, source);
	Token_Array tokens = {};
	Arena scratch_arena = arena_create(0);
	u64 capacity = arena.capacity;
	arena.capacity = arena.used;
	ASSERT(lexer_tokenize(compiler_source, &scratch_arena, &tokens) == COM_STATUS_OK);
	Token token = tokens.items[0];
	expect_token_type(token, TOK_ERROR, "atom allocation failure returns an error token");
	if (token.status != COM_STATUS_OOM) {
		test_fail("atom allocation failure retains its OOM error");
	}
	arena.capacity = capacity;
	arena_destroy(&scratch_arena);
	arena_destroy(&arena);

	arena = arena_create(0);
	compiler = compiler_create(&arena);
	source = (elf_StrSlice) {(char *)"@", 1};
	compiler_source = lexer_test_add_source(compiler, (elf_StrSlice) {(char *)"lexer_report_oom", sizeof("lexer_report_oom") - 1}, source);
	tokens = (Token_Array) {};
	scratch_arena = arena_create(0);
	capacity = arena.capacity;
	arena.capacity = arena.used;
	ASSERT(lexer_tokenize(compiler_source, &scratch_arena, &tokens) == COM_STATUS_OK);
	token = tokens.items[0];
	expect_token_type(token, TOK_ERROR, "diagnostic allocation failure returns an error token");
	if (token.status != COM_STATUS_OOM) {
		test_fail("diagnostic allocation failure retains its OOM error");
	}
	arena.capacity = capacity;
	arena_destroy(&scratch_arena);
	arena_destroy(&arena);
}

static void run_lexer_tests(elf_State *state)
{
	TEST_RUN("lexer.keywords_and_identifiers", test_lexer_keywords_and_identifiers, state);
	TEST_RUN("lexer.macros", test_lexer_macros, state);
	TEST_RUN("lexer.strings", test_lexer_strings, state);
	TEST_RUN("lexer.format_string_parts", test_lexer_format_string_parts, state);
	TEST_RUN("lexer.line_slices", test_lexer_line_slices, state);
	TEST_RUN("lexer.line_tracking_through_skipped_text", test_lexer_line_tracking_through_skipped_text, state);
	TEST_RUN("lexer.line_breaks", test_lexer_line_breaks, state);
	TEST_RUN("lexer.numbers", test_lexer_numbers, state);
	TEST_RUN("lexer.operators", test_lexer_operators, state);
	TEST_RUN("lexer.compiler_source", test_lexer_compiler_source, state);
	TEST_RUN("lexer.bounded_source", test_lexer_bounded_source, state);
	TEST_RUN("lexer.out_of_memory", test_lexer_out_of_memory, state);
}
