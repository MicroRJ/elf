/*
** See Copyright Notice In elf.h
** lexer.c
*/



#define elf_thischr() (file->thischar[0])
#define elf_thenchr() (file->thischar[1])
#define move_char() (*(file->thischar ++))
#define elf_movxchr(n) ((file->thischar += n))
#define pick_char(xx) ((elf_thischr() == (xx)) ? move_char(), 1 : 0)



static void new_line(FileState *fs) {
	fs->linechar = fs->thischar;
	fs->linenumber += 1;
}


/* todo: speed */
static elTokenType text_is_word_or_macro(char *name) {
#define MCITEM(NAME,SYM) if (text_eq(SYM,name)) return FUSE(TK_M_,NAME);
	MCLIST(MCITEM)
#undef MCITEM
	return TK_WORD;
}


static elTokenType text_is_word_or_keyword(char *name) {
	#define KWITEM(NAME,SYM) if (text_eq(SYM,name)) return FUSE(TK_,NAME);
	KWLIST(KWITEM)
	#undef KWITEM
	return TK_WORD;
}


typedef struct t_token_info {
	char *name;
	char prec;
} t_token_info;


elGLOBAL t_token_info elf_token_intel[] = {
	{"none",-2},
#define TKITEM(_,SYM) {SYM,-2},
#define OPITEM(_,SYM,PRC) {SYM,PRC},
	KWLIST(TKITEM)
	MCLIST(TKITEM)
	TKLIST(TKITEM)
	OPLIST(OPITEM)
};


void file_dialog(FileState *fs, char *line, char const *fmt, ...) {
	line = line ? line : fs->this_token.line;

	int linenum;
	char *lineloc;
	elf_get_line_source_info(fs->filetext,line,&linenum,&lineloc);

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
		char *filename = fs_get_name(fs);
		printf("%s [%i:%lli]: %s\n",filename,linenum,(elInteger)(1+line-lineloc),b);
	}
	printf("| %.*s\n",linelen,lineloc);
	printf("| %.*s\n",underline+1,u);
}


static int pick_esc_char(FileState *file) {
	int tk = move_char();
	if (tk != '\\') return tk;
	tk = move_char();
	switch (tk) {
		case '\\': return '\\';
		case '\0': return '\0';
		case 't':  return '\t';
		case 'n':  return '\n';
		case 'r':  return '\r';
	}
	return tk;
}


static int lex_word(FileState *file, char *buffer) {
	int length = 0;
	do {
		buffer[length++] = move_char();
	} while (is_letter_or_digit_chr(elf_thischr()) || (elf_thischr() == '_'));
	buffer[length] = 0;
	return length;
}


static void pick_empty_chars(FileState *file) {
	retry:
	switch (*file->thischar) {
		case ' ': case '\t': {
			move_char();
		} goto retry;
		case '\n': {
			move_char();
			new_line(file);
		} goto retry;
		case '\r': {
			move_char();
			pick_char('\n');
			new_line(file);
		} goto retry;
	}
}


elNumber lex_number(FileState *file, int base) {
	elNumber N=0,P=1;
	do {
		N=N*base+(move_char()-'0');
		P=P*base;
	} while(is_digit_chr(elf_thischr()));
	return N/P;
}


/* not the fastest thing out there */
FileToken poll_token(FileState *file) {
	elGLOBAL char buffer[0x100];

	FileToken tk;

	retry:
	elf_clear_memory(&tk,sizeof(tk));
	tk.type=TK_NONE;
	tk.line=file->thischar;

	switch (elf_thischr()) {
		/* not sure how portable this is */
		case 'A'...'Z': case 'a'...'z': case '_': {
			int length = 0;
			do {
				ASSERT(length < 0xff);
				buffer[length++] = move_char();
			} while (is_letter_or_digit_chr(elf_thischr()) || (elf_thischr() == '_'));
			buffer[length] = 0;
			/* todo: string interner please */
			tk.type=text_is_word_or_keyword(buffer);
			if (tk.type==TK_WORD) {
				tk.text=copy_text2(HEAP_ALLOCATOR,length,buffer);
			}
		} break;
		case '0'...'9': {
			tk.type = TK_INTEGER;
			elInteger B,I,C;

			B=10;
			if (elf_thischr()=='0') {
				if (elf_thenchr()=='b') elf_movxchr(2),B=2; else
				if (elf_thenchr()=='x') elf_movxchr(2),B=16;
			}

			for (I = 0, C = -1; ; I = I * B + C) {
				if (WITHIN(elf_thischr(),'A','Z'+1)) {
					C = 10 + move_char() - 'A';
					if (B != 16) goto _error;
				} else if (WITHIN(elf_thischr(),'a','z'+1)) {
					C = 10 + move_char() - 'A';
					if (B != 16) goto _error;
				} else if (WITHIN(elf_thischr(),'0','9'+1)) {
					C = move_char() - '0';
					if (B == 2 && C > 1) goto _error;
				} else {
					if (0) _error: {
						file_dialog(file, file->thischar, "invalid base '%i' for digit", B);
					}
					break;
				}
			}
			tk.integer=I;
			/* lex decimal part */
			elNumber P,N;
			if ((elf_thischr()=='.')&&(elf_thenchr()!='.')) {
				move_char();
				tk.type=TK_NUMBER;
				P=1,N=0;
				while (is_digit_chr(elf_thischr())) {
					N=N*10+(move_char()-'0');
					P=N*10;
				}
				tk.number = I + N / P;
				goto esc;
			}
		} break;
		case '\'': {
			move_char();
			tk.type = TK_LETTER;
			do { tk.integer = move_char();
			} while(0);
			if (!pick_char('\'')) {
				file_dialog(file,tk.line,"invalid character constant, expected \"'\"");
			}
		} break;
		case '"': {
			move_char();
			/* todo: use static buffer first */
			char *buffer = 0;
			int   length = 0;
			while (elf_thischr() != 0) {
				while (elf_thischr() != 0 && elf_thischr() != '"') {
					/* are multi-line strings illegal? */
					if (pick_char('\n') || (pick_char('\r') && (pick_char('\n'),1))) {
						new_line(file);
						ARRAY_ADD(buffer,'\n');
					} else {
						char chr = pick_esc_char(file);
						ARRAY_ADD(buffer,chr);
					}
				}
				if (!pick_char('"')) {
					file_dialog(file,tk.line,"invalid string");
				}
				pick_empty_chars(file);
				if (!pick_char('"')) {
					break;
				}
				ARRAY_ADD(buffer,'\n');
			}

			ARRAY_ADD(buffer,0);

			tk.type = TK_STRING;
			tk.text = copy_text2(HEAP_ALLOCATOR,length,buffer);
		} break;
		case '.': { move_char(); tk.type = TK_DOT;
			if (pick_char('.')) { tk.type = TK_DOT_DOT;
				/* todo: eventually rename use TK_ELLIPSIS */
				if (pick_char('.'))   tk.type = TK_DOT_DOT;
			} else if (is_digit_chr(elf_thischr())) {
				tk.type = TK_NUMBER;
				elNumber n = 0;
				elNumber p = 1;
				do {
					n = n * 10 + (move_char() - '0');
					p *= 10;
				} while (is_digit_chr(elf_thischr()));
				tk.number = n / p;
			}
		} break;
		case '#': {
			move_char();
			lex_word(file,buffer);
			tk.type=text_is_word_or_macro(buffer);
			if (tk.type==TK_M_ENDOFFILE) {
				tk.type=TK_NONE;
			} else if (tk.type==TK_M_FILE_NAME) {
				tk.type=TK_STRING;
				tk.text=fs_get_name(file);
			} else if (tk.type==TK_M_LINE_NUMBER) {
				tk.type=TK_INTEGER;
				tk.integer=file->linenumber;
			} else if (tk.type==TK_WORD) {
				file_dialog(file,tk.line,"unrecognized macro");
			} else {
				/* let parser handle this */
			}
		} break;
		case '\0': {
			tk.type = TK_NONE;
		} break;
		case ' ': case '\t': {
			move_char();
		} goto retry;
		case '\n': {
			move_char();
			file->linechar = file->thischar;
			file->linenumber += 1;
		} goto retry;
		case '\r': {
			move_char();
			pick_char('\n');
			file->linechar = file->thischar;
			file->linenumber += 1;
		} goto retry;
		case ';': {
			move_char();
			while (elf_thischr() != 0 && !is_eol_chr(elf_thischr())) {
				move_char();
			}
			goto retry;
		} break;


		case '<': {
			move_char();
			tk.type = TK_LT;
			if (pick_char('=')) {
				tk.type = TK_LTEQ;
			} else
			if (pick_char('<')) {
				tk.type = TK_SHL;
			}
		} break;
		case '>': { move_char(); tk.type = TK_GT;
			if (pick_char('=')) tk.type = TK_GTEQ;
			else if (pick_char('>')) tk.type = TK_SHR;
		} break;
		case '?': { move_char(); tk.type = TK_QMARK;
			if (pick_char('?')) tk.type = TK_NIL_OR;
			else if (pick_char('=')) tk.type = TK_NIL_ASSIGN;
		} break;
		case '!': { move_char(); tk.type = TK_EXCLAMATION_MARK;
			if (pick_char('!')) tk.type = TK_NIL_AND;
			else if (pick_char('=')) tk.type = TK_NEQ;
		} break;
		case '*': { move_char(); tk.type = TK_MUL;
			if (pick_char('*')) tk.type = TK_POW;
		} break;

		#define TK_XCASE2(C0,T0,C1,T1) \
		case C0: {                     \
			move_char();                  \
			tk.type = T0;               \
			if (pick_char(C1)) {          \
				tk.type = T1;            \
			}                           \
		} break;

		TK_XCASE2('|',TK_BIT_OR,'|',TK_LOG_OR);
		TK_XCASE2('&',TK_BIT_AND,'&',TK_LOG_AND);
		TK_XCASE2('=',TK_ASSIGN,'=',TK_EQ);

		#undef TK_XCASE2

		#define TK_XCASE1(C,T) \
		case C: {              \
			move_char();          \
			tk.type = T;        \
		} break

		TK_XCASE1('[',TK_SQUARE_LEFT);
		TK_XCASE1(']',TK_SQUARE_RIGHT);
		TK_XCASE1('(',TK_PAREN_LEFT);
		TK_XCASE1(')',TK_PAREN_RIGHT);
		TK_XCASE1('{',TK_CURLY_LEFT);
		TK_XCASE1('}',TK_CURLY_RIGHT);
		TK_XCASE1(',',TK_COMMA);
		TK_XCASE1('%',TK_MOD);
		TK_XCASE1(':',TK_COLON);
		TK_XCASE1('^',TK_BIT_XOR);
		TK_XCASE1('-',TK_SUB);
		TK_XCASE1('+',TK_ADD);

		#undef TK_XCASE1

		case '/': {
			move_char();
			tk.type = TK_DIV;
			if (pick_char('*')) {
				for (;;) {
					/* handle lines */
					if (pick_char('\n')) {
						file->linechar = file->thischar;
						file->linenumber += 1;
					} else
					if (pick_char('\r')) {
						pick_char('\n');
						file->linechar = file->thischar;
						file->linenumber += 1;
					} else
					if (pick_char('*')) {
						if (pick_char('/')) {
							/* end of comment */
							break;
						}
					} else {
						/* keep skipping! */
						move_char();
					}
				}
				goto retry;
			} else
			if (pick_char('/')) {
				while (elf_thischr() != 0 && !is_eol_chr(elf_thischr())) {
					move_char();
				}
				goto retry;
			}
		} break;
	}

	esc: ;

	/* passive hinting */
	while ((elf_thischr()==' ')||(elf_thischr()=='\t')) {
		move_char();
	}
	if ((elf_thischr()==';')||((elf_thischr()=='/')&&((elf_thenchr()=='/')||(elf_thenchr()=='*')))) {
		tk.eol = 1;
	} else if ((elf_thischr()=='\n')||(elf_thischr()=='\r')) {
		tk.eol = 1;
	}

	file->last_token = file->this_token;
	file->this_token = file->then_token;
	file->then_token = tk;
	return file->last_token;
}