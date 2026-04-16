//
// See Copyright Notice In elf.h
//

// todo: avoid using these macros, they're ugly
#define POS0() (parser->cursor[0])
#define POS1() (parser->cursor[1])
#define MOVE() (*(parser->cursor ++))
#define MOVEN(n) ((parser->cursor += n))
#define PICK(xx) ((POS0() == (xx)) ? (MOVEN(1), 1) : 0)


// Todo, bruh !
static TokenType text_is_word_or_macro(char *name)
{
#define MCITEM(NAME,SYM) if (text_eq(SYM,name)) return XFUSE(TOK_,NAME);
	MACRODEF(MCITEM)
#undef MCITEM
	return TOK_IDENTIFIER;
}

static TokenType check_keyword(char *name)
{
	#define KWITEM(NAME,SYM) if (text_eq(SYM,name)) return XFUSE(TOK_,NAME);
	KEYWORDDEF(KWITEM)
	#undef KWITEM
	return TOK_IDENTIFIER;
}

//
// todo: we already have one of these somewhere, recycle!
//
static void parser_dialog(Parser *parser, Source site, char const *fmt, ...)
{
	site = site ? site : parser->tok.site;

	// | attempted to get field of
	//
	// | name := 0
	// | name.name = 0
	// | ^
	// |
	// |

	char *lineloc;
	int linenum = get_source_info(parser->source,site,&lineloc);

	/* skip initial blank characters for optimal gimmicky */
	while (*lineloc == '\t' || *lineloc == ' ') {
		lineloc += 1;
	}

	char u[0x40];

	int underline = site - lineloc;
	if (underline >= sizeof(u)) {
		underline = sizeof(u) - 1;
		lineloc = site - underline;
	}

	int linelen = 0;
	for (; linelen < underline+32; ++ linelen) {
		if (lineloc[linelen] == '\0') break;
		if (lineloc[linelen] == '\r') break;
		if (lineloc[linelen] == '\n') break;
	}

	for (int i = 0; i < underline; ++ i) {
		u[i] = lineloc[i] == '\t' ? '\t' : '-';
	}
	u[underline]='^';

	if (fmt !=  0) {
		char b[0x1000];
		va_list v;
		va_start(v,fmt);
		stbsp_vsnprintf(b,sizeof(b),fmt,v);
		va_end(v);
		char *filename = parser->name;
		printf("%s [%i:%lli]: %s\n",filename,linenum,(elf_Integer)(1+site-lineloc),b);
	}
	printf("| %.*s\n",linelen,lineloc);
	printf("| %.*s\n",underline+1,u);
}


static int pick_esc_char(Parser *parser) {
	int chr = *parser->cursor ++;
	if (chr == '\\') {

		chr = *parser->cursor ++;

		switch (chr) {
			case '\\': return '\\';
			case '\0': return '\0';
			case 't':  return '\t';
			case 'n':  return '\n';
			case 'r':  return '\r';
		}
	}
	return chr;
}



// assuming we're already at an integer char,
// todo: also, we assume base 10, but do we want to
// support 0x255.255 type stuff?
static Num lex_fractional(Parser *parser) {
	Num x = 0, y = 1;
	while (is_digit_chr(*parser->cursor)) {
		x = x * 10 + (*parser->cursor ++ - '0');
		y = y * 10;
	}
	return x / y;
}



// assuming we're already at an integer char
static Int lex_integer(Parser *parser) {

	Int base = 10;

	// figure out the base
	if (*parser->cursor == '0') {
		parser->cursor += 1;
		if (*parser->cursor == 'b') {
			parser->cursor += 1;
			base = 2;
		}
		else if (*parser->cursor == 'x') {
			parser->cursor += 1;
			base = 16;
		}
	}

	Int integer, digit;
	for (integer = 0, digit = -1; ; integer = integer * base + digit) {

		if (WITHIN(*parser->cursor, 'A', 'Z' + 1)) {

			digit = 10 + *parser->cursor ++ - 'A';

			if (base != 16) goto _err;
		}
		else if (WITHIN(*parser->cursor, 'a', 'z' + 1)) {

			digit = 10 + *parser->cursor ++ - 'a';

			if (base != 16) goto _err;
		}
		else if (WITHIN(*parser->cursor, '0', '9' + 1)) {

			digit = *parser->cursor ++ - '0';

			if (base == 2 && digit > 1) goto _err;
		}
		else {
			break;
		}
	}

	return integer;
	_err:
	parser_dialog(parser, parser->cursor, "invalid base '%i' for digit", base);
	return integer;
}



// assuming we're at an identifier char
static int lex_identifier(Parser *parser, char *buf, int cap) {
	int len = 0;

	do {
		if (len >= cap) {
			parser_dialog(parser, parser->cursor, "identifier is too long");
		}
		buf[len ++] = *parser->cursor ++;
	} while (is_letter_or_digit_chr(*parser->cursor) || (*parser->cursor == '_'));

	buf[len] = 0;
	return len;
}

static int pick_empty_chars(Parser *parser) {
	int lines = 0;
	retry:
	switch (*parser->cursor) {
		case ' ': case '\t': {
			MOVE();
		} goto retry;
		case '\n': {
			MOVE();
			// new_line(parser);
			lines += 1;
		} goto retry;
		case '\r': {
			MOVE();
			PICK('\n');
			// new_line(parser);
			lines += 1;
		} goto retry;
	}
	return lines;
}

static Token lex_token(Parser *parser)
{
	retry:
	Token token = {};
	token.type = TOK_NONE;
	token.site = parser->cursor;

	switch (*parser->cursor)
	{
		case'A':case'B':case'C':case'D':case'E':case'F':case'G':case'H':case'I':case'J':case'K':case'L':case'M':
		case'N':case'O':case'P':case'Q':case'R':case'S':case'T':case'U':case'V':case'W':case'X':case'Y':case'Z':
		case'a':case'b':case'c':case'd':case'e':case'f':case'g':case'h':case'i':case'j':case'k':case'l':case'm':
		case'n':case'o':case'p':case'q':case'r':case's':case't':case'u':case'v':case'w':case'x':case'y':case'z':
		case'_':
		{
			// Todo, can this be done in the parser?
			if (parser->cursor[0] == 'f' && parser->cursor[1] == '"') {
				parser->cursor ++;
				token.type = TOK_FORMAT_STRING;
				goto strcase;
			}

			char buffer[1024];

			u32 length = lex_identifier(parser, buffer, sizeof(buffer) - 1);

			token.type = check_keyword(buffer);

			// Todo, proper allocator ...
			if (token.type == TOK_IDENTIFIER)
			{
				token.text = copy_text2(length, buffer);
			}

		}
		break;
		// Todo, handle exponent notation!
		case '0':case '1':case '2':case '3':case '4':
		case '5':case '6':case '7':case '8':case '9':
		{
			i64 x = lex_integer(parser);

			token.type    = TOK_INTEGER;
			token.integer = x;

			// post-decimal part, ensure we don't match a ".."
			if ((POS0() == '.') && (POS1() != '.'))
			{
				MOVE();

				f64 y = lex_fractional(parser);

				token.type   = TOK_NUMBER;
				token.number = x + y;
			}
		}
		break;

		case '\'': {
			MOVE();

			// todo!!: come back to this!
			token.type = TOK_LETTER;
			do {
				token.integer = pick_esc_char(parser);
				// MOVE();
			} while(0);

			if (!PICK('\'')) {
				parser_dialog(parser, token.line, "invalid character constant, expected \"'\"");
			}
		} break;
		case '"': {
			strcase:
			MOVE();

			/* todo: use static buffer first if that fills up move onto
			the dynamic buffer, store static buffer within the lexer */
			char *buffer = 0;
			int   length = 0;
			int needsformatting = false;

			while (*parser->cursor) {
				while (POS0() != 0 && POS0() != '"') {
					/* are multi-line strings illegal? */
					if (PICK('\n') || (PICK('\r') && (PICK('\n'),1))) {
						// new_line(parser);
						heap_array_add(buffer,'\n');
					} else {
						// don't check for format
						if (token.type != TOK_FORMAT_STRING) goto escchar;

						// todo: make it so that we can escape the formatting
						if (POS0() == FORMAT_CHAR && POS1() == '{') {
							needsformatting = true;
							heap_array_add(buffer, *parser->cursor ++);
							heap_array_add(buffer, *parser->cursor ++);
						} else {
							escchar:
							int chr = pick_esc_char(parser);
							heap_array_add(buffer, chr);
						}
					}
				}
				if (!PICK('"')) {
					parser_dialog(parser,token.line,"invalid string");
				}

				// try to find another string to join with
				int lines = pick_empty_chars(parser);
				if (lines) token.eol = true;

				// did we find anything?
				if (PICK('"')) {
					heap_array_add(buffer,'\n');
				} else break;
			}

			heap_array_add(buffer,0);

			// todo: leak, allocate this properly in some sort
			// of constant pool with intering, use the atomizer
			// that way we avoid going thru the GC, if during
			// parsing we figure out the thing is dead, we dealloc it
			if (!needsformatting || token.type != TOK_FORMAT_STRING) {
				token.type = TOK_STRING;
			}
			token.text = copy_text2(length,buffer);
			free_heap_array(buffer);
		} break;

		case '#':
		{
			char buffer[128];

			++ parser->cursor;

			lex_identifier(parser, buffer, sizeof(buffer) - 1);

			token.type = text_is_word_or_macro(buffer);
			if (token.type == TOK_M_ENDOFFILE) {
				token.type = TOK_NONE;
			} else if (token.type==TOK_M_FILE_NAME) {
				token.type = TOK_STRING;
				token.text = parser->name;
			} else if (token.type == TOK_M_LINE_NUMBER) {
				// todo: get this from the source location!
				__debugbreak();

				token.type = TOK_INTEGER;
				token.integer = -1;
			} else if (token.type == TOK_IDENTIFIER) {
				parser_dialog(parser,token.line,"unrecognized macro");
			} else {
				/* let parser handle this */
			}
		}
		break;


		case '\0': {
			token.type = TOK_NONE;
		} break;

		case ' ': case '\t': {
			MOVE();
		} goto retry;

		case '\n': {
			MOVE();
			//	parser->line_pos = parser->cursor;
			//	parser->line_num += 1;
		} goto retry;

		case '\r': {
			MOVE();
			PICK('\n');
			//	parser->line_pos = parser->cursor;
			//	parser->line_num += 1;
		} goto retry;

		case '/': {
			MOVE();
			token.type = TOK_DIV;
			if (PICK('*')) {
				for (;;) {
					/* handle lines */
					if (PICK('\n')) {
						//	parser->line_pos = parser->cursor;
						//	parser->line_num += 1;
					} else
					if (PICK('\r')) {
						PICK('\n');
						//	parser->line_pos = parser->cursor;
						//	parser->line_num += 1;
					} else
					if (PICK('*')) {
						if (PICK('/')) {
							/* end of comment */
							break;
						}
					} else {
						/* keep skipping! */
						MOVE();
					}
				}
				goto retry;
			} else
			if (PICK('/')) {
				while (POS0() != 0 && !is_eol_chr(POS0())) {
					MOVE();
				}
				goto retry;
			}
		} break;

		case '.':
		{
			++ parser->cursor;
			token.type = TOK_DOT;

			// Todo, settle on either ... or ..
			if (* parser->cursor == '.') {
				++ parser->cursor;
				token.type = TOK_ELLIPSIS;
				if (* parser->cursor == '.') {
					++ parser->cursor;
					token.type = TOK_ELLIPSIS;
				}
			}
			// Todo, could this just be done in the parser or no?
			else if (is_digit_chr(POS0())) {
				token.type = TOK_NUMBER;
				token.number = lex_fractional(parser);
			}
		}
		break;
		case '[': {
			token.type = TOK_SQUARE_LEFT;
			++ parser->cursor;
		} break;
		case ']': {
			token.type = TOK_SQUARE_RIGHT;
			++ parser->cursor;
		} break;
		case '<': {
			token.type = TOK_LT;
			++ parser->cursor;
			if (* parser->cursor == '=') {
				token.type = TOK_LTEQ;
				++ parser->cursor;
			}
			else if (* parser->cursor == '<') {
				token.type = TOK_SHL;
				++ parser->cursor;
			}
		} break;
		case ':': {
			token.type = TOK_COLON;
			++ parser->cursor;
			if (* parser->cursor == ':') {
				token.type = TOK_STATIC_BIND;
				++ parser->cursor;
				if (* parser->cursor == '=') {
					token.type = TOK_HARD_BIND;
					++ parser->cursor;
				}
			}
			else if (* parser->cursor == '=') {
				token.type = TOK_BIND;
				++ parser->cursor;
			}
		} break;

		case '-': {
			token.type = TOK_SUB;
			++ parser->cursor;
			if (* parser->cursor == '-') {
				token.type = TOK_MINUS_MINUS;
				++ parser->cursor;
				if (* parser->cursor == '>') {
					token.type = TOK_LONG_ARROW;
					++ parser->cursor;
				}
			}
			else if (* parser->cursor == '>') {
				token.type = TOK_ARROW;
				++ parser->cursor;
			}
		} break;

		case '>': {
			token.type = TOK_GT;
			++ parser->cursor;
			if (* parser->cursor == '=') {
				token.type = TOK_GTEQ;
				++ parser->cursor;
			}
			else if (* parser->cursor == '>') {
				token.type = TOK_SHR;
				++ parser->cursor;
			}
		} break;
		case '?': {
			token.type = TOK_QMARK;
			++ parser->cursor;
			if (* parser->cursor == '?') {
				token.type = TOK_NIL_OR;
				++ parser->cursor;
			}
			else if (* parser->cursor == '=') {
				token.type = TOK_NIL_ASSIGN;
				++ parser->cursor;
			}
		} break;
		case '!': {
			token.type = TOK_EXCLAMATION_MARK;
			++ parser->cursor;
			if (* parser->cursor == '!') {
				token.type = TOK_NIL_AND;
				++ parser->cursor;
			}
			else if (* parser->cursor == '=') {
				token.type = TOK_NEQ;
				++ parser->cursor;
			}
		} break;
		case '|': {
			token.type = TOK_BIT_OR;
			++ parser->cursor;
			if (* parser->cursor == '|') {
				token.type = TOK_LOG_OR;
				++ parser->cursor;
			}
		} break;
		case '&': {
			token.type = TOK_BIT_AND;
			++ parser->cursor;
			if (* parser->cursor == '&') {
				token.type = TOK_LOG_AND;
				++ parser->cursor;
			}
		} break;
		case '=': {
			token.type = TOK_ASSIGN;
			++ parser->cursor;
			if (* parser->cursor == '=') {
				token.type = TOK_EQ;
				++ parser->cursor;
			}
		} break;
		case '*': {
			token.type = TOK_MUL;
			++ parser->cursor;
			if (* parser->cursor == '*') {
				token.type = TOK_POW;
				++ parser->cursor;
			}
		} break;
		case '(': {
			token.type = TOK_LEFT_PAREN;
			++ parser->cursor;
		} break;
		case ')': {
			token.type = TOK_PAREN_RIGHT;
			++ parser->cursor;
		} break;
		case '{': {
			token.type = TOK_LEFT_BRACE;
			++ parser->cursor;
		} break;
		case '}': {
			token.type = TOK_RIGHT_BRACE;
			++ parser->cursor;
		} break;
		case ',': {
			token.type = TOK_COMMA;
			++ parser->cursor;
		} break;
		case '%': {
			token.type = TOK_MOD;
			++ parser->cursor;
		} break;
		case '^': {
			token.type = TOK_BIT_XOR;
			++ parser->cursor;
		} break;
		case '+': {
			token.type = TOK_ADD;
			++ parser->cursor;
			if (* parser->cursor == '=') {
				token.type = TOK_ADD_ASSIGN;
				++ parser->cursor;
			}
		} break;
		case '~': {
			token.type = TOK_TILDE;
			++ parser->cursor;
		} break;
		case ';': {
			token.type = TOK_SEMICOLON;
			++ parser->cursor;
		} break;
	}

	esc: ;

	/* passive hinting */
	while (*parser->cursor == ' ' || *parser->cursor == '\t') {
		++ parser->cursor;
	}

	// (POS0()==';')
	if (((POS0()=='/')&&((POS1()=='/')||(POS1()=='*')))) {
		token.eol = 1;
	} else if ((POS0()=='\n')||(POS0()=='\r')) {
		token.eol = 1;
	}

	parser->tok_prev = parser->tok;
	parser->tok = parser->tok_prox;
	parser->tok_prox = token;
	return parser->tok_prev;
}
