//
// See Copyright Notice In elf.h
//

#include "source_diagnostics.c"

static const char *token_type_name(TokenType type)
{
	static const char *names[TOK_COUNT_] =
	{
#define XPAND(ENUM, NAME) [TOK_##ENUM] = NAME,
		TOKEN_XDEF(XPAND)
#undef XPAND
	};

	if ((u32)type >= TOK_COUNT_) {
		return "unknown";
	}
	return names[type];
}

static TokenType atom_is_word_or_macro(elf_String *atom)
{
	switch (atom->id) {
#define MCITEM(NAME, SYM) case XFUSE(TOK_, NAME): return XFUSE(TOK_, NAME);
		MACRODEF(MCITEM)
#undef MCITEM
		default: return TOK_IDENTIFIER;
	}
}

static TokenType check_keyword(elf_String *atom)
{
	switch (atom->id) {
#define KWITEM(NAME, SYM) case XFUSE(TOK_, NAME): return XFUSE(TOK_, NAME);
		KEYWORDDEF(KWITEM)
#undef KWITEM
		default: return TOK_IDENTIFIER;
	}
}

static u64 lexer_scratch_capacity(Lexer *lexer)
{
	return lexer->source.size + 2;
}

static b32 is_identifier_start(char c)
{
	return ('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z') || c == '_';
}

static b32 is_identifier_continue(char c)
{
	return is_identifier_start(c) || ('0' <= c && c <= '9');
}

static void lexer_init(Lexer *lexer, elf_State *state, elf_String *name, elf_StrSlice source)
{
	lexer->state = state;
	lexer->name = name;
	lexer->source = source;
	lexer->cursor = source.data;
	lexer->line_index = 1;
	lexer->line_start = source.data;
}

static void lexer_advance_line(Lexer *lexer, char *line_start)
{
	lexer->line_index += 1;
	lexer->line_start = line_start;
}

static SourceSite source_site_from_ptr(Lexer *lexer, Source data)
{
	ASSERT(data);
	ASSERT(data >= lexer->line_start);
	SourceSite site;
	site.data = data;
	site.size = 1;
	site.line_start = lexer->line_start;
	site.line_index = lexer->line_index;
	return site;
}

static SourceSite lexer_source_site(Lexer *lexer, Source data)
{
	SourceSite site;
	site.data = data;
	site.size = 1;
	site.line_start = lexer->line_start;
	site.line_index = lexer->line_index;
	return site;
}

static void log_source_error(Lexer *lexer, SourceSite site, char const *fmt, ...)
{
	elf_Scratch scratch = elf_begin_scratch();

	va_list args;
	va_start(args, fmt);
	char *message = elf_arena_pushfv(scratch.arena, fmt, args);
	va_end(args);
	elf_arena_push_zero(scratch.arena, 1);

	log_linef(LOG_LEVEL_ERROR, "%s [%u:%llu]: %s"
	,	atom_data(lexer->name)
	,	site.line_index
	,	source_slice_column(site)
	,	message);

	elf_StrSlice source = lexer->source;
	print_source_slice_marker(site, source);
	elf_end_scratch(scratch);
}

static b32 lexer_is_hex_digit(char c)
{
	return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static u32 lexer_hex_digit(char c)
{
	if (c >= '0' && c <= '9')
	{
		return (u32)(c - '0');
	}
	if (c >= 'a' && c <= 'f')
	{
		return 10 + (u32)(c - 'a');
	}
	return 10 + (u32)(c - 'A');
}

static b32 lexer_parse_hex_codepoint(Lexer *lexer, char **cursor, u32 digits, SourceSite site, u32 *out)
{
	char *cur = *cursor;
	u32 codepoint = 0;
	for (u32 i = 0; i < digits; ++ i)
	{
		if (!lexer_is_hex_digit(cur[i]))
		{
			log_source_error(lexer, site, "invalid unicode escape");
			return false;
		}
		codepoint = (codepoint << 4) | lexer_hex_digit(cur[i]);
	}

	*cursor = cur + digits;
	*out = codepoint;
	return true;
}

static b32 lexer_parse_unicode_escape(Lexer *lexer, char **cursor, SourceSite site, u32 *out)
{
	char *cur = *cursor;
	char kind = *cur++;
	u32 codepoint = 0;

	if (kind == 'u')
	{
		if (!lexer_parse_hex_codepoint(lexer, &cur, 4, site, &codepoint))
		{
			*cursor = cur;
			return false;
		}

		if (codepoint >= 0xD800 && codepoint <= 0xDBFF)
		{
			if (cur[0] == '\\' && cur[1] == 'u')
			{
				cur += 2;
				u32 low = 0;
				if (!lexer_parse_hex_codepoint(lexer, &cur, 4, site, &low))
				{
					*cursor = cur;
					return false;
				}
				if (low >= 0xDC00 && low <= 0xDFFF)
				{
					codepoint = 0x10000 + ((codepoint - 0xD800) << 10) + (low - 0xDC00);
				}
				else
				{
					log_source_error(lexer, site, "invalid unicode surrogate pair");
					*cursor = cur;
					return false;
				}
			}
			else
			{
				log_source_error(lexer, site, "missing unicode surrogate pair");
				*cursor = cur;
				return false;
			}
		}
		else if (codepoint >= 0xDC00 && codepoint <= 0xDFFF)
		{
			log_source_error(lexer, site, "unexpected unicode low surrogate");
			*cursor = cur;
			return false;
		}
	}
	else
	{
		if (!lexer_parse_hex_codepoint(lexer, &cur, 8, site, &codepoint))
		{
			*cursor = cur;
			return false;
		}
	}

	if (codepoint > 0x10FFFF)
	{
		log_source_error(lexer, site, "unicode codepoint out of range");
		*cursor = cur;
		return false;
	}

	*cursor = cur;
	*out = codepoint;
	return true;
}

static u32 lex_escape_codepoint(Lexer *lexer, char **cursor, SourceSite site)
{
	char *cur = *cursor;
	u32 codepoint = (u8)*cur++;

	if (codepoint != '\\')
	{
		*cursor = cur;
		return codepoint;
	}

	SourceSite escape_site = source_site_from_ptr(lexer, cur - 1);
	char escape = *cur++;
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
			if (!lexer_parse_hex_codepoint(lexer, &cur, 2, escape_site, &codepoint))
			{
				codepoint = 0xFFFD;
			}
		} break;

		case 'u':
		case 'U':
		{
			cur -= 1;
			if (!lexer_parse_unicode_escape(lexer, &cur, escape_site, &codepoint))
			{
				codepoint = 0xFFFD;
			}
		} break;

		case '\0':
		{
			log_source_error(lexer, site, "unterminated escape sequence");
			codepoint = 0;
			cur -= 1;
		} break;

		default:
		{
			log_source_error(lexer, escape_site, "invalid escape sequence");
			codepoint = (u8)escape;
		} break;
	}

	*cursor = cur;
	return codepoint;
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

static f64 lex_fractional(char **cursor)
{
	char *cur = *cursor;
	f64 value = 0;
	f64 scale = 1;

	while ('0' <= *cur && *cur <= '9') {
		value = value * 10 + (*cur++ - '0');
		scale *= 10;
	}

	*cursor = cur;
	return value / scale;
}

static u64 lex_integer(Lexer *lexer, char **cursor)
{
	char *cur = *cursor;
	u64 base = 10;

	if (*cur == '0') {
		cur += 1;
		if (*cur == 'b') {
			cur += 1;
			base = 2;
		}
		else if (*cur == 'x') {
			cur += 1;
			base = 16;
		}
	}

	u64 integer = 0;
	b32 overflow = false;
	for (;;) {
		u64 digit;

		if ('A' <= *cur && *cur <= 'Z') {
			digit = 10 + *cur++ - 'A';
		}
		else if ('a' <= *cur && *cur <= 'z') {
			digit = 10 + *cur++ - 'a';
		}
		else if ('0' <= *cur && *cur <= '9') {
			digit = *cur++ - '0';
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
			if (!overflow) {
				log_source_error(lexer, source_site_from_ptr(lexer, cur - 1), "integer literal exceeds u64 range");
			}
			overflow = true;
			integer = max;
		}
		else if (!overflow)
		{
			integer = integer * base + digit;
		}
	}

	*cursor = cur;
	return integer;

	invalid_base:
	*cursor = cur;
	log_source_error(lexer, source_site_from_ptr(lexer, cur - 1), "invalid base '%llu' for digit", base);
	return integer;
}

static u32 lex_identifier(char **cursor, char *buffer)
{
	char *cur = *cursor;
	char *out = buffer;

	ASSERT(is_identifier_start(*cur));
	do {
		*out++ = *cur++;
	}
	while (is_identifier_continue(*cur));

	*out = 0;
	*cursor = cur;
	return (u32)(out - buffer);
}

static elf_String *lex_string(Lexer *lexer, char **cursor, SourceSite site, b32 is_format, TokenType *type)
{
	char *cur = *cursor;
	elf_Scratch scratch = elf_begin_scratch();
	char *buffer = elf_arena_reserve(scratch.arena, lexer_scratch_capacity(lexer));
	char *out = buffer;
	b32 is_block = cur[0] == '"' && cur[1] == '"' && cur[2] == '"';

	cur += is_block ? 3 : 1;

	for (;;)
	{
		if (*cur == 0) {
			log_source_error(lexer, site, is_block ? "unterminated string block" : "unterminated string");
			break;
		}

		if (is_block && cur[0] == '"' && cur[1] == '"' && cur[2] == '"') {
			cur += 3;
			break;
		}

		if (!is_block && *cur == '"') {
			cur += 1;
			break;
		}

		if (!is_block && (*cur == '\n' || *cur == '\r')) {
			log_source_error(lexer, source_site_from_ptr(lexer, cur), "newline in string; use \\n or a string block");
			break;
		}

		if (*cur == '\r') {
			cur += 1;
			if (*cur == '\n') {
				cur += 1;
			}
			lexer_advance_line(lexer, cur);
			*out++ = '\n';
		}
		else if (*cur == '\n') {
			cur += 1;
			lexer_advance_line(lexer, cur);
			*out++ = '\n';
		}
		else if (is_format && cur[0] == FORMAT_CHAR && cur[1] == '{') {
			*out++ = *cur++;
			*out++ = *cur++;
		}
		else if (*cur == '\\') {
			u32 codepoint = lex_escape_codepoint(lexer, &cur, site);
			out = write_utf8(out, codepoint);
		}
		else {
			*out++ = *cur++;
		}
	}

	*out = 0;

	if (!is_format) {
		*type = TOK_STRING;
	}
	else {
		*type = TOK_FORMAT_STRING;
	}

	escape:
	elf_String *atom = elf_atom_from_data_size(lexer->state, buffer, (u32)(out - buffer));
	*cursor = cur;
	elf_end_scratch(scratch);
	return atom;
}


static elf_String *lex_string_part(Lexer *lexer, char **cursor, SourceSite site, b32 in_block, b32 *ended)
{
	*ended = false;

	char *cur = *cursor;
	elf_Scratch scratch = elf_begin_scratch();
	char *buffer = elf_arena_reserve(scratch.arena, lexer_scratch_capacity(lexer));
	char *out = buffer;

	for (;;)
	{
		if (*cur == 0) {
			log_source_error(lexer, site, in_block ? "unterminated string block" : "unterminated string");
			break;
		}

		if (in_block && cur[0] == '"' && cur[1] == '"' && cur[2] == '"') {
			cur += 3;
			*ended = true;
			break;
		}

		if (!in_block && *cur == '"') {
			cur += 1;
			*ended = true;
			break;
		}

		if (cur[0] == FORMAT_CHAR && cur[1] == '{') {
			cur += 2;
			break;
		}

		if (!in_block && (*cur == '\n' || *cur == '\r')) {
			log_source_error(lexer, source_site_from_ptr(lexer, cur), "newline in string; use \\n or a string block");
			break;
		}

		if (*cur == '\r') {
			cur += 1;
			if (*cur == '\n') {
				cur += 1;
			}
			lexer_advance_line(lexer, cur);
			*out++ = '\n';
		}
		else if (*cur == '\n') {
			cur += 1;
			lexer_advance_line(lexer, cur);
			*out++ = '\n';
		}
		else if (*cur == '\\') {
			u32 codepoint = lex_escape_codepoint(lexer, &cur, site);
			out = write_utf8(out, codepoint);
		}
		else {
			*out++ = *cur++;
		}
	}

	*out = 0;

	elf_String *atom = elf_atom_from_data_size(lexer->state, buffer, (u32)(out - buffer));
	*cursor = cur;
	elf_end_scratch(scratch);
	return atom;
}

static Token lex_token(Lexer *lexer)
{
	char *cur = lexer->cursor;
	Token token;

	retry:
	token = (Token){0};
	token.type = TOK_NONE;
	token.site = lexer_source_site(lexer, cur);

	if (lexer->mode.type == LEXER_MODE_INTERPOLATION && *cur == '}' && lexer->mode.depth == 0)
	{
		if (lexer->mode_index == 0) {
			log_source_error(lexer, source_site_from_ptr(lexer, lexer->mode.string_start), "invalid interpolated string");
		}

		cur += 1;
		b32 ended;
		token.atom = lex_string_part(lexer, &cur, source_site_from_ptr(lexer, lexer->mode.string_start), lexer->mode.is_block_string, &ended);
		token.type = ended ? TOK_STRING_END : TOK_STRING_PART;

		if (ended) {
			if (lexer->mode_index <= 0) {
				log_source_error(lexer, source_site_from_ptr(lexer, lexer->mode.string_start), "invalid interpolated string");
				return token;
			}
			lexer->mode = lexer->mode_stack[-- lexer->mode_index];
		}
		else {
			lexer->mode.depth               = 0;
			lexer->mode.interpolation_start = cur;
		}
		goto update_lexer;
	}

	switch (*cur)
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
			if (* cur == 'f' && cur[1] == '"')
			{
				if (lexer->mode_index >= ARRAY_COUNT(lexer->mode_stack)) {
					log_source_error(lexer, source_site_from_ptr(lexer, cur), "too many interpolated strings, limit: %i", ARRAY_COUNT(lexer->mode_stack));
					return token;
				}

				char *string_start = cur;

				b32 is_block = cur[2] == '"' && cur[3] == '"';
				cur += 2 + is_block * 2;

				b32 ended;
				token.atom = lex_string_part(lexer, &cur, source_site_from_ptr(lexer, string_start), is_block, &ended);
				token.type = ended ? TOK_STRING : TOK_STRING_START;
				if (!ended)
				{
					lexer->mode_stack[lexer->mode_index ++] = lexer->mode;
					lexer->mode.type = LEXER_MODE_INTERPOLATION;
					lexer->mode.interpolation_start = cur;
					lexer->mode.string_start = string_start;
					lexer->mode.is_block_string = is_block;
					lexer->mode.depth = 0;
				}
				break;
			}
			else
			{
				elf_Scratch scratch = elf_begin_scratch();
				char *buffer = elf_arena_push(scratch.arena, lexer_scratch_capacity(lexer));
				u32 size = lex_identifier(&cur, buffer);
				elf_String *atom = elf_atom_from_data_size(lexer->state, buffer, size);
				token.type = check_keyword(atom);
				if (token.type == TOK_IDENTIFIER) {
					token.atom = atom;
				}
				elf_end_scratch(scratch);
			}
		} break;

		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
		{
			u64 value = lex_integer(lexer, &cur);

			token.type = TOK_INTEGER;
			token.integer_magnitude = value;

			if (cur[0] == '.' && cur[1] != '.') {
				cur += 1;
				token.type = TOK_NUMBER;
				token.number = (f64)value + lex_fractional(&cur);
			}
		} break;

		case '\'':
		{
			cur += 1;
			token.type = TOK_LETTER;
			token.integer_magnitude = lex_escape_codepoint(lexer, &cur, token.site);

			if (*cur == '\'') {
				cur += 1;
			}
			else {
				log_source_error(lexer, token.site, "invalid character constant, expected \"'\"");
			}
		} break;

		case '"':
		{
			token.atom = lex_string(lexer, &cur, token.site, false, &token.type);
		} break;

		case '#':
		{
			elf_Scratch scratch = elf_begin_scratch();
			char *buffer = elf_arena_push(scratch.arena, lexer_scratch_capacity(lexer));
			cur += 1;

			if (!is_identifier_start(*cur))
			{
				token.type = TOK_IDENTIFIER;
				log_source_error(lexer, token.site, "expected macro name after '#'");
			}
			else
			{
				buffer[0] = '#';
				u32 size = 1 + lex_identifier(&cur, buffer + 1);
				elf_String *atom = elf_atom_from_data_size(lexer->state, buffer, size);

				token.type = atom_is_word_or_macro(atom);
				if (token.type == TOK_M_ENDOFFILE) {
					token.type = TOK_NONE;
				}
				else if (token.type == TOK_M_FILE_NAME) {
					token.type = TOK_STRING;
					token.atom = lexer->name;
				}
				else if (token.type == TOK_M_LINE_NUMBER) {
					token.type = TOK_INTEGER;
					token.integer_magnitude = lexer->line_index;
				}
				else if (token.type == TOK_IDENTIFIER) {
					log_source_error(lexer, token.site, "unrecognized macro");
				}
			}

			elf_end_scratch(scratch);
		} break;

		case '\0':
		{
			token.type = TOK_NONE;
		} break;

		case ' ': case '\t':
		{
			cur += 1;
			goto retry;
		}

		case '\n':
		{
			cur += 1;
			lexer_advance_line(lexer, cur);
			goto retry;
		}

		case '\r':
		{
			cur += 1;
			if (*cur == '\n') {
				cur += 1;
			}
			lexer_advance_line(lexer, cur);
			goto retry;
		}

		case '[': token.type = TOK_SQUARE_LEFT;  cur += 1; break;
		case ']': token.type = TOK_SQUARE_RIGHT; cur += 1; break;
		case '(': token.type = TOK_LEFT_PAREN;   cur += 1; break;
		case ')': token.type = TOK_PAREN_RIGHT;  cur += 1; break;
		case ',': token.type = TOK_COMMA;        cur += 1; break;

		case '{':
		{
			cur += 1;
			token.type = TOK_LEFT_BRACE;
			if (lexer->mode.type == LEXER_MODE_INTERPOLATION) {
				lexer->mode.depth ++;
			}
		}
		break;
		case '}':
		{
			cur += 1;
			token.type = TOK_RIGHT_BRACE;
			if (lexer->mode.type == LEXER_MODE_INTERPOLATION) {
				lexer->mode.depth --;
			}
		}
		break;

		case '%':
		{
			cur += 1;
			token.type = TOK_MOD;
			if (*cur == '=') {
				cur += 1;
				token.type = TOK_MOD_ASSIGN;
			}
		} break;
		case '^':
		{
			cur += 1;
			token.type = TOK_BIT_XOR;
			if (*cur == '=') {
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

			if (*cur == '=') {
				cur += 1;
				token.type = TOK_DIV_ASSIGN;
			}
			else if (*cur == '*') {
				cur += 1;
				while (*cur) {
					if (*cur == '*' && cur[1] == '/') {
						cur += 2;
						goto retry;
					}
					if (*cur == '\r') {
						cur += 1;
						if (*cur == '\n') {
							cur += 1;
						}
						lexer_advance_line(lexer, cur);
						continue;
					}
					if (*cur == '\n') {
						cur += 1;
						lexer_advance_line(lexer, cur);
						continue;
					}
					cur += 1;
				}

				log_source_error(lexer, token.site, "unterminated comment");
				goto retry;
			}
			else if (*cur == '/') {
				while (*cur && *cur != '\n' && *cur != '\r') {
					cur += 1;
				}
				goto retry;
			}
		} break;

		case '.':
		{
			cur += 1;
			token.type = TOK_DOT;

			if (*cur == '.') {
				cur += 1;
				token.type = TOK_ELLIPSIS;
				if (*cur == '.') {
					cur += 1;
				}
			}
			else if ('0' <= *cur && *cur <= '9') {
				token.type = TOK_NUMBER;
				token.number = lex_fractional(&cur);
			}
		} break;

		case '<':
		{
			cur += 1;
			token.type = TOK_LT;
			if (*cur == '=') {
				cur += 1;
				token.type = TOK_LTEQ;
			}
			else if (*cur == '<') {
				cur += 1;
				token.type = TOK_SHL;
				if (*cur == '=') {
					cur += 1;
					token.type = TOK_SHL_ASSIGN;
				}
			}
		} break;

		case ':':
		{
			cur += 1;
			token.type = TOK_COLON;
			if (*cur == ':') {
				cur += 1;
				token.type = TOK_STATIC_BIND;
				if (*cur == '=') {
					cur += 1;
					token.type = TOK_HARD_BIND;
				}
			}
			else if (*cur == '=') {
				cur += 1;
				token.type = TOK_BIND;
			}
		} break;

		case '-':
		{
			cur += 1;
			token.type = TOK_SUB;
			if (*cur == '=') {
				cur += 1;
				token.type = TOK_SUB_ASSIGN;
			}
			else if (*cur == '-') {
				cur += 1;
				token.type = TOK_MINUS_MINUS;
			}
			else if (*cur == '>') {
				cur += 1;
				token.type = TOK_ARROW;
			}
		} break;

		case '>':
		{
			cur += 1;
			token.type = TOK_GT;
			if (*cur == '=') {
				cur += 1;
				token.type = TOK_GTEQ;
			}
			else if (*cur == '>') {
				cur += 1;
				token.type = TOK_SHR;
				if (*cur == '=') {
					cur += 1;
					token.type = TOK_SHR_ASSIGN;
				}
			}
		} break;

		case '?':
		{
			cur += 1;
			token.type = TOK_QMARK;
			if (*cur == '?') {
				cur += 1;
				token.type = TOK_NIL_OR;
			}
			else if (*cur == '=') {
				cur += 1;
				token.type = TOK_NIL_ASSIGN;
			}
		} break;

		case '!':
		{
			cur += 1;
			token.type = TOK_EXCLAMATION_MARK;
			if (*cur == '!') {
				cur += 1;
				token.type = TOK_NIL_AND;
			}
			else if (*cur == '=') {
				cur += 1;
				token.type = TOK_NEQ;
			}
		} break;

		case '|':
		{
			cur += 1;
			token.type = TOK_BIT_OR;
			if (*cur == '|') {
				cur += 1;
				token.type = TOK_LOG_OR;
			}
		} break;

		case '&':
		{
			cur += 1;
			token.type = TOK_BIT_AND;
			if (*cur == '&') {
				cur += 1;
				token.type = TOK_LOG_AND;
			}
		} break;

		case '=':
		{
			cur += 1;
			token.type = TOK_ASSIGN;
			if (*cur == '=') {
				cur += 1;
				token.type = TOK_EQ;
			}
		} break;

		case '*':
		{
			cur += 1;
			token.type = TOK_MUL;
			if (*cur == '=') {
				cur += 1;
				token.type = TOK_MUL_ASSIGN;
			}
			else if (*cur == '*') {
				cur += 1;
				token.type = TOK_POW;
			}
		} break;

		// snuck
		case '+':
		{
			cur += 1;
			token.type = TOK_ADD;
			if (*cur == '=') {
				cur += 1;
				token.type = TOK_ADD_ASSIGN;
			}
		} break;

		default:
		{
			log_source_error(lexer, token.site, "unexpected character");
			cur += 1;
		} break;
	}

	token.site.size = (u32)(cur - token.site.data);
	if (token.site.size == 0) {
		token.site.size = 1;
	}

	while (*cur == ' ' || *cur == '\t') {
		cur += 1;
	}
	if (cur[0] == '/' && (cur[1] == '/' || cur[1] == '*')) {
		token.eol = 1;
	}
	else if (cur[0] == '\n' || cur[0] == '\r') {
		token.eol = 1;
	}

	update_lexer:
	lexer->cursor = cur;
	return token;
}
