//
// See Copyright Notice In elf.h
//

// todo: remove!
void elf_get_line_source_info(char *q, char *loc, int *linenum, char **lineloc);


#define POS0() (parser->pos[0])
#define POS1() (parser->pos[1])
#define MOVE() (*(parser->pos ++))
#define MOVEN(n) ((parser->pos += n))
#define PICK(xx) ((POS0() == (xx)) ? (MOVEN(1), 1) : 0)

static void new_line(elf_Parser *parser) {
	parser->line_pos = parser->pos;
	parser->line_num ++;
}


/* todo: speed */
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
	elf_get_line_source_info(parser->text,line,&linenum,&lineloc);

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
		printf("%s [%i:%lli]: %s\n",filename,linenum,(elf_Int)(1+line-lineloc),b);
	}
	printf("| %.*s\n",linelen,lineloc);
	printf("| %.*s\n",underline+1,u);
}


static int pick_esc_char(elf_Parser *parser) {
	int chr = MOVE();
	if (chr == '\\') {

		chr = MOVE();

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
static elf_f64 lex_fractional(elf_Parser *parser) {
	elf_f64 x = 0, y = 1;
	while (is_digit_chr(POS0())) {
		x = x * 10 + (MOVE() - '0');
		y = y * 10;
	}
	return x / y;
}

// assuming we're already at an integer char
static elf_i64 lex_integer(elf_Parser *parser) {

	elf_i64 x = 10, y, z;

	// figure out the base
	if (POS0() == '0') {
		if (POS1() == 'b') MOVEN(2), x =  2; else
		if (POS1() == 'x') MOVEN(2), x = 16;
	}

	for (y = 0, z = -1; ; y = y * x + z) {
		if (WITHIN(POS0(), 'A', 'Z' + 1)) {
			z = 10 + MOVE() - 'A';
			if (x != 16) goto _error;
		} else if (WITHIN(POS0(), 'a', 'z' + 1)) {
			z = 10 + MOVE() - 'A';
			if (x != 16) goto _error;
		} else if (WITHIN(POS0(), '0', '9' + 1)) {
			z = MOVE() - '0';
			if (x == 2 && z > 1) goto _error;
		} else {
			if (0) _error: {
				parser_dialog(parser, parser->pos, "invalid base '%i' for digit", x);
			}
			break;
		}
	}

	return y;
}

// assuming we're at an identifier char
static int lex_identifier(elf_Parser *parser, char *buffer, int capacity) {
	int length = 0;

	do {
		if (length >= capacity) {
			parser_dialog(parser, parser->pos, "identifier is too long");
		}
		buffer[length ++] = MOVE();
	} while (is_letter_or_digit_chr(POS0()) || (POS0() == '_'));

	buffer[length] = 0;
	return length;
}


static int pick_empty_chars(elf_Parser *parser) {
	int lines=0;
	retry:
	switch (*parser->pos) {
		case ' ': case '\t': {
			MOVE();
		} goto retry;
		case '\n': {
			MOVE();
			new_line(parser);
			lines+=1;
		} goto retry;
		case '\r': {
			MOVE();
			PICK('\n');
			new_line(parser);
			lines+=1;
		} goto retry;
	}
	return lines;
}


//
// todo: would it be faster to just do all the tokens
// in one go...
//
static elf_Token get_tok(elf_Parser *parser) {

	char buffer[256];

	retry:
	elf_Token token = {};
	token.type = TK_NONE;
	token.line = parser->pos;

	switch (POS0()) {
		case 'A'...'Z': case 'a'...'z': case '_': {

			int length = lex_identifier(parser, buffer, sizeof(buffer) - 1);

			token.type = check_keyword(buffer);
			if (token.type == TK_WORD) {
				// todo: proper string allocator
				// todo: leak!
				token.text = copy_text2(length, buffer);
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
			MOVE();
			/* todo: use static buffer first */
			char *buffer = 0;
			int   length = 0;
			while (POS0() != 0) {
				while (POS0() != 0 && POS0() != '"') {
					/* are multi-line strings illegal? */
					if (PICK('\n') || (PICK('\r') && (PICK('\n'),1))) {
						new_line(parser);
						ARRAY_ADD(buffer,'\n');
					} else {
						char chr = pick_esc_char(parser);
						ARRAY_ADD(buffer,chr);
					}
				}
				if (!PICK('"')) {
					parser_dialog(parser,token.line,"invalid string");
				}
				int lines=pick_empty_chars(parser);
				if (lines)token.eol=1;
				if (!PICK('"')) {
					break;
				}
				ARRAY_ADD(buffer,'\n');
			}

			ARRAY_ADD(buffer,0);

			// todo: leak.
			// todo: strings can be arbitrarily big so
			// this has to be heap allocated, but could
			// we instead GC allocate this? and then it
			// will get automatically collected if not
			// referenced? Would this cause other unintended
			// effects?
			token.type = TK_STRING;
			token.text = copy_text2(length,buffer);

			ARRAY_DELETE(buffer);
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
			lex_identifier(parser, buffer, sizeof(buffer) - 1);
			token.type = text_is_word_or_macro(buffer);
			if (token.type == TK_M_ENDOFFILE) {
				token.type = TK_NONE;
			} else if (token.type==TK_M_FILE_NAME) {
				token.type = TK_STRING;
				token.text = parser->name;
			} else if (token.type == TK_M_LINE_NUMBER) {
				token.type = TK_INTEGER;
				token.integer = parser->line_num;
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
			parser->line_pos = parser->pos;
			parser->line_num += 1;
		} goto retry;
		case '\r': {
			MOVE();
			PICK('\n');
			parser->line_pos = parser->pos;
			parser->line_num += 1;
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
						parser->line_pos = parser->pos;
						parser->line_num += 1;
					} else
					if (PICK('\r')) {
						PICK('\n');
						parser->line_pos = parser->pos;
						parser->line_num += 1;
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

		CASE('<', TK_LT
		, 	 PICK('='), TK_LTEQ, 0, 0
		,   PICK('<'), TK_SHL,  0, 0)

		CASE(':', TK_COLON
		, 	 PICK(':'), TK_COLON, PICK('='), TK_HARD_BIND
		,   PICK('='), TK_BIND ,         0,            0)

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

		LEX1('[', TK_SQUARE_LEFT);
		LEX1(']', TK_SQUARE_RIGHT);
		LEX1('(', TK_PAREN_LEFT);
		LEX1(')', TK_PAREN_RIGHT);
		LEX1('{', TK_CURLY_LEFT);
		LEX1('}', TK_CURLY_RIGHT);
		LEX1(',', TK_COMMA);
		LEX1('%', TK_MOD);
		LEX1('^', TK_BIT_XOR);
		LEX1('+', TK_ADD);
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
