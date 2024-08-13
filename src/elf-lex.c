/*
** See Copyright Notice In elf.h
** llexer.c
** Lexical Analyzer
*/

#undef TRUE
#undef FALSE

elTokenType elf_textiswordormacro(char *name) {
	#define MCITEM(NAME,SYM) if (elf_texteq(SYM,name)) return elFUSE(TK_M_,NAME);
	MCLIST(MCITEM)
	#undef MCITEM
	return TK_WORD;
}


elTokenType elf_is_word_or_keyword(char *name) {
	#define KWITEM(NAME,SYM) if (elf_texteq(SYM,name)) return elFUSE(TK_,NAME);
	KWLIST(KWITEM)
	#undef KWITEM
	return TK_WORD;
}


#define TRUE  1
#define FALSE 0



typedef struct ltokenintel {
	char *name;
	char prec;
} ltokenintel;


elGLOBAL ltokenintel elf_token_intel[] = {
	{"none",-2},
#define TKITEM(_,SYM) {SYM,-2},
#define OPITEM(_,SYM,PRC) {SYM,PRC},
	KWLIST(TKITEM)
	MCLIST(TKITEM)
	TKLIST(TKITEM)
	OPLIST(OPITEM)
};





void elf_fdialog(elFileState *fs, char *line, char const *fmt, ...) {
	line = line ? line : fs->this_token.line;

	int linenum;
	char *lineloc;
	elf_get_line_location_info(fs->filetext,line,&linenum,&lineloc);

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
		char *filename = elf_fget_name(fs);
		printf("%s [%i:%lli]: %s\n",filename,linenum,(elInteger)(1+line-lineloc),b);
	}
	printf("| %.*s\n",linelen,lineloc);
	printf("| %.*s\n",underline+1,u);
}


#define elf_thischr() (file->thischar[0])
#define elf_thenchr() (file->thischar[1])
#define elf_movechr() (*(file->thischar ++))
#define elf_movxchr(n) ((file->thischar += n))
#define elf_cmovchr(xx) ((elf_thischr() == (xx)) ? elf_movechr(), 1 : 0)



int elf_lexescchr(elFileState *file) {
	int tk = elf_movechr();
	if (tk != '\\') return tk;
	tk = elf_movechr();
	switch (tk) {
		case '\\': return '\\';
		case '\0': return '\0';
		case 't':  return '\t';
		case 'n':  return '\n';
		case 'r':  return '\r';
	}
	return tk;
}


int elf_lextext(elFileState *file, char *buffer) {
	int length = 0;
	do {
		buffer[length++] = elf_movechr();
	} while (elf_is_letter_or_digit_char(elf_thischr()) || (elf_thischr() == '_'));
	buffer[length] = 0;
	return length;
}


void elf_flexnewline(elFileState *fs) {
	fs->linechar = fs->thischar;
	fs->linenumber += 1;
}


void elf_lexer_get_emptychr(elFileState *file) {
	retry:
	switch (elf_thischr()) {
		case ' ': case '\t': {
			elf_movechr();
		} goto retry;
		case '\n': {
			elf_movechr();
			elf_flexnewline(file);
		} goto retry;
		case '\r': {
			elf_movechr();
			elf_cmovchr('\n');
			elf_flexnewline(file);
		} goto retry;
	}
}


/* not the fastest thing out there */
elToken elf_poll_token(elFileState *file) {
	/* identifiers can only be 255 characters long
	(256 - 1 null terminator), for no reason... */
	elGLOBAL char buffer[0x100];

	elToken tk;

	retry:
	elf_clear_memory(&tk,sizeof(tk));
	tk.type=TK_NONE;
	tk.line=file->thischar;

	switch (elf_thischr()) {
		/* not sure how portable this is */
		case 'A'...'Z': case 'a'...'z': case '_': {
			int length = 0;
			do {
				elASSERT(length < 0xff);
				buffer[length++] = elf_movechr();
			} while (elf_is_letter_or_digit_char(elf_thischr()) || (elf_thischr() == '_'));
			buffer[length] = 0;
			/* todo: string interner please */
			tk.type=elf_is_word_or_keyword(buffer);
			if (tk.type==TK_WORD) {
				tk.text=elf_copyltext(elHEAP_ALLOCATOR,length,buffer);
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
				if (elWITHIN(elf_thischr(),'A','Z'+1)) {
					C = 10 + elf_movechr() - 'A';
					if (B != 16) goto _error;
				} else if (elWITHIN(elf_thischr(),'a','z'+1)) {
					C = 10 + elf_movechr() - 'A';
					if (B != 16) goto _error;
				} else if (elWITHIN(elf_thischr(),'0','9'+1)) {
					C = elf_movechr() - '0';
					if (B == 2 && C > 1) goto _error;
				} else {
					if (0) _error: {
						elf_fdialog(file, file->thischar, "invalid base '%i' for digit", B);
					}
					break;
				}
			}
			tk.integer=I;
			/* lex decimal part */
			elNumber P,N;
			if ((elf_thischr()=='.')&&(elf_thenchr()!='.')) {
				elf_movechr();
				tk.type=TK_NUMBER;
				P=1,N=0;
				while (elf_is_digit_char(elf_thischr())) {
					N=N*10+(elf_movechr()-'0');
					P=N*10;
				}
				tk.number = I + N / P;
				goto esc;
			}
		} break;
		case '\'': {
			elf_movechr();
			tk.type = TK_LETTER;
			do { tk.integer = elf_movechr();
			} while(0);
			if (!elf_cmovchr('\'')) {
				elf_fdialog(file,tk.line,"invalid character constant, expected \"'\"");
			}
		} break;
		case '"': {
			elf_movechr();
			/* todo: use static buffer first */
			char *buffer = 0;
			int   length = 0;
			while (elf_thischr() != 0) {
				while (elf_thischr() != 0 && elf_thischr() != '"') {
					/* are multi-line strings illegal? */
					if (elf_cmovchr('\n') || (elf_cmovchr('\r') && (elf_cmovchr('\n'),1))) {
						elf_flexnewline(file);
						ARRAY_ADD(buffer,'\n');
					} else {
						char chr = elf_lexescchr(file);
						ARRAY_ADD(buffer,chr);
					}
				}
				if (!elf_cmovchr('"')) {
					elf_fdialog(file,tk.line,"invalid string");
				}
				elf_lexer_get_emptychr(file);
				tk.eol=1;
				if (!elf_cmovchr('"')) {
					break;
				}
				ARRAY_ADD(buffer,'\n');
			}

			ARRAY_ADD(buffer,0);

			tk.type = TK_STRING;
			tk.text = elf_copyltext(elHEAP_ALLOCATOR,length,buffer);
		} break;
		case '.': { elf_movechr(); tk.type = TK_DOT;
			if (elf_cmovchr('.')) { tk.type = TK_DOT_DOT;
				/* todo: eventually rename use TK_ELLIPSIS */
				if (elf_cmovchr('.'))   tk.type = TK_DOT_DOT;
			} else if (elf_is_digit_char(elf_thischr())) {
				tk.type = TK_NUMBER;
				elNumber n = 0;
				elNumber p = 1;
				do {
					n = n * 10 + (elf_movechr() - '0');
					p *= 10;
				} while (elf_is_digit_char(elf_thischr()));
				tk.number = n / p;
			}
		} break;
		case '#': {
			elf_movechr();
			elf_lextext(file,buffer);
			tk.type=elf_textiswordormacro(buffer);
			if (tk.type==TK_M_ENDOFFILE) {
				tk.type=TK_NONE;
			} else if (tk.type==TK_M_FILE_NAME) {
				tk.type=TK_STRING;
				tk.text=elf_fget_name(file);
			} else if (tk.type==TK_M_LINE_NUMBER) {
				tk.type=TK_INTEGER;
				tk.integer=file->linenumber;
			} else if (tk.type==TK_WORD) {
				elf_fdialog(file,tk.line,"unrecognized macro");
			} else {
				/* let parser handle this */
			}
		} break;
		case '\0': {
			tk.type = TK_NONE;
		} break;
		case ' ': case '\t': {
			elf_movechr();
		} goto retry;
		case '\n': {
			elf_movechr();
			file->linechar = file->thischar;
			file->linenumber += 1;
		} goto retry;
		case '\r': {
			elf_movechr();
			elf_cmovchr('\n');
			file->linechar = file->thischar;
			file->linenumber += 1;
		} goto retry;
		case ';': {
			elf_movechr();
			while (elf_thischr() != 0 && !elf_chriseol(elf_thischr())) {
				elf_movechr();
			}
			goto retry;
		} break;


		case '<': {
			elf_movechr();
			tk.type = TK_LT;
			if (elf_cmovchr('=')) {
				tk.type = TK_LTEQ;
			} else
			if (elf_cmovchr('<')) {
				tk.type = TK_SHL;
			}
		} break;
		case '>': { elf_movechr(); tk.type = TK_GT;
			if (elf_cmovchr('=')) tk.type = TK_GTEQ;
			else if (elf_cmovchr('>')) tk.type = TK_SHR;
		} break;
		case '?': { elf_movechr(); tk.type = TK_QMARK;
			if (elf_cmovchr('?')) tk.type = TK_NIL_OR;
			else if (elf_cmovchr('=')) tk.type = TK_NIL_ASSIGN;
		} break;
		case '!': { elf_movechr(); tk.type = TK_EXCLAMATION_MARK;
			if (elf_cmovchr('!')) tk.type = TK_NIL_AND;
			else if (elf_cmovchr('=')) tk.type = TK_NEQ;
		} break;
		case '*': { elf_movechr(); tk.type = TK_MUL;
			if (elf_cmovchr('*')) tk.type = TK_POW;
		} break;

		#define TK_XCASE2(C0,T0,C1,T1) \
		case C0: {                     \
			elf_movechr();                  \
			tk.type = T0;               \
			if (elf_cmovchr(C1)) {          \
				tk.type = T1;            \
			}                           \
		} break;

		TK_XCASE2('|',TK_BIT_OR,'|',TK_LOG_OR);
		TK_XCASE2('&',TK_BIT_AND,'&',TK_LOG_AND);
		TK_XCASE2('=',TK_ASSIGN,'=',TK_EQ);

		#undef TK_XCASE2

		#define TK_XCASE1(C,T) \
		case C: {              \
			elf_movechr();          \
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
			elf_movechr();
			tk.type = TK_DIV;
			if (elf_cmovchr('*')) {
				for (;;) {
					/* handle lines */
					if (elf_cmovchr('\n')) {
						file->linechar = file->thischar;
						file->linenumber += 1;
					} else
					if (elf_cmovchr('\r')) {
						elf_cmovchr('\n');
						file->linechar = file->thischar;
						file->linenumber += 1;
					} else
					if (elf_cmovchr('*')) {
						if (elf_cmovchr('/')) {
							/* end of comment */
							break;
						}
					} else {
						/* keep skipping! */
						elf_movechr();
					}
				}
				goto retry;
			} else
			if (elf_cmovchr('/')) {
				while (elf_thischr() != 0 && !elf_chriseol(elf_thischr())) {
					elf_movechr();
				}
				goto retry;
			}
		} break;
	}

	esc: ;

	/* passive hinting */
	while ((elf_thischr()==' ')||(elf_thischr()=='\t')) {
		elf_movechr();
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