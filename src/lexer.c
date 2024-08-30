/*
** See Copyright Notice In elf.h
** lexer.c
** not the fastest thing out there
*/



#define CHARA()   (file->thischar[0])
#define CHARB()   (file->thischar[1])
#define MOVE()  (*(file->thischar ++))
#define MOVEN(n) ((file->thischar += n))
#define PICK(xx) ((CHARA()==(xx))?MOVE(),1:0)

#define LEX1(C,T) case C:MOVE();tk.type=T;break
#define LEX2(A,X,B,Y) case A:MOVE();tk.type=X;if(PICK(B))tk.type=Y;break
#define LEX3(A,X,B,Y,C,Z) case A:MOVE();tk.type=X;if(PICK(B))tk.type=Y;else if(PICK(C))tk.type=Z;break



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


GLOBAL t_token_info elf_token_intel[] = {
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
		printf("%s [%i:%lli]: %s\n",filename,linenum,(elf_Int)(1+line-lineloc),b);
	}
	printf("| %.*s\n",linelen,lineloc);
	printf("| %.*s\n",underline+1,u);
}


static int pick_esc_char(FileState *file) {
	int tk = MOVE();
	if (tk != '\\') return tk;
	tk = MOVE();
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
		buffer[length++] = MOVE();
	} while (is_letter_or_digit_chr(CHARA()) || (CHARA() == '_'));
buffer[length] = 0;
return length;
}


static int pick_empty_chars(FileState *file) {
	int lines=0;
	retry:
	switch (*file->thischar) {
		case ' ': case '\t': {
			MOVE();
		} goto retry;
		case '\n': {
			MOVE();
			new_line(file);
			lines+=1;
		} goto retry;
		case '\r': {
			MOVE();
			PICK('\n');
			new_line(file);
			lines+=1;
		} goto retry;
	}
	return lines;
}


elf_Num lex_number(FileState *file, int base) {
	elf_Num N=0,P=1;
	do {
		N=N*base+(MOVE()-'0');
		P=P*base;
	} while(is_digit_chr(CHARA()));
return N/P;
}


FileToken poll_token(FileState *file) {
	GLOBAL char buffer[0x100];

	FileToken tk;

	retry:
	clear_memory(&tk,sizeof(tk));
	tk.type=TK_NONE;
	tk.line=file->thischar;

	/* not sure how portable this is */
	switch (CHARA()) {

	case 'A'...'Z': case 'a'...'z': case '_': {
		int length = 0;
		do {
			ASSERT(length < 0xff);
			buffer[length++] = MOVE();
		} while (is_letter_or_digit_chr(CHARA()) || (CHARA() == '_'));
		buffer[length] = 0;
		tk.type=text_is_word_or_keyword(buffer);
		if (tk.type==TK_WORD) {
				/* todo: string interner please */
			tk.text=copy_text2(HEAP_ALLOCATOR,length,buffer);
		}
	} break;
	case '0'...'9': {
		tk.type = TK_INTEGER;
		elf_Int B,I,C;

		B=10;
		if (CHARA()=='0') {
			if (CHARB()=='b') MOVEN(2),B=2; else
			if (CHARB()=='x') MOVEN(2),B=16;
		}

		for (I = 0, C = -1; ; I = I * B + C) {
			if(WITHIN(CHARA(),'A','Z'+1)) {
				C = 10 + MOVE() - 'A';
				if (B != 16) goto _error;
			} else if(WITHIN(CHARA(),'a','z'+1)) {
				C = 10 + MOVE() - 'A';
				if (B != 16) goto _error;
			} else if(WITHIN(CHARA(),'0','9'+1)) {
				C = MOVE() - '0';
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
		elf_Num P,N;
		if ((CHARA()=='.')&&(CHARB()!='.')) {
			MOVE();
			tk.type=TK_NUMBER;
			P=1,N=0;
			while (is_digit_chr(CHARA())) {
				N=N*10+(MOVE()-'0');
				P=P*10;
			}
			tk.number = I + N / P;
			goto esc;
		}
	} break;
	case '\'': {
		MOVE();
		tk.type = TK_LETTER;
		do { tk.integer = MOVE();
		} while(0);
		if (!PICK('\'')) {
			file_dialog(file,tk.line,"invalid character constant, expected \"'\"");
		}
	} break;
	case '"': {
		MOVE();
			/* todo: use static buffer first */
		char *buffer = 0;
		int   length = 0;
		while (CHARA() != 0) {
			while (CHARA() != 0 && CHARA() != '"') {
					/* are multi-line strings illegal? */
				if (PICK('\n') || (PICK('\r') && (PICK('\n'),1))) {
					new_line(file);
					ARRAY_ADD(buffer,'\n');
				} else {
					char chr = pick_esc_char(file);
					ARRAY_ADD(buffer,chr);
				}
			}
			if (!PICK('"')) {
				file_dialog(file,tk.line,"invalid string");
			}
			int lines=pick_empty_chars(file);
			if (lines)tk.eol=1;
			if (!PICK('"')) {
				break;
			}
			ARRAY_ADD(buffer,'\n');
		}

		ARRAY_ADD(buffer,0);

		tk.type = TK_STRING;
		tk.text = copy_text2(HEAP_ALLOCATOR,length,buffer);
	} break;
	case '.': { MOVE(); tk.type = TK_DOT;
		if (PICK('.')) { tk.type = TK_DOT_DOT;
				/* todo: eventually rename use TK_ELLIPSIS */
			if (PICK('.'))   tk.type = TK_DOT_DOT;
		} else if (is_digit_chr(CHARA())) {
			tk.type = TK_NUMBER;
			elf_Num n = 0;
			elf_Num p = 1;
			do {
				n = n * 10 + (MOVE() - '0');
				p *= 10;
			} while (is_digit_chr(CHARA()));
			tk.number = n / p;
		}
	} break;
	case '#': {
		MOVE();
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
		MOVE();
	} goto retry;
	case '\n': {
		MOVE();
		file->linechar = file->thischar;
		file->linenumber += 1;
	} goto retry;
	case '\r': {
		MOVE();
		PICK('\n');
		file->linechar = file->thischar;
		file->linenumber += 1;
	} goto retry;
	case ';': {
		MOVE();
		while (CHARA() != 0 && !is_eol_chr(CHARA())) {
			MOVE();
		}
		goto retry;
	} break;


	case '<': {
		MOVE();
		tk.type = TK_LT;
		if (PICK('=')) {
			tk.type = TK_LTEQ;
		} else if (PICK('<')) {
			tk.type = TK_SHL;
		}
	} break;
	case '/': {
		MOVE();
		tk.type = TK_DIV;
		if (PICK('*')) {
			for (;;) {
					/* handle lines */
				if (PICK('\n')) {
					file->linechar = file->thischar;
					file->linenumber += 1;
				} else
				if (PICK('\r')) {
					PICK('\n');
					file->linechar = file->thischar;
					file->linenumber += 1;
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
			while (CHARA() != 0 && !is_eol_chr(CHARA())) {
				MOVE();
			}
			goto retry;
		}
	} break;

	LEX3('>',TK_GT,'=',TK_GTEQ,'>',TK_SHR);
	LEX3('?',TK_QMARK,'?',TK_NIL_OR,'=',TK_NIL_ASSIGN);
	LEX3('!',TK_EXCLAMATION_MARK,'!',TK_NIL_AND,'=',TK_NEQ);
	LEX2('|',TK_BIT_OR,'|',TK_LOG_OR);
	LEX2('&',TK_BIT_AND,'&',TK_LOG_AND);
	LEX2('=',TK_ASSIGN,'=',TK_EQ);
	LEX2('*',TK_MUL,'*',TK_POW);
	LEX1('[',TK_SQUARE_LEFT);
	LEX1(']',TK_SQUARE_RIGHT);
	LEX1('(',TK_PAREN_LEFT);
	LEX1(')',TK_PAREN_RIGHT);
	LEX1('{',TK_CURLY_LEFT);
	LEX1('}',TK_CURLY_RIGHT);
	LEX1(',',TK_COMMA);
	LEX1('%',TK_MOD);
	LEX1(':',TK_COLON);
	LEX1('^',TK_BIT_XOR);
	LEX1('-',TK_SUB);
	LEX1('+',TK_ADD);
}

esc: ;

	/* passive hinting */
while ((CHARA()==' ')||(CHARA()=='\t')) {
	MOVE();
}
if ((CHARA()==';')||((CHARA()=='/')&&((CHARB()=='/')||(CHARB()=='*')))) {
	tk.eol = 1;
} else if ((CHARA()=='\n')||(CHARA()=='\r')) {
	tk.eol = 1;
}

file->last_token = file->this_token;
file->this_token = file->then_token;
file->then_token = tk;
return file->last_token;
}
