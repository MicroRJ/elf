//
// See Copyright Notice In elf.h
//

#include <math.h>

#include "source_diagnostics.c"

typedef struct
{
	Compiler    *compiler;
	Com_Source  *compiler_source;
	Atom_Table  *atoms;
	Arena       *scratch_arena;
	Token_Array *tokens;
	const char  *cursor;
	const char  *end;
	u32          line_index;
	const char  *line_start;
}
Lexer;

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
	return lexer->compiler_source->source.size;
}

enum { LEXER_EOF = -1 };

static inline int lexer_peek(Lexer *lexer, u32 offset)
{
	ASSERT(lexer->cursor >= lexer->compiler_source->source.data && lexer->cursor <= lexer->end);
	u64 remaining = (u64)(lexer->end - lexer->cursor);
	return offset < remaining ? (u8)lexer->cursor[offset] : LEXER_EOF;
}

static b32 is_identifier_start(u32 c)
{
	return ('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z') || c == '_';
}

static b32 is_identifier_continue(u32 c)
{
	return is_identifier_start(c) || ('0' <= c && c <= '9');
}

static b32 is_hexadecimal(u32 c)
{
	return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static void lexer_advance_line(Lexer *lexer)
{
	lexer->line_index += 1;
	lexer->line_start = lexer->cursor;
}

static SourceSite source_site_from_ptr(Lexer *lexer, const char *data)
{
	ASSERT(data);
	ASSERT(data >= lexer->line_start);
	ASSERT(data <= lexer->end);
	u64 offset = (u64)(data - lexer->compiler_source->source.data);
	u64 line_offset = (u64)(lexer->line_start - lexer->compiler_source->source.data);
	ASSERT(offset <= UINT32_MAX);
	ASSERT(line_offset <= UINT32_MAX);
	return (SourceSite) {
		.source      = lexer->compiler_source->id,
		.offset      = (u32) offset,
		.size        = 1,
		.line_offset = (u32) line_offset,
		.line_index  = lexer->line_index,
	};
}

static SourceSite lexer_source_site(Lexer *lexer)
{
	return source_site_from_ptr(lexer, lexer->cursor);
}

static Com_Status lexer_errorf(Lexer *lexer, ErrorCode error, SourceSite site, char const *format, ...)
{
	va_list args;
	va_start(args, format);
	b32 reported = compiler_reportv(lexer->compiler, error, ERROR_ERROR, ERROR_PHASE_LEXER, site, format, args);
	va_end(args);
	return reported ? COM_STATUS_REPORTED : COM_STATUS_OOM;
}

static u32 lexer_hex_digit(u32 c)
{
	if (c >= '0' && c <= '9') return  0 + (u32)(c - '0');
	if (c >= 'a' && c <= 'f') return 10 + (u32)(c - 'a');
	return 10 + (u32)(c - 'A');
}

static Com_Status lexer_parse_hex_codepoint(Lexer *lexer, u32 digits, SourceSite site, u32 *out)
{
	u32 codepoint = 0;
	for (u32 i = 0; i < digits; ++i)
	{
		int digit = lexer_peek(lexer, i);
		if (digit == LEXER_EOF || !is_hexadecimal((char)digit))
		{
			return lexer_errorf(lexer, ERROR_INVALID_UNICODE_ESCAPE, site, "invalid unicode escape");
		}
		codepoint = (codepoint << 4) | lexer_hex_digit((char)digit);
	}

	lexer->cursor += digits;
	*out = codepoint;
	return COM_STATUS_OK;
}

static Com_Status lexer_parse_unicode_escape(Lexer *lexer, SourceSite site, u32 *out)
{
	int kind_value = lexer_peek(lexer, 0);
	if (kind_value == LEXER_EOF)
	{
		return lexer_errorf(lexer, ERROR_INVALID_UNICODE_ESCAPE, site, "unterminated unicode escape");
	}
	char kind = (char)kind_value;
	lexer->cursor += 1;
	u32 codepoint = 0;
	Com_Status error = COM_STATUS_OK;

	if (kind == 'u')
	{
		error = lexer_parse_hex_codepoint(lexer, 4, site, &codepoint);
		if (error != COM_STATUS_OK) {
			return error;
		}

		if (codepoint >= 0xD800 && codepoint <= 0xDBFF)
		{
			if (lexer_peek(lexer, 0) == '\\' && lexer_peek(lexer, 1) == 'u')
			{
				lexer->cursor += 2;
				u32 low = 0;
				error = lexer_parse_hex_codepoint(lexer, 4, site, &low);
				if (error != COM_STATUS_OK) {
					return error;
				}
				if (low >= 0xDC00 && low <= 0xDFFF)
				{
					codepoint = 0x10000 + ((codepoint - 0xD800) << 10) + (low - 0xDC00);
				}
				else
				{
					return lexer_errorf(lexer, ERROR_INVALID_UNICODE_ESCAPE, site, "invalid unicode surrogate pair");
				}
			}
			else
			{
				return lexer_errorf(lexer, ERROR_INVALID_UNICODE_ESCAPE, site, "missing unicode surrogate pair");
			}
		}
		else if (codepoint >= 0xDC00 && codepoint <= 0xDFFF)
		{
			return lexer_errorf(lexer, ERROR_INVALID_UNICODE_ESCAPE, site, "unexpected unicode low surrogate");
		}
	}
	else
	{
		error = lexer_parse_hex_codepoint(lexer, 8, site, &codepoint);
		if (error != COM_STATUS_OK) {
			return error;
		}
	}

	if (codepoint > 0x10FFFF)
	{
		return lexer_errorf(lexer, ERROR_INVALID_UNICODE_ESCAPE, site, "unicode codepoint out of range");
	}

	*out = codepoint;
	return COM_STATUS_OK;
}

static Com_Status lex_escape_codepoint(Lexer *lexer, SourceSite site, u32 *out)
{
	int first = lexer_peek(lexer, 0);
	if (first == LEXER_EOF)
	{
		return lexer_errorf(lexer, ERROR_UNTERMINATED_ESCAPE_SEQUENCE, site, "unterminated escape sequence");
	}
	u32 codepoint = (u8)first;
	lexer->cursor += 1;

	if (codepoint != '\\')
	{
		*out = codepoint;
		return COM_STATUS_OK;
	}

	SourceSite escape_site = source_site_from_ptr(lexer, lexer->cursor - 1);
	int escape_value = lexer_peek(lexer, 0);
	if (escape_value == LEXER_EOF)
	{
		return lexer_errorf(lexer, ERROR_UNTERMINATED_ESCAPE_SEQUENCE, site, "unterminated escape sequence");
	}
	char escape = (char)escape_value;
	lexer->cursor += 1;
	Com_Status error = COM_STATUS_OK;
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
			error = lexer_parse_hex_codepoint(lexer, 2, escape_site, &codepoint);
		} break;

		case 'u':
		case 'U':
		{
			lexer->cursor -= 1;
			error = lexer_parse_unicode_escape(lexer, escape_site, &codepoint);
		} break;

		case '\0':
		{
			error = lexer_errorf(lexer, ERROR_NUL_BYTE_IN_SOURCE, escape_site, "NUL byte in source");
		} break;

		default:
		{
			error = lexer_errorf(lexer, ERROR_INVALID_ESCAPE_SEQUENCE, escape_site, "invalid escape sequence");
		} break;
	}

	if (error == COM_STATUS_OK) {
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

static f64 lex_fractional(Lexer *lexer)
{
	f64 value = 0;
	f64 scale = 1;

	int digit;
	while ((digit = lexer_peek(lexer, 0)) >= '0' && digit <= '9') {
		value = value * 10 + (digit - '0');
		lexer->cursor += 1;
		scale *= 10;
	}

	return value / scale;
}

static Com_Status lex_number_exponent(Lexer *lexer, f64 *value, b32 *present)
{
	int current = lexer_peek(lexer, 0);
	*present = current == 'e' || current == 'E';
	if (!*present) {
		return COM_STATUS_OK;
	}

	SourceSite site = lexer_source_site(lexer);
	lexer->cursor += 1;

	b32 negative = false;
	current = lexer_peek(lexer, 0);
	if (current == '+' || current == '-')
	{
		negative = current == '-';
		lexer->cursor += 1;
	}

	current = lexer_peek(lexer, 0);
	if (current < '0' || current > '9')
	{
		return lexer_errorf(lexer, ERROR_INVALID_NUMBER_LITERAL, site, "expected digits after number exponent");
	}

	f64 exponent = 0;
	while ((current = lexer_peek(lexer, 0)) >= '0' && current <= '9')
	{
		exponent = exponent * 10 + (current - '0');
		lexer->cursor += 1;
	}

	if (*value != 0) {
		*value *= pow(10.0, negative ? -exponent : exponent);
	}
	return COM_STATUS_OK;
}

static Com_Status lex_integer(Lexer *lexer, u64 *out)
{
	u64 base = 10;

	if (lexer_peek(lexer, 0) == '0') {
		lexer->cursor += 1;
		if (lexer_peek(lexer, 0) == 'b') {
			lexer->cursor += 1;
			base = 2;
		}
		else if (lexer_peek(lexer, 0) == 'x') {
			lexer->cursor += 1;
			base = 16;
		}
	}

	u64 integer = 0;
	for (;;) {
		u64 digit;
		int current = lexer_peek(lexer, 0);

		if (base == 10 && (current == 'e' || current == 'E')) {
			break;
		}
		else if ('A' <= current && current <= 'Z') {
			digit = 10 + current - 'A';
			lexer->cursor += 1;
		}
		else if ('a' <= current && current <= 'z') {
			digit = 10 + current - 'a';
			lexer->cursor += 1;
		}
		else if ('0' <= current && current <= '9') {
			digit = current - '0';
			lexer->cursor += 1;
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
			return lexer_errorf(lexer, ERROR_INTEGER_LITERAL_OVERFLOW, source_site_from_ptr(lexer, lexer->cursor - 1), "integer literal exceeds u64 range");
		}
		integer = integer * base + digit;
	}

	*out = integer;
	return COM_STATUS_OK;

	invalid_base:
	return lexer_errorf(lexer, ERROR_INVALID_NUMBER_LITERAL, source_site_from_ptr(lexer, lexer->cursor - 1), "invalid base '%llu' for digit", base);
}

static u32 lex_identifier_size(Lexer *lexer)
{
	const char *start = lexer->cursor;

	ASSERT(is_identifier_start((char)lexer_peek(lexer, 0)));
	do { lexer->cursor += 1; }
	while (is_identifier_continue((char)lexer_peek(lexer, 0)));
	return (u32)(lexer->cursor - start);
}

static Atom *lex_identifier(Lexer *lexer)
{
	const char *start = lexer->cursor;
	u32 size = lex_identifier_size(lexer);

	return atom_from_data_size(lexer->atoms, start, size);
}

static Com_Status lex_string_segment(Lexer *lexer, SourceSite site, char quote, b32 is_block, b32 is_format, b32 *ended, Atom **out_atom)
{
	if (ended) {
		*ended = false;
	}
	*out_atom = 0;

	u64 scratch_mark = arena_mark(lexer->scratch_arena);
	char *text = arena_push(lexer->scratch_arena, lexer_scratch_capacity(lexer), 1);
	if (!text && lexer_scratch_capacity(lexer)) {
		arena_restore(lexer->scratch_arena, scratch_mark);
		return COM_STATUS_OOM;
	}
	char *out = text;
	Com_Status error = COM_STATUS_OK;

	while (error == COM_STATUS_OK)
	{
		int current = lexer_peek(lexer, 0);
		if (current == LEXER_EOF)
		{
			error = lexer_errorf(lexer, ERROR_UNTERMINATED_STRING, site, is_block ? "unterminated string block" : "unterminated string");
		}
		else if (current == 0)
		{
			error = lexer_errorf(lexer, ERROR_NUL_BYTE_IN_SOURCE, lexer_source_site(lexer), "NUL byte in source");
		}
		else if (is_block && current == quote && lexer_peek(lexer, 1) == quote && lexer_peek(lexer, 2) == quote)
		{
			lexer->cursor += 3;
			if (ended) {
				*ended = true;
			}
			break;
		}
		else if (!is_block && current == quote)
		{
			lexer->cursor += 1;
			if (ended) {
				*ended = true;
			}
			break;
		}
		else if (is_format && current == FORMAT_CHAR && lexer_peek(lexer, 1) == '{')
		{
			lexer->cursor += 2;
			break;
		}
		else if (!is_block && (current == '\n' || current == '\r'))
		{
			error = lexer_errorf(lexer, ERROR_NEWLINE_IN_STRING, lexer_source_site(lexer), "newline in string; use \\n or a string block");
		}
		else if (current == '\r')
		{
			lexer->cursor += 1;
			if (lexer_peek(lexer, 0) == '\n') {
				lexer->cursor += 1;
			}
			lexer_advance_line(lexer);
			*out++ = '\n';
		}
		else if (current == '\n') {
			lexer->cursor += 1;
			lexer_advance_line(lexer);
			*out++ = '\n';
		}
		else if (current == '\\')
		{
			u32 codepoint = 0;
			error = lex_escape_codepoint(lexer, site, &codepoint);
			if (error == COM_STATUS_OK) {
				out = write_utf8(out, codepoint);
			}
		}
		else
		{
			*out++ = (char)current;
			lexer->cursor += 1;
		}
	}

	if (error == COM_STATUS_OK)
	{
		*out_atom = atom_from_data_size(lexer->atoms, text, (u32)(out - text));
		if (!*out_atom) {
			error = COM_STATUS_OOM;
		}
	}
	arena_restore(lexer->scratch_arena, scratch_mark);
	return error;
}

static Com_Status lexer_skip_trivia(Lexer *lexer, b32 *line_break)
{
	*line_break = false;
	Com_Status error = COM_STATUS_OK;

	while (error == COM_STATUS_OK)
	{
		int current = lexer_peek(lexer, 0);
		if (current == ' ' || current == '\t')
		{
			lexer->cursor += 1;
		}
		else if (current == '\n')
		{
			lexer->cursor += 1;
			lexer_advance_line(lexer);
			*line_break = true;
		}
		else if (current == '\r')
		{
			lexer->cursor += 1;
			if (lexer_peek(lexer, 0) == '\n') {
				lexer->cursor += 1;
			}
			lexer_advance_line(lexer);
			*line_break = true;
		}
		else if (current == '/' && lexer_peek(lexer, 1) == '/')
		{
			lexer->cursor += 2;
			while ((current = lexer_peek(lexer, 0)) != LEXER_EOF && current != '\n' && current != '\r') {
				lexer->cursor += 1;
			}
		}
		else if (current == '/' && lexer_peek(lexer, 1) == '*')
		{
			SourceSite site = lexer_source_site(lexer);
			lexer->cursor += 2;
			for (;;)
			{
				current = lexer_peek(lexer, 0);
				if (current == LEXER_EOF)
				{
					error = lexer_errorf(lexer, ERROR_UNTERMINATED_COMMENT, site, "unterminated comment");
					break;
				}
				if (current == '*' && lexer_peek(lexer, 1) == '/')
				{
					lexer->cursor += 2;
					break;
				}
				if (current == '\n')
				{
					lexer->cursor += 1;
					lexer_advance_line(lexer);
					*line_break = true;
				}
				else if (current == '\r')
				{
					lexer->cursor += 1;
					if (lexer_peek(lexer, 0) == '\n') {
						lexer->cursor += 1;
					}
					lexer_advance_line(lexer);
					*line_break = true;
				}
				else
				{
					lexer->cursor += 1;
				}
			}
		}
		else
		{
			break;
		}
	}

	return error;
}

static Com_Status token_array_push(Token_Array *tokens, Arena *arena, Token token)
{
	if (tokens->count >= tokens->capacity)
	{
		u32 capacity = tokens->capacity ? tokens->capacity * 2 : 16;
		Token *items = arena_push(arena, sizeof(*items) * capacity, _Alignof(Token));
		if (!items) {
			return COM_STATUS_OOM;
		}
		if (tokens->count) {
			copy_memory(items, tokens->items, sizeof(*items) * tokens->count);
		}
		tokens->items = items;
		tokens->capacity = capacity;
	}
	tokens->items[tokens->count++] = token;
	return COM_STATUS_OK;
}

static Com_Status lexer_emit_token(Lexer *lexer, Token *emitted);

static Com_Status lexer_format_string(Lexer *lexer, Token opening, char quote, b32 is_block)
{
	u32 token_start = lexer->tokens->count;
	u32 interpolation_count = 0;
	SourceSite part_site = opening.site;
	b32 line_break_before = opening.line_break_before;
	Com_Status error = COM_STATUS_OK;

	while (error == COM_STATUS_OK)
	{
		b32 ended = false;
		Atom *atom = 0;
		error = lex_string_segment(lexer, opening.site, quote, is_block, true, &ended, &atom);
		if (error != COM_STATUS_OK) {
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
		part.site.size = (u32)(lexer->cursor - (lexer->compiler_source->source.data + part.site.offset));
		if (part.site.size == 0) {
			part.site.size = 1;
		}
		error = token_array_push(lexer->tokens, lexer->scratch_arena, part);
		if (error != COM_STATUS_OK) {
			break;
		}

		if (ended || lexer->cursor >= lexer->end) {
			break;
		}
		interpolation_count += 1;

		u32 brace_depth = 0;
		Token closing = {0};
		for (;;)
		{
			u32 emitted_start = lexer->tokens->count;
			Token token = {};
			error = lexer_emit_token(lexer, &token);
			if (error != COM_STATUS_OK) {
				break;
			}
			if (token.type == TOK_NONE)
			{
				error = lexer_errorf(lexer, ERROR_UNTERMINATED_INTERPOLATION, opening.site, "unterminated interpolated expression");
				break;
			}

			if (token.type == TOK_RIGHT_BRACE && brace_depth == 0)
			{
				ASSERT(lexer->tokens->count == emitted_start + 1);
				lexer->tokens->count = emitted_start;
				closing = token;
				break;
			}
			if (token.type == TOK_LEFT_BRACE) {
				brace_depth += 1;
			}
			else if (token.type == TOK_RIGHT_BRACE) {
				brace_depth -= 1;
			}
		}

		if (error != COM_STATUS_OK || closing.type == TOK_NONE) {
			break;
		}
		part_site = closing.site;
		line_break_before = closing.line_break_before;
	}

	if (error != COM_STATUS_OK)
	{
		lexer->tokens->count = token_start;
	}
	else
	{
		ASSERT(lexer->tokens->count > token_start);
	}
	return error;
}

static Com_Status lexer_emit_token(Lexer *lexer, Token *emitted)
{
	ASSERT(emitted);
	b32 line_break_before = false;
	Com_Status error = lexer_skip_trivia(lexer, &line_break_before);
	Token token = {0};
	token.type = TOK_NONE;
	token.site = lexer_source_site(lexer);
	token.line_break_before = line_break_before;
	if (error != COM_STATUS_OK) {
		return error;
	}

	switch (lexer_peek(lexer, 0))
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
			u32 quote = lexer_peek(lexer, 1);
			if (lexer_peek(lexer, 0) == 'f' && (quote == '"' || quote == '`'))
			{
				b32 is_block = lexer_peek(lexer, 2) == quote && lexer_peek(lexer, 3) == quote;
				lexer->cursor += 2 + is_block * 2;
				u32 token_start = lexer->tokens->count;
				error = lexer_format_string(lexer, token, (char)quote, is_block);
				if (error == COM_STATUS_OK) {
					*emitted = lexer->tokens->items[token_start];
				}
				return error;
			}
			else
			{
				Atom *atom = lex_identifier(lexer);
				if (!atom)
				{
					error = COM_STATUS_OOM;
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
			b32 is_decimal = !(lexer_peek(lexer, 0) == '0' && (lexer_peek(lexer, 1) == 'b' || lexer_peek(lexer, 1) == 'x'));
			u64 value = 0;
			error = lex_integer(lexer, &value);
			if (error != COM_STATUS_OK) {
				break;
			}

			token.type = TOK_INTEGER;
			token.integer_magnitude = value;

			if (is_decimal && lexer_peek(lexer, 0) == '.' && lexer_peek(lexer, 1) != '.')
			{
				lexer->cursor += 1;
				token.type = TOK_NUMBER;
				token.number = (f64)value + lex_fractional(lexer);
			}
			if (is_decimal)
			{
				f64 number = token.type == TOK_NUMBER ? token.number : (f64)value;
				b32 has_exponent = false;
				error = lex_number_exponent(lexer, &number, &has_exponent);
				if (error == COM_STATUS_OK && has_exponent)
				{
					token.type = TOK_NUMBER;
					token.number = number;
				}
			}
		} break;

		case '\'':
		{
			lexer->cursor += 1;
			token.type = TOK_LETTER;
			u32 codepoint = 0;
			error = lex_escape_codepoint(lexer, token.site, &codepoint);
			if (error != COM_STATUS_OK) {
				break;
			}
			token.integer_magnitude = codepoint;

			if (lexer_peek(lexer, 0) == '\'') {
				lexer->cursor += 1;
			}
			else {
				error = lexer_errorf(lexer, ERROR_INVALID_CHARACTER_CONSTANT, token.site, "invalid character constant, expected \"'\"");
			}
		} break;

		case '`':
		case '"':
		{
			char quote = (char)lexer_peek(lexer, 0);
			b32 is_block = lexer_peek(lexer, 1) == quote && lexer_peek(lexer, 2) == quote;
			lexer->cursor += is_block ? 3 : 1;
			error = lex_string_segment(lexer, token.site, quote, is_block, false, 0, &token.atom);
			if (error == COM_STATUS_OK) {
				token.type = TOK_STRING;
			}
		} break;

		case '#':
		{
			lexer->cursor += 1;

			if (!is_identifier_start((char)lexer_peek(lexer, 0)))
			{
				error = lexer_errorf(lexer, ERROR_EXPECTED_MACRO_NAME, token.site, "expected macro name after '#'");
			}
			else
			{
				lex_identifier_size(lexer);
				const char *token_data = lexer->compiler_source->source.data + token.site.offset;
				u32 size = (u32)(lexer->cursor - token_data);
				Atom *atom = atom_from_data_size(lexer->atoms, token_data, size);
				if (!atom)
				{
					error = COM_STATUS_OOM;
				}
				else
				{
					token.type = atom_is_word_or_macro(atom);
					if (token.type == TOK_M_ENDOFFILE) {
						token.type = TOK_NONE;
					}
					else if (token.type == TOK_M_FILE_NAME) {
						token.type = TOK_STRING;
						token.atom = lexer->compiler_source->name;
					}
					else if (token.type == TOK_M_LINE_NUMBER) {
						token.type = TOK_INTEGER;
						token.integer_magnitude = lexer->line_index;
					}
					else if (token.type == TOK_IDENTIFIER) {
						error = lexer_errorf(lexer, ERROR_UNRECOGNIZED_MACRO, token.site, "unrecognized macro");
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
			error = lexer_errorf(lexer, ERROR_NUL_BYTE_IN_SOURCE, token.site, "NUL byte in source");
			lexer->cursor += 1;
		} break;

		case '[': token.type = TOK_SQUARE_LEFT;  lexer->cursor += 1; break;
		case ']': token.type = TOK_SQUARE_RIGHT; lexer->cursor += 1; break;
		case '(': token.type = TOK_LEFT_PAREN;   lexer->cursor += 1; break;
		case ')': token.type = TOK_PAREN_RIGHT;  lexer->cursor += 1; break;
		case ',': token.type = TOK_COMMA;        lexer->cursor += 1; break;

		case '{': token.type = TOK_LEFT_BRACE;  lexer->cursor += 1; break;
		case '}': token.type = TOK_RIGHT_BRACE; lexer->cursor += 1; break;

		case '%':
		{
			lexer->cursor += 1;
			token.type = TOK_MOD;
			if (lexer_peek(lexer, 0) == '=') {
				lexer->cursor += 1;
				token.type = TOK_MOD_ASSIGN;
			}
		} break;
		case '^':
		{
			lexer->cursor += 1;
			token.type = TOK_BIT_XOR;
			if (lexer_peek(lexer, 0) == '=') {
				lexer->cursor += 1;
				token.type = TOK_XOR_ASSIGN;
			}
		} break;
		case '~': token.type = TOK_TILDE;     lexer->cursor += 1; break;
		case ';': token.type = TOK_SEMICOLON; lexer->cursor += 1; break;

		case '/':
		{
			lexer->cursor += 1;
			token.type = TOK_DIV;

			if (lexer_peek(lexer, 0) == '=') {
				lexer->cursor += 1;
				token.type = TOK_DIV_ASSIGN;
			}
		} break;

		case '.':
		{
			lexer->cursor += 1;
			token.type = TOK_DOT;

			if (lexer_peek(lexer, 0) == '.') {
				lexer->cursor += 1;
				token.type = TOK_ELLIPSIS;
				if (lexer_peek(lexer, 0) == '.') {
					lexer->cursor += 1;
				}
			}
			else if (lexer_peek(lexer, 0) >= '0' && lexer_peek(lexer, 0) <= '9') {
				token.type = TOK_NUMBER;
				token.number = lex_fractional(lexer);
				b32 has_exponent = false;
				error = lex_number_exponent(lexer, &token.number, &has_exponent);
			}
		} break;

		case '<':
		{
			lexer->cursor += 1;
			token.type = TOK_LT;
			if (lexer_peek(lexer, 0) == '=') {
				lexer->cursor += 1;
				token.type = TOK_LTEQ;
			}
			else if (lexer_peek(lexer, 0) == '<') {
				lexer->cursor += 1;
				token.type = TOK_SHL;
				if (lexer_peek(lexer, 0) == '=') {
					lexer->cursor += 1;
					token.type = TOK_SHL_ASSIGN;
				}
			}
		} break;

		case ':':
		{
			lexer->cursor += 1;
			token.type = TOK_COLON;
			if (lexer_peek(lexer, 0) == ':') {
				lexer->cursor += 1;
				token.type = TOK_STATIC_BIND;
				if (lexer_peek(lexer, 0) == '=') {
					lexer->cursor += 1;
					token.type = TOK_HARD_BIND;
				}
			}
			else if (lexer_peek(lexer, 0) == '=') {
				lexer->cursor += 1;
				token.type = TOK_BIND;
			}
		} break;

		case '-':
		{
			lexer->cursor += 1;
			token.type = TOK_SUB;
			if (lexer_peek(lexer, 0) == '=') {
				lexer->cursor += 1;
				token.type = TOK_SUB_ASSIGN;
			}
			else if (lexer_peek(lexer, 0) == '-') {
				lexer->cursor += 1;
				token.type = TOK_MINUS_MINUS;
			}
			else if (lexer_peek(lexer, 0) == '>') {
				lexer->cursor += 1;
				token.type = TOK_ARROW;
			}
		} break;

		case '>':
		{
			lexer->cursor += 1;
			token.type = TOK_GT;
			if (lexer_peek(lexer, 0) == '=') {
				lexer->cursor += 1;
				token.type = TOK_GTEQ;
			}
			else if (lexer_peek(lexer, 0) == '>') {
				lexer->cursor += 1;
				token.type = TOK_SHR;
				if (lexer_peek(lexer, 0) == '=') {
					lexer->cursor += 1;
					token.type = TOK_SHR_ASSIGN;
				}
			}
		} break;

		case '?':
		{
			lexer->cursor += 1;
			token.type = TOK_QMARK;
			if (lexer_peek(lexer, 0) == '?') {
				lexer->cursor += 1;
				token.type = TOK_NIL_OR;
			}
			else if (lexer_peek(lexer, 0) == '=') {
				lexer->cursor += 1;
				token.type = TOK_NIL_ASSIGN;
			}
		} break;

		case '!':
		{
			lexer->cursor += 1;
			token.type = TOK_EXCLAMATION_MARK;
			if (lexer_peek(lexer, 0) == '!') {
				lexer->cursor += 1;
				token.type = TOK_NIL_AND;
			}
			else if (lexer_peek(lexer, 0) == '=') {
				lexer->cursor += 1;
				token.type = TOK_NEQ;
			}
		} break;

		case '|':
		{
			lexer->cursor += 1;
			token.type = TOK_BIT_OR;
			if (lexer_peek(lexer, 0) == '|') {
				lexer->cursor += 1;
				token.type = TOK_LOG_OR;
			}
		} break;

		case '&':
		{
			lexer->cursor += 1;
			token.type = TOK_BIT_AND;
			if (lexer_peek(lexer, 0) == '&') {
				lexer->cursor += 1;
				token.type = TOK_LOG_AND;
			}
		} break;

		case '=':
		{
			lexer->cursor += 1;
			token.type = TOK_ASSIGN;
			if (lexer_peek(lexer, 0) == '=') {
				lexer->cursor += 1;
				token.type = TOK_EQ;
			}
			else if (lexer_peek(lexer, 0) == '>') {
				lexer->cursor += 1;
				token.type = TOK_RET;
			}
		} break;

		case '*':
		{
			lexer->cursor += 1;
			token.type = TOK_MUL;
			if (lexer_peek(lexer, 0) == '=') {
				token.type = TOK_MUL_ASSIGN;
				lexer->cursor += 1;
			}
			else if (lexer_peek(lexer, 0) == '*') {
				token.type = TOK_POW;
				lexer->cursor += 1;
			}
		} break;

		// snuck
		case '+':
		{
			lexer->cursor += 1;
			token.type = TOK_ADD;
			if (lexer_peek(lexer, 0) == '=') {
				lexer->cursor += 1;
				token.type = TOK_ADD_ASSIGN;
			}
		} break;

		default:
		{
			error = lexer_errorf(lexer, ERROR_UNEXPECTED_CHARACTER, token.site, "unexpected character");
			lexer->cursor += 1;
		} break;
	}

	token.site.size = (u32)(lexer->cursor - (lexer->compiler_source->source.data + token.site.offset));
	if (token.site.size == 0) {
		token.site.size = 1;
	}

	if (error == COM_STATUS_OK)
	{
		error = token_array_push(lexer->tokens, lexer->scratch_arena, token);
		if (error == COM_STATUS_OK) {
			*emitted = token;
		}
	}
	return error;
}

static Com_Status lexer_tokenize(Compiler *compiler, Com_Source *source, Arena *scratch_arena, Token_Array *tokens)
{
	ASSERT(compiler);
	ASSERT(source);
	ASSERT(scratch_arena);
	ASSERT(tokens);
	Lexer lexer = {
		.compiler = compiler,
		.compiler_source = source,
		.atoms = compiler->atoms,
		.scratch_arena = scratch_arena,
		.tokens = tokens,
		.cursor = source->source.data,
		.end = source->source.data + source->source.size,
		.line_index = 1,
		.line_start = source->source.data,
	};

	for (;;)
	{
		SourceSite site = lexer_source_site(&lexer);
		Token token = {};
		Com_Status error = lexer_emit_token(&lexer, &token);
		if (error != COM_STATUS_OK)
		{
			Token error_token = token_from_status(error, site);
			return token_array_push(tokens, scratch_arena, error_token) == COM_STATUS_OK ? COM_STATUS_OK : COM_STATUS_OOM;
		}
		if (token.type == TOK_NONE) {
			return COM_STATUS_OK;
		}
	}
}
