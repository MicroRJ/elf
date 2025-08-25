//
// See Copyright Notice In elf.h
//


// todo: unary operator and keyword isnil
// if table isnil == isnil table ? {
//
// }
// todo: experiment with pack16, pack32, pack64 macros,
// I think they could be useful!


// todo: avoid using these macros, they're ugly
#define POS0() (parser->cursor[0])
#define POS1() (parser->cursor[1])
#define MOVE() (*(parser->cursor ++))
#define MOVEN(n) ((parser->cursor += n))
#define PICK(xx) ((POS0() == (xx)) ? (MOVEN(1), 1) : 0)


/* todo: binary search or something goofy */
static tokenTy text_is_word_or_macro(char *name) {
#define MCITEM(NAME,SYM) if (text_eq(SYM,name)) return XFUSE(TK_M_,NAME);
	MACRODEF(MCITEM)
#undef MCITEM
	return TK_WORD;
}

/* todo: speed! */
static tokenTy check_keyword(char *name) {
	#define KWITEM(NAME,SYM) if (text_eq(SYM,name)) return XFUSE(TK_,NAME);
	KEYWORDDEF(KWITEM)
	#undef KWITEM
	return TK_WORD;
}

//
// todo: we already have one of these somewhere, recycle!
//
static void parser_dialog(elf_Parser *parser, char *line, char const *fmt, ...) {
	line = line ? line : parser->tok.line;

	// | attempted to get field of
	//
	// | name := 0
	// | name.name = 0
	// | ^
	// |
	// |

	int linenum;
	char *lineloc;
	get_source_info(parser->source,line,&linenum,&lineloc);

	/* skip initial blank characters for optimal gimmicky */
	while (*lineloc == '\t' || *lineloc == ' ') {
		lineloc += 1;
	}

	char u[0x40];

	int underline = line - lineloc;
	if (underline >= sizeof(u)) {
		underline = sizeof(u) - 1;
		lineloc = line - underline;
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
		printf("%s [%i:%lli]: %s\n",filename,linenum,(elf_Integer)(1+line-lineloc),b);
	}
	printf("| %.*s\n",linelen,lineloc);
	printf("| %.*s\n",underline+1,u);
}


static int pick_esc_char(elf_Parser *parser) {
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
static Num lex_fractional(elf_Parser *parser) {
	Num x = 0, y = 1;
	while (is_digit_chr(*parser->cursor)) {
		x = x * 10 + (*parser->cursor ++ - '0');
		y = y * 10;
	}
	return x / y;
}



// assuming we're already at an integer char
static Int lex_integer(elf_Parser *parser) {

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
static int lex_identifier(elf_Parser *parser, char *buf, int cap) {
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







static int pick_empty_chars(elf_Parser *parser) {
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




//
// todo: would it be faster to just do all the tokens
// in one go...
//
static Token next_tok(elf_Parser *parser) {

	retry:
	Token token = {};
	token.type = TK_NONE;
	token.line = parser->cursor;

	switch (*parser->cursor) {

		case 'A'...'Z': case 'a'...'z': case '_': {
			if (parser->cursor[0] == 'f' && parser->cursor[1] == '"') {
				parser->cursor ++;
				token.type = TK_FORMAT_STRING;
				goto strcase;
			}

			int length = lex_identifier(parser, parser->tempbuf, sizeof(parser->tempbuf) - 1);

			token.type = check_keyword(parser->tempbuf);
			if (token.type == TK_WORD) {
				// todo: proper string allocator
				// todo: leak!
				token.text = copy_text2(length, parser->tempbuf);
			}

		} break;
		case '0'...'9': {

			elf_i64 x = lex_integer(parser);

			token.type = TK_INTEGER;
			token.integer = x;

			// post-decimal part, ensure we don't match a ".."
			if ((POS0() == '.') && (POS1() != '.')) {

				MOVE();

				elf_f64 y = lex_fractional(parser);

				token.type = TK_NUMBER;
				token.number = x + y;
			}
		} break;
		case '\'': {
			MOVE();

			// todo!!: come back to this!
			token.type = TK_LETTER;
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
						darr_add(buffer,'\n');
					} else {
						// don't check for format
						if (token.type != TK_FORMAT_STRING) goto escchar;

						// todo: make it so that we can escape the formatting
						if (POS0() == FORMAT_CHAR && POS1() == '{') {
							needsformatting = true;
							darr_add(buffer, *parser->cursor ++);
							darr_add(buffer, *parser->cursor ++);
						} else {
							escchar:
							int chr = pick_esc_char(parser);
							darr_add(buffer, chr);
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
					darr_add(buffer,'\n');
				} else break;
			}

			darr_add(buffer,0);

			// todo: leak, allocate this properly in some sort
			// of constant pool with intering, use the atomizer
			// that way we avoid going thru the GC, if during
			// parsing we figure out the thing is dead, we dealloc it
			if (!needsformatting || token.type != TK_FORMAT_STRING) {
				token.type = TK_STRING;
			}
			token.text = copy_text2(length,buffer);
			darr_free(buffer);
		} break;
		case '.': {

			MOVE();
			token.type = TK_DOT;

			// todo: so there's no difference between "..." and ".."
			if (PICK('.')) {
				token.type = TK_DOT_DOT;
				if (PICK('.')) {
					token.type = TK_DOT_DOT;
				}
			} else if (is_digit_chr(POS0())) {
				token.type = TK_NUMBER;
				token.number = lex_fractional(parser);
			}
		} break;
		case '#': {
			MOVE();
			lex_identifier(parser, parser->tempbuf, sizeof(parser->tempbuf) - 1);
			token.type = text_is_word_or_macro(parser->tempbuf);
			if (token.type == TK_M_ENDOFFILE) {
				token.type = TK_NONE;
			} else if (token.type==TK_M_FILE_NAME) {
				token.type = TK_STRING;
				token.text = parser->name;
			} else if (token.type == TK_M_LINE_NUMBER) {
				// todo: get this from the source location!
				__debugbreak();

				token.type = TK_INTEGER;
				token.integer = -1;
			} else if (token.type == TK_WORD) {
				parser_dialog(parser,token.line,"unrecognized macro");
			} else {
				/* let parser handle this */
			}
		} break;
		case '\0': {
			token.type = TK_NONE;
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
		case ';': {
			MOVE();
			while (POS0() != 0 && !is_eol_chr(POS0())) {
				MOVE();
			}
			goto retry;
		} break;



		case '/': {
			MOVE();
			token.type = TK_DIV;
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


		#define ROW(A, X, B, Y) if (A) { token.type = X; if (B) { token.type = Y; } }

		#define COL(A, X, B, Y, C, Z, D, W) ROW(A, X, B, Y) else ROW(C, Z, D, W)

		#define CASE(A, TA, B, TB, BB, TBB, C, TC, CC, TCC) case (A): { MOVE(); token.type = TA; COL(B, TB, BB, TBB, C, TC, CC, TCC) } break;

#define LEX1(A,X)         CASE(A, X,       0, 0, 0, 0,       0, 0, 0, 0)
#define LEX2(A,X,B,Y)     CASE(A, X, PICK(B), Y, 0, 0,       0, 0, 0, 0)
#define LEX3(A,X,B,Y,C,Z) CASE(A, X, PICK(B), Y, 0, 0, PICK(C), Z, 0, 0)

		CASE('[', TK_SQUARE_LEFT
		, 		PICK('['), TK_SQUARE_SQUARE_LEFT, 0, 0
		, 		0,                             0, 0, 0);
		CASE(']', TK_SQUARE_RIGHT
		, 		PICK(']'), TK_SQUARE_SQUARE_RIGHT, 0, 0
		, 		0,                              0, 0, 0);


		CASE('<', TK_LT
		, 	 PICK('='), TK_LTEQ, 0, 0
		,   PICK('<'), TK_SHL,  0, 0)

		CASE(':', TK_COLON
		, 	 PICK(':'), TK_STATIC_BIND, PICK('='), TK_HARD_BIND
		,   PICK('='),       TK_BIND ,         0,            0)

		CASE('-', TK_SUB
		, 	 PICK('-'), TK_MINUS_MINUS, PICK('>'), TK_HARD_ARROW
		,   PICK('>'),       TK_ARROW,         0,             0)


		LEX3('>',TK_GT                , '=', TK_GTEQ    , '>', TK_SHR);
		LEX3('?',TK_QMARK             , '?', TK_NIL_OR  , '=', TK_NIL_ASSIGN);
		LEX3('!',TK_EXCLAMATION_MARK  , '!', TK_NIL_AND , '=', TK_NEQ);

		LEX2('|', TK_BIT_OR  , '|', TK_LOG_OR);
		LEX2('&', TK_BIT_AND , '&', TK_LOG_AND);
		LEX2('=', TK_ASSIGN  , '=', TK_EQ);
		LEX2('*', TK_MUL     , '*', TK_POW);

		LEX1('(', TK_PAREN_LEFT);
		LEX1(')', TK_PAREN_RIGHT);
		LEX1('{', TK_CURLY_LEFT);
		LEX1('}', TK_CURLY_RIGHT);
		LEX1(',', TK_COMMA);
		LEX1('%', TK_MOD);
		LEX1('^', TK_BIT_XOR);
		LEX1('+', TK_ADD);
		LEX1('~', TK_TILDE);
	}

	esc: ;

	/* passive hinting */
	while ((POS0()==' ')||(POS0()=='\t')) {
		MOVE();
	}
	if ((POS0()==';')||((POS0()=='/')&&((POS1()=='/')||(POS1()=='*')))) {
		token.eol = 1;
	} else if ((POS0()=='\n')||(POS0()=='\r')) {
		token.eol = 1;
	}

	parser->tok_prev = parser->tok;
	parser->tok = parser->tok_prox;
	parser->tok_prox = token;
	return parser->tok_prev;
}
