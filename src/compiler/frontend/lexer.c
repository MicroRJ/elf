//
// See Copyright Notice In elf.h
//

#include <math.h>

#include "source_diagnostics.c"

static const char *token_type_name(Token_Type type)
{
	static const char *names[TOK_COUNT_] =
	{
		[TOK_NONE] = "eof",
#define XPAND(ENUM, NAME) [TOK_##ENUM] = NAME,
		TOKEN_DEFINITIONS(XPAND)
#undef XPAND
	};

	if ((u32)type >= TOK_COUNT_) {
		return "unknown";
	}
	return names[type];
}

static Token_Type atom_is_word_or_macro(Atom *atom)
{
	switch (atom->id) {
#define MCITEM(NAME, SYM) case XFUSE(TOK_, NAME): return XFUSE(TOK_, NAME);
		MACRO_DEFINITIONS(MCITEM)
#undef MCITEM
		default: return TOK_IDENTIFIER;
	}
}

static Token_Type check_keyword(Atom *atom)
{
	switch (atom->id) {
#define KWITEM(NAME, SYM) case XFUSE(TOK_, NAME): return XFUSE(TOK_, NAME);
		KEYWORD_DEFINITIONS(KWITEM)
#undef KWITEM
		default: return TOK_IDENTIFIER;
	}
}

static u32 lexer_scratch_capacity(Lexer *lexer)
{
	return lexer->source.size;
}

enum { LEXER_EOF = -1 };

static inline int lexer_peek(Lexer *lexer, const char *cursor, u32 offset)
{
	ASSERT(cursor >= lexer->source.data && cursor <= lexer->end);
	u64 remaining = (u64)(lexer->end - cursor);
	return offset < remaining ? (u8)cursor[offset] : LEXER_EOF;
}

static b32 is_identifier_start(u32 c)
{
	return ('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z') || c == '_';
}

static b32 is_identifier_continue(u32 c)
{
	return is_identifier_start(c) || ('0' <= c && c <= '9');
}

static b32 lexer_is_hex_digit(u32 c)
{
	return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static void lexer_init(Lexer *lexer, Compiler *compiler, Com_ModuleId module)
{
	ASSERT(module < compiler->module_count);
	Com_Module *source_module = compiler->modules + module;
	lexer->compiler = compiler;
	lexer->module = module;
	lexer->atoms = compiler->atoms;
	lexer->source = source_module->source;
	lexer->cursor = source_module->source.data;
	lexer->end = source_module->source.data + source_module->source.size;
	lexer->line_index = 1;
	lexer->line_start = source_module->source.data;
	lexer->tokens = (Lexer_TokenFIFO){0};
}

static void lexer_advance_line(Lexer *lexer, const char *line_start)
{
	lexer->line_index += 1;
	lexer->line_start = line_start;
}

static SourceSite source_site_from_ptr(Lexer *lexer, const char *data)
{
	ASSERT(data);
	ASSERT(data >= lexer->line_start);
	ASSERT(data <= lexer->end);
	u64 offset = (u64)(data - lexer->source.data);
	u64 line_offset = (u64)(lexer->line_start - lexer->source.data);
	ASSERT(offset <= UINT32_MAX);
	ASSERT(line_offset <= UINT32_MAX);
	return (SourceSite) {
		.module      = lexer->module,
		.offset      = (u32) offset,
		.size        = 1,
		.line_offset = (u32) line_offset,
		.line_index  = lexer->line_index,
	};
}

static SourceSite lexer_source_site(Lexer *lexer, const char *data)
{
	return source_site_from_ptr(lexer, data);
}

static Com_Error log_source_error(Lexer *lexer, SourceSite site, char const *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	b32 reported = compiler_reportv(lexer->compiler, COMPILER_ERROR, COMPILER_PHASE_LEXER, site, fmt, args);
	va_end(args);
	return reported ? COM_ERROR_SOURCE : COM_ERROR_OOM;
}

static u32 lexer_hex_digit(u32 c)
{
	if (c >= '0' && c <= '9') return  0 + (u32)(c - '0');
	if (c >= 'a' && c <= 'f') return 10 + (u32)(c - 'a');
	return 10 + (u32)(c - 'A');
}

static Com_Error lexer_parse_hex_codepoint(Lexer *lexer, const char **cursor, u32 digits, SourceSite site, u32 *out)
{
	const char *cur = *cursor;
	u32 codepoint = 0;
	for (u32 i = 0; i < digits; ++i)
	{
		int digit = lexer_peek(lexer, cur, i);
		if (digit == LEXER_EOF || !lexer_is_hex_digit((char)digit))
		{
			return log_source_error(lexer, site, "invalid unicode escape");
		}
		codepoint = (codepoint << 4) | lexer_hex_digit((char)digit);
	}

	*cursor = cur + digits;
	*out = codepoint;
	return COM_ERROR_NONE;
}

static Com_Error lexer_parse_unicode_escape(Lexer *lexer, const char **cursor, SourceSite site, u32 *out)
{
	const char *cur = *cursor;
	int kind_value = lexer_peek(lexer, cur, 0);
	if (kind_value == LEXER_EOF)
	{
		return log_source_error(lexer, site, "unterminated unicode escape");
	}
	char kind = (char)kind_value;
	cur += 1;
	u32 codepoint = 0;
	Com_Error error = COM_ERROR_NONE;

	if (kind == 'u')
	{
		error = lexer_parse_hex_codepoint(lexer, &cur, 4, site, &codepoint);
		if (error != COM_ERROR_NONE)
		{
			*cursor = cur;
			return error;
		}

		if (codepoint >= 0xD800 && codepoint <= 0xDBFF)
		{
			if (lexer_peek(lexer, cur, 0) == '\\' && lexer_peek(lexer, cur, 1) == 'u')
			{
				cur += 2;
				u32 low = 0;
				error = lexer_parse_hex_codepoint(lexer, &cur, 4, site, &low);
				if (error != COM_ERROR_NONE)
				{
					*cursor = cur;
					return error;
				}
				if (low >= 0xDC00 && low <= 0xDFFF)
				{
					codepoint = 0x10000 + ((codepoint - 0xD800) << 10) + (low - 0xDC00);
				}
				else
				{
					*cursor = cur;
					return log_source_error(lexer, site, "invalid unicode surrogate pair");
				}
			}
			else
			{
				*cursor = cur;
				return log_source_error(lexer, site, "missing unicode surrogate pair");
			}
		}
		else if (codepoint >= 0xDC00 && codepoint <= 0xDFFF)
		{
			*cursor = cur;
			return log_source_error(lexer, site, "unexpected unicode low surrogate");
		}
	}
	else
	{
		error = lexer_parse_hex_codepoint(lexer, &cur, 8, site, &codepoint);
		if (error != COM_ERROR_NONE)
		{
			*cursor = cur;
			return error;
		}
	}

	if (codepoint > 0x10FFFF)
	{
		*cursor = cur;
		return log_source_error(lexer, site, "unicode codepoint out of range");
	}

	*cursor = cur;
	*out = codepoint;
	return COM_ERROR_NONE;
}

static Com_Error lex_escape_codepoint(Lexer *lexer, const char **cursor, SourceSite site, u32 *out)
{
	const char *cur = *cursor;
	int first = lexer_peek(lexer, cur, 0);
	if (first == LEXER_EOF)
	{
		return log_source_error(lexer, site, "unterminated escape sequence");
	}
	u32 codepoint = (u8)first;
	cur += 1;

	if (codepoint != '\\')
	{
		*cursor = cur;
		*out = codepoint;
		return COM_ERROR_NONE;
	}

	SourceSite escape_site = source_site_from_ptr(lexer, cur - 1);
	int escape_value = lexer_peek(lexer, cur, 0);
	if (escape_value == LEXER_EOF)
	{
		*cursor = cur;
		return log_source_error(lexer, site, "unterminated escape sequence");
	}
	char escape = (char)escape_value;
	cur += 1;
	Com_Error error = COM_ERROR_NONE;
	switch (escape)
	{
		case '"':  codepoint = '"';  break;
		case '\'': codepoint = '\''; break;
		case '\\': codepoint = '\\'; break;
		case '/':  codepoint = '/';  break;
		case '0':  codepoint = '\0'; break;
		case 'a':  codepoint = '\a'; break;
		case 'b':  codepoint = '\b'; break;
		case 'f':  codepoint = '\f'; break;
		case 'n':  codepoint = '\n'; break;
		case 'r':  codepoint = '\r'; break;
		case 't':  codepoint = '\t'; break;
		case 'v':  codepoint = '\v'; break;

		case 'x':
		{
			error = lexer_parse_hex_codepoint(lexer, &cur, 2, escape_site, &codepoint);
		} break;

		case 'u':
		case 'U':
		{
			cur -= 1;
			error = lexer_parse_unicode_escape(lexer, &cur, escape_site, &codepoint);
		} break;

		case '\0':
		{
			error = log_source_error(lexer, escape_site, "NUL byte in source");
		} break;

		default:
		{
			error = log_source_error(lexer, escape_site, "invalid escape sequence");
		} break;
	}

	*cursor = cur;
	if (error == COM_ERROR_NONE) {
		*out = codepoint;
	}
	return error;
}

static char *write_utf8(char *out, u32 codepoint)
{
	if (codepoint <= 0x7F)
	{
		*out++ = (char)codepoint;
	}
	else if (codepoint <= 0x7FF)
	{
		*out++ = (char)(0xC0 | (codepoint >> 6));
		*out++ = (char)(0x80 | (codepoint & 0x3F));
	}
	else if (codepoint <= 0xFFFF)
	{
		*out++ = (char)(0xE0 | (codepoint >> 12));
		*out++ = (char)(0x80 | ((codepoint >> 6) & 0x3F));
		*out++ = (char)(0x80 | (codepoint & 0x3F));
	}
	else
	{
		*out++ = (char)(0xF0 | (codepoint >> 18));
		*out++ = (char)(0x80 | ((codepoint >> 12) & 0x3F));
		*out++ = (char)(0x80 | ((codepoint >> 6) & 0x3F));
		*out++ = (char)(0x80 | (codepoint & 0x3F));
	}
	return out;
}

static f64 lex_fractional(Lexer *lexer, const char **cursor)
{
	const char *cur = *cursor;
	f64 value = 0;
	f64 scale = 1;

	int digit;
	while ((digit = lexer_peek(lexer, cur, 0)) >= '0' && digit <= '9') {
		value = value * 10 + (digit - '0');
		cur += 1;
		scale *= 10;
	}

	*cursor = cur;
	return value / scale;
}

static Com_Error lex_number_exponent(Lexer *lexer, const char **cursor, f64 *value, b32 *present)
{
	const char *cur = *cursor;
	int current = lexer_peek(lexer, cur, 0);
	*present = current == 'e' || current == 'E';
	if (!*present) {
		return COM_ERROR_NONE;
	}

	SourceSite site = source_site_from_ptr(lexer, cur);
	cur += 1;

	b32 negative = false;
	current = lexer_peek(lexer, cur, 0);
	if (current == '+' || current == '-')
	{
		negative = current == '-';
		cur += 1;
	}

	current = lexer_peek(lexer, cur, 0);
	if (current < '0' || current > '9')
	{
		*cursor = cur;
		return log_source_error(lexer, site, "expected digits after number exponent");
	}

	f64 exponent = 0;
	while ((current = lexer_peek(lexer, cur, 0)) >= '0' && current <= '9')
	{
		exponent = exponent * 10 + (current - '0');
		cur += 1;
	}

	if (*value != 0) {
		*value *= pow(10.0, negative ? -exponent : exponent);
	}
	*cursor = cur;
	return COM_ERROR_NONE;
}

static Com_Error lex_integer(Lexer *lexer, const char **cursor, u64 *out)
{
	const char *cur = *cursor;
	u64 base = 10;

	if (lexer_peek(lexer, cur, 0) == '0') {
		cur += 1;
		if (lexer_peek(lexer, cur, 0) == 'b') {
			cur += 1;
			base = 2;
		}
		else if (lexer_peek(lexer, cur, 0) == 'x') {
			cur += 1;
			base = 16;
		}
	}

	u64 integer = 0;
	for (;;) {
		u64 digit;
		int current = lexer_peek(lexer, cur, 0);

		if (base == 10 && (current == 'e' || current == 'E')) {
			break;
		}
		else if ('A' <= current && current <= 'Z') {
			digit = 10 + current - 'A';
			cur += 1;
		}
		else if ('a' <= current && current <= 'z') {
			digit = 10 + current - 'a';
			cur += 1;
		}
		else if ('0' <= current && current <= '9') {
			digit = current - '0';
			cur += 1;
		}
		else {
			break;
		}

		if (digit >= base) {
			goto invalid_base;
		}

		u64 max = ~(u64)0;
		if (integer > (max - digit) / base)
		{
			*cursor = cur;
			return log_source_error(lexer, source_site_from_ptr(lexer, cur - 1), "integer literal exceeds u64 range");
		}
		integer = integer * base + digit;
	}

	*cursor = cur;
	*out = integer;
	return COM_ERROR_NONE;

	invalid_base:
	*cursor = cur;
	return log_source_error(lexer, source_site_from_ptr(lexer, cur - 1), "invalid base '%llu' for digit", base);
}

static u32 lex_identifier(Lexer *lexer, const char **cursor)
{
	const char *cur = *cursor;
	const char *start = cur;

	ASSERT(is_identifier_start((char)lexer_peek(lexer, cur, 0)));
	do { cur += 1; }
	while (is_identifier_continue((char)lexer_peek(lexer, cur, 0)));

	*cursor = cur;
	return (u32)(cur - start);
}

static Com_Error lex_string_segment(Lexer *lexer, const char **cursor, SourceSite site, char quote, b32 is_block, b32 is_format, b32 *ended, Atom **out_atom)
{
	if (ended) {
		*ended = false;
	}
	*out_atom = 0;

	const char *cur = *cursor;
	elf_Scratch scratch = elf_begin_scratch_excluding(lexer->compiler->arena);
	char *text = elf_arena_push(scratch.arena, lexer_scratch_capacity(lexer));
	if (!text && lexer_scratch_capacity(lexer)) {
		elf_end_scratch(scratch);
		return COM_ERROR_OOM;
	}
	char *out = text;
	Com_Error error = COM_ERROR_NONE;

	while (error == COM_ERROR_NONE)
	{
		int current = lexer_peek(lexer, cur, 0);
		if (current == LEXER_EOF)
		{
			error = log_source_error(lexer, site, is_block ? "unterminated string block" : "unterminated string");
		}
		else if (current == 0)
		{
			error = log_source_error(lexer, source_site_from_ptr(lexer, cur), "NUL byte in source");
		}
		else if (is_block && current == quote && lexer_peek(lexer, cur, 1) == quote && lexer_peek(lexer, cur, 2) == quote)
		{
			cur += 3;
			if (ended) {
				*ended = true;
			}
			break;
		}
		else if (!is_block && current == quote)
		{
			cur += 1;
			if (ended) {
				*ended = true;
			}
			break;
		}
		else if (is_format && current == FORMAT_CHAR && lexer_peek(lexer, cur, 1) == '{')
		{
			cur += 2;
			break;
		}
		else if (!is_block && (current == '\n' || current == '\r'))
		{
			error = log_source_error(lexer, source_site_from_ptr(lexer, cur), "newline in string; use \\n or a string block");
		}
		else if (current == '\r')
		{
			cur += 1;
			if (lexer_peek(lexer, cur, 0) == '\n') {
				cur += 1;
			}
			lexer_advance_line(lexer, cur);
			*out++ = '\n';
		}
		else if (current == '\n') {
			cur += 1;
			lexer_advance_line(lexer, cur);
			*out++ = '\n';
		}
		else if (current == '\\')
		{
			u32 codepoint = 0;
			error = lex_escape_codepoint(lexer, &cur, site, &codepoint);
			if (error == COM_ERROR_NONE) {
				out = write_utf8(out, codepoint);
			}
		}
		else
		{
			*out++ = (char)current;
			cur += 1;
		}
	}

	if (error == COM_ERROR_NONE)
	{
		*out_atom = atom_from_data_size(lexer->atoms, text, (u32)(out - text));
		if (!*out_atom) {
			error = COM_ERROR_OOM;
		}
	}
	elf_end_scratch(scratch);
	*cursor = cur;
	return error;
}

static Com_Error lexer_skip_trivia(Lexer *lexer, const char **cursor, b32 *line_break)
{
	const char *cur = *cursor;
	*line_break = false;
	Com_Error error = COM_ERROR_NONE;

	while (error == COM_ERROR_NONE)
	{
		int current = lexer_peek(lexer, cur, 0);
		if (current == ' ' || current == '\t')
		{
			cur += 1;
		}
		else if (current == '\n')
		{
			cur += 1;
			lexer_advance_line(lexer, cur);
			*line_break = true;
		}
		else if (current == '\r')
		{
			cur += 1;
			if (lexer_peek(lexer, cur, 0) == '\n') {
				cur += 1;
			}
			lexer_advance_line(lexer, cur);
			*line_break = true;
		}
		else if (current == '/' && lexer_peek(lexer, cur, 1) == '/')
		{
			cur += 2;
			while ((current = lexer_peek(lexer, cur, 0)) != LEXER_EOF && current != '\n' && current != '\r') {
				cur += 1;
			}
		}
		else if (current == '/' && lexer_peek(lexer, cur, 1) == '*')
		{
			SourceSite site = source_site_from_ptr(lexer, cur);
			cur += 2;
			for (;;)
			{
				current = lexer_peek(lexer, cur, 0);
				if (current == LEXER_EOF)
				{
					error = log_source_error(lexer, site, "unterminated comment");
					break;
				}
				if (current == '*' && lexer_peek(lexer, cur, 1) == '/')
				{
					cur += 2;
					break;
				}
				if (current == '\n')
				{
					cur += 1;
					lexer_advance_line(lexer, cur);
					*line_break = true;
				}
				else if (current == '\r')
				{
					cur += 1;
					if (lexer_peek(lexer, cur, 0) == '\n') {
						cur += 1;
					}
					lexer_advance_line(lexer, cur);
					*line_break = true;
				}
				else
				{
					cur += 1;
				}
			}
		}
		else
		{
			break;
		}
	}

	*cursor = cur;
	return error;
}

// TODO(RJ): proper allocation!
static Com_Error lexer_tokens_push(Lexer_TokenFIFO *tokens, elf_Arena *arena, Token token)
{
	if (tokens->count >= tokens->capacity)
	{
		u32 capacity = tokens->capacity ? tokens->capacity * 2 : 16;
		Token *items = elf_arena_push(arena, sizeof(*items) * capacity);
		if (!items) {
			return COM_ERROR_OOM;
		}
		if (tokens->count) {
			copy_memory(items, tokens->items, sizeof(*items) * tokens->count);
		}
		tokens->items = items;
		tokens->capacity = capacity;
	}
	tokens->items[tokens->count++] = token;
	return COM_ERROR_NONE;
}

static Token lex_token(Lexer *lexer);

static Com_Error lexer_format_string(Lexer *lexer, const char **cursor, Token opening, char quote, b32 is_block)
{
	Lexer worker = *lexer;
	worker.cursor = *cursor;
	worker.tokens = (Lexer_TokenFIFO){0};

	u32 interpolation_count = 0;
	SourceSite part_site = opening.site;
	b32 line_break_before = opening.line_break_before;
	Com_Error error = COM_ERROR_NONE;

	while (error == COM_ERROR_NONE)
	{
		const char *cur = worker.cursor;
		b32 ended = false;
		Atom *atom = 0;
		error = lex_string_segment(&worker, &cur, opening.site, quote, is_block, true, &ended, &atom);
		worker.cursor = cur;
		if (error != COM_ERROR_NONE) {
			break;
		}

		Token part = {
			.type = ended
				? (interpolation_count ? TOK_STRING_END : TOK_STRING)
				: (interpolation_count ? TOK_STRING_PART : TOK_STRING_START),
			.site = part_site,
			.line_break_before = line_break_before,
			.atom = atom,
		};
		part.site.size = (u32)(cur - (worker.source.data + part.site.offset));
		if (part.site.size == 0) {
			part.site.size = 1;
		}
		error = lexer_tokens_push(&lexer->tokens, lexer->atoms->arena, part);
		if (error != COM_ERROR_NONE) {
			break;
		}

		if (ended || cur >= worker.end) {
			break;
		}
		interpolation_count += 1;

		u32 brace_depth = 0;
		Token closing = {0};
		for (;;)
		{
			Token token = lex_token(&worker);
			if (token_is_error(token))
			{
				error = token.error;
				break;
			}
			if (token.type == TOK_NONE)
			{
				error = log_source_error(&worker, opening.site, "unterminated interpolated expression");
				break;
			}

			if (token.type == TOK_RIGHT_BRACE && brace_depth == 0)
			{
				closing = token;
				break;
			}

			error = lexer_tokens_push(&lexer->tokens, lexer->atoms->arena, token);
			if (error != COM_ERROR_NONE) {
				break;
			}
			if (token.type == TOK_LEFT_BRACE) {
				brace_depth += 1;
			}
			else if (token.type == TOK_RIGHT_BRACE) {
				brace_depth -= 1;
			}
		}

		if (error != COM_ERROR_NONE || closing.type == TOK_NONE) {
			break;
		}
		part_site = closing.site;
		line_break_before = closing.line_break_before;
	}

	*cursor = worker.cursor;
	lexer->cursor = worker.cursor;
	lexer->line_index = worker.line_index;
	lexer->line_start = worker.line_start;

	if (error != COM_ERROR_NONE)
	{
		lexer->tokens.index = 0;
		lexer->tokens.count = 0;
	}
	else
	{
		ASSERT(lexer->tokens.count);
	}
	return error;
}

static Com_Error lex(Lexer *lexer)
{
	ASSERT(lexer->tokens.index == lexer->tokens.count);
	lexer->tokens.index = 0;
	lexer->tokens.count = 0;

	const char *cur = lexer->cursor;
	b32 line_break_before = false;
	Com_Error error = lexer_skip_trivia(lexer, &cur, &line_break_before);
	Token token = {0};
	token.type = TOK_NONE;
	token.site = lexer_source_site(lexer, cur);
	token.line_break_before = line_break_before;
	if (error != COM_ERROR_NONE) {
		lexer->cursor = cur;
		return error;
	}

	switch (lexer_peek(lexer, cur, 0))
	{
		case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G':
		case 'H': case 'I': case 'J': case 'K': case 'L': case 'M': case 'N':
		case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T': case 'U':
		case 'V': case 'W': case 'X': case 'Y': case 'Z':
		case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g':
		case 'h': case 'i': case 'j': case 'k': case 'l': case 'm': case 'n':
		case 'o': case 'p': case 'q': case 'r': case 's': case 't': case 'u':
		case 'v': case 'w': case 'x': case 'y': case 'z':
		case '_':
		{
			u32 quote = lexer_peek(lexer, cur, 1);
			if (lexer_peek(lexer, cur, 0) == 'f' && (quote == '"' || quote == '`'))
			{
				b32 is_block = lexer_peek(lexer, cur, 2) == quote && lexer_peek(lexer, cur, 3) == quote;
				cur += 2 + is_block * 2;
				error = lexer_format_string(lexer, &cur, token, (char)quote, is_block);
				return error;
			}
			else
			{
				const char *start = cur;
				u32 size = lex_identifier(lexer, &cur);
				Atom *atom = atom_from_data_size(lexer->atoms, start, size);
				if (!atom)
				{
					error = COM_ERROR_OOM;
				}
				else
				{
					token.type = check_keyword(atom);
					// Keep the spelling for keywords as well. Keywords remain reserved in
					// expression position, but may be used as field names after '.'.
					token.atom = atom;
				}
			}
		} break;

		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
		{
			b32 is_decimal = !(lexer_peek(lexer, cur, 0) == '0' && (lexer_peek(lexer, cur, 1) == 'b' || lexer_peek(lexer, cur, 1) == 'x'));
			u64 value = 0;
			error = lex_integer(lexer, &cur, &value);
			if (error != COM_ERROR_NONE) {
				break;
			}

			token.type = TOK_INTEGER;
			token.integer_magnitude = value;

			if (is_decimal && lexer_peek(lexer, cur, 0) == '.' && lexer_peek(lexer, cur, 1) != '.')
			{
				cur += 1;
				token.type = TOK_NUMBER;
				token.number = (f64)value + lex_fractional(lexer, &cur);
			}
			if (is_decimal)
			{
				f64 number = token.type == TOK_NUMBER ? token.number : (f64)value;
				b32 has_exponent = false;
				error = lex_number_exponent(lexer, &cur, &number, &has_exponent);
				if (error == COM_ERROR_NONE && has_exponent)
				{
					token.type = TOK_NUMBER;
					token.number = number;
				}
			}
		} break;

		case '\'':
		{
			cur += 1;
			token.type = TOK_LETTER;
			u32 codepoint = 0;
			error = lex_escape_codepoint(lexer, &cur, token.site, &codepoint);
			if (error != COM_ERROR_NONE) {
				break;
			}
			token.integer_magnitude = codepoint;

			if (lexer_peek(lexer, cur, 0) == '\'') {
				cur += 1;
			}
			else {
				error = log_source_error(lexer, token.site, "invalid character constant, expected \"'\"");
			}
		} break;

		case '`':
		case '"':
		{
			char quote = lexer_peek(lexer, cur, 0);
			b32 is_block = lexer_peek(lexer, cur, 1) == quote && lexer_peek(lexer, cur, 2) == quote;
			cur += is_block ? 3 : 1;
			error = lex_string_segment(lexer, &cur, token.site, quote, is_block, false, 0, &token.atom);
			if (error == COM_ERROR_NONE) {
				token.type = TOK_STRING;
			}
		} break;

		case '#':
		{
			cur += 1;

			if (!is_identifier_start((char)lexer_peek(lexer, cur, 0)))
			{
				error = log_source_error(lexer, token.site, "expected macro name after '#'");
			}
			else
			{
				lex_identifier(lexer, &cur);
				const char *token_data = lexer->source.data + token.site.offset;
				u32 size = (u32)(cur - token_data);
				Atom *atom = atom_from_data_size(lexer->atoms, token_data, size);
				if (!atom)
				{
					error = COM_ERROR_OOM;
				}
				else
				{
					token.type = atom_is_word_or_macro(atom);
					if (token.type == TOK_M_ENDOFFILE) {
						token.type = TOK_NONE;
					}
					else if (token.type == TOK_M_FILE_NAME) {
						token.type = TOK_STRING;
						token.atom = lexer->compiler->modules[lexer->module].name;
					}
					else if (token.type == TOK_M_LINE_NUMBER) {
						token.type = TOK_INTEGER;
						token.integer_magnitude = lexer->line_index;
					}
					else if (token.type == TOK_IDENTIFIER) {
						error = log_source_error(lexer, token.site, "unrecognized macro");
					}
				}
			}
		} break;

		case LEXER_EOF:
		{
			token.type = TOK_NONE;
		} break;

		case '\0':
		{
			error = log_source_error(lexer, token.site, "NUL byte in source");
			cur += 1;
		} break;

		case '[': token.type = TOK_SQUARE_LEFT;  cur += 1; break;
		case ']': token.type = TOK_SQUARE_RIGHT; cur += 1; break;
		case '(': token.type = TOK_LEFT_PAREN;   cur += 1; break;
		case ')': token.type = TOK_PAREN_RIGHT;  cur += 1; break;
		case ',': token.type = TOK_COMMA;        cur += 1; break;

		case '{': token.type = TOK_LEFT_BRACE;  cur += 1; break;
		case '}': token.type = TOK_RIGHT_BRACE; cur += 1; break;

		case '%':
		{
			cur += 1;
			token.type = TOK_MOD;
			if (lexer_peek(lexer, cur, 0) == '=') {
				cur += 1;
				token.type = TOK_MOD_ASSIGN;
			}
		} break;
		case '^':
		{
			cur += 1;
			token.type = TOK_BIT_XOR;
			if (lexer_peek(lexer, cur, 0) == '=') {
				cur += 1;
				token.type = TOK_XOR_ASSIGN;
			}
		} break;
		case '~': token.type = TOK_TILDE;     cur += 1; break;
		case ';': token.type = TOK_SEMICOLON; cur += 1; break;

		case '/':
		{
			cur += 1;
			token.type = TOK_DIV;

			if (lexer_peek(lexer, cur, 0) == '=') {
				cur += 1;
				token.type = TOK_DIV_ASSIGN;
			}
		} break;

		case '.':
		{
			cur += 1;
			token.type = TOK_DOT;

			if (lexer_peek(lexer, cur, 0) == '.') {
				cur += 1;
				token.type = TOK_ELLIPSIS;
				if (lexer_peek(lexer, cur, 0) == '.') {
					cur += 1;
				}
			}
			else if (lexer_peek(lexer, cur, 0) >= '0' && lexer_peek(lexer, cur, 0) <= '9') {
				token.type = TOK_NUMBER;
				token.number = lex_fractional(lexer, &cur);
				b32 has_exponent = false;
				error = lex_number_exponent(lexer, &cur, &token.number, &has_exponent);
			}
		} break;

		case '<':
		{
			cur += 1;
			token.type = TOK_LT;
			if (lexer_peek(lexer, cur, 0) == '=') {
				cur += 1;
				token.type = TOK_LTEQ;
			}
			else if (lexer_peek(lexer, cur, 0) == '<') {
				cur += 1;
				token.type = TOK_SHL;
				if (lexer_peek(lexer, cur, 0) == '=') {
					cur += 1;
					token.type = TOK_SHL_ASSIGN;
				}
			}
		} break;

		case ':':
		{
			cur += 1;
			token.type = TOK_COLON;
			if (lexer_peek(lexer, cur, 0) == ':') {
				cur += 1;
				token.type = TOK_STATIC_BIND;
				if (lexer_peek(lexer, cur, 0) == '=') {
					cur += 1;
					token.type = TOK_HARD_BIND;
				}
			}
			else if (lexer_peek(lexer, cur, 0) == '=') {
				cur += 1;
				token.type = TOK_BIND;
			}
		} break;

		case '-':
		{
			cur += 1;
			token.type = TOK_SUB;
			if (lexer_peek(lexer, cur, 0) == '=') {
				cur += 1;
				token.type = TOK_SUB_ASSIGN;
			}
			else if (lexer_peek(lexer, cur, 0) == '-') {
				cur += 1;
				token.type = TOK_MINUS_MINUS;
			}
			else if (lexer_peek(lexer, cur, 0) == '>') {
				cur += 1;
				token.type = TOK_ARROW;
			}
		} break;

		case '>':
		{
			cur += 1;
			token.type = TOK_GT;
			if (lexer_peek(lexer, cur, 0) == '=') {
				cur += 1;
				token.type = TOK_GTEQ;
			}
			else if (lexer_peek(lexer, cur, 0) == '>') {
				cur += 1;
				token.type = TOK_SHR;
				if (lexer_peek(lexer, cur, 0) == '=') {
					cur += 1;
					token.type = TOK_SHR_ASSIGN;
				}
			}
		} break;

		case '?':
		{
			cur += 1;
			token.type = TOK_QMARK;
			if (lexer_peek(lexer, cur, 0) == '?') {
				cur += 1;
				token.type = TOK_NIL_OR;
			}
			else if (lexer_peek(lexer, cur, 0) == '=') {
				cur += 1;
				token.type = TOK_NIL_ASSIGN;
			}
		} break;

		case '!':
		{
			cur += 1;
			token.type = TOK_EXCLAMATION_MARK;
			if (lexer_peek(lexer, cur, 0) == '!') {
				cur += 1;
				token.type = TOK_NIL_AND;
			}
			else if (lexer_peek(lexer, cur, 0) == '=') {
				cur += 1;
				token.type = TOK_NEQ;
			}
		} break;

		case '|':
		{
			cur += 1;
			token.type = TOK_BIT_OR;
			if (lexer_peek(lexer, cur, 0) == '|') {
				cur += 1;
				token.type = TOK_LOG_OR;
			}
		} break;

		case '&':
		{
			cur += 1;
			token.type = TOK_BIT_AND;
			if (lexer_peek(lexer, cur, 0) == '&') {
				cur += 1;
				token.type = TOK_LOG_AND;
			}
		} break;

		case '=':
		{
			cur += 1;
			token.type = TOK_ASSIGN;
			if (lexer_peek(lexer, cur, 0) == '=') {
				cur += 1;
				token.type = TOK_EQ;
			}
			else if (lexer_peek(lexer, cur, 0) == '>') {
				cur += 1;
				token.type = TOK_RET;
			}
		} break;

		case '*':
		{
			cur += 1;
			token.type = TOK_MUL;
			if (lexer_peek(lexer, cur, 0) == '=') {
				token.type = TOK_MUL_ASSIGN;
				cur += 1;
			}
			else if (lexer_peek(lexer, cur, 0) == '*') {
				token.type = TOK_POW;
				cur += 1;
			}
		} break;

		// snuck
		case '+':
		{
			cur += 1;
			token.type = TOK_ADD;
			if (lexer_peek(lexer, cur, 0) == '=') {
				cur += 1;
				token.type = TOK_ADD_ASSIGN;
			}
		} break;

		default:
		{
			error = log_source_error(lexer, token.site, "unexpected character");
			cur += 1;
		} break;
	}

	token.site.size = (u32)(cur - (lexer->source.data + token.site.offset));
	if (token.site.size == 0) {
		token.site.size = 1;
	}

	lexer->cursor = cur;
	if (error == COM_ERROR_NONE) {
		error = lexer_tokens_push(&lexer->tokens, lexer->atoms->arena, token);
	}
	return error;
}

static Token lex_token(Lexer *lexer)
{
	if (lexer->tokens.index == lexer->tokens.count)
	{
		SourceSite site = lexer_source_site(lexer, lexer->cursor);
		Com_Error error = lex(lexer);
		if (error != COM_ERROR_NONE) {
			return token_from_error(error, site);
		}
	}
	ASSERT(lexer->tokens.index < lexer->tokens.count);
	return lexer->tokens.items[lexer->tokens.index++];
}
