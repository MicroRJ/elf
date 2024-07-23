/*
** See Copyright Notice In elf.h
** elf-tkn.h
** tokens...
*/

// todo: remove over and in and using

typedef struct elToken {
	unsigned char type;
	elFileLine line;
	unsigned int eol: 1;
	union {
		char *s;
		elInteger i;
		elNumber n;
	};
} elToken;

#define FIRST_KEYWORD 1
#define LAST_KEYWORD  28
#define FIRST_MACRO   29
#define LAST_MACRO    34


#define KWLIST(_) \
_(ENUM,"enum") \
_(ELF,"elf") \
_(LOAD,"load") \
_(TRY,"try") _(CATCH,"catch") _(FINALLY,"finally") \
_(NEW,"new") _(THIS,"this") \
_(FUN,"fun") \
_(LET,"let") _(FOR,"for") \
_(DO,"do") _(WHILE,"while") \
_(BREAK,"break") _(CONTINUE,"continue") \
_(LASTLY,"lastly") \
_(LEAVE,"leave") \
_(IF,"if") _(IFF,"iff") _(ELSE,"else") _(ELIF,"elif") \
_(THEN,"then") \
_(NIL,"nil") _(TRUE,"true") _(FALSE,"false")

#define MCLIST(_) \
_(LINE_NUMBER,"line_number") _(LINE_CHAR,"line_char") _(FILE_NAME,"file_name") \
_(LEVEL,"level") _(REGISTER,"register") \
_(INDEX,"index") _(VALUE,"value") _(ARRAY,"array") _(FIELD,"field")

#define OPLIST(_) \
_(MUL,"*",11) _(DIV,"/",11) _(MODULUS,"%",11) _(ADD,"+",10) _(SUB,"-",10) \
_(RIGHT_SHIFT,">>",9) _(LEFT_SHIFT,"<<",9) \
_(LESS_THAN,"<",8) _(LESS_THAN_EQUAL,"<=",8) _(GREATER_THAN,">",8) _(GREATER_THAN_EQUAL,">=",8) \
_(EQUALS,"==",7) _(NOT_EQUALS,"!=",7) \
_(BIT_AND,"&",6) _(BIT_OR,"|",5) _(BIT_XOR,"^",4) \
_(LOG_AND,"&&",3) _(LOG_OR,"||",2) _(NIL_OR,"??",2) \
_(DOT_DOT,"..",1)

#define TKLIST(_) \
_(NEGATE,"!") \
_(INTEGER,"integer") _(NUMBER,"number") _(STRING,"string") _(LETTER,"letter") _(WORD,"word") \
_(QUESTION_MARK,"?") _(EXCLAMATION_MARK,"!") \
_(ASSIGN,"=") _(NIL_ASSIGN,"?=") \
_(COLON,":") _(SEMI_COLON,";") \
_(COMMA,",") _(DOT,".") \
_(SQUARE_LEFT,"[") _(SQUARE_RIGHT,"]") _(CURLY_LEFT,"{") _(CURLY_RIGHT,"}") \
_(PAREN_LEFT,"(") _(PAREN_RIGHT,")") \

typedef enum elTokenType {
	TK_NONE = 0,
#define TKITEM(NAME,_) XFUSE(TK_,NAME),
#define OPITEM(NAME,_,__) XFUSE(TK_,NAME),
#define MCITEM(NAME,_) XFUSE(TK_M_,NAME),
	KWLIST(TKITEM)
	MCLIST(MCITEM)
	TKLIST(TKITEM)
	OPLIST(OPITEM)
#undef TKITEM
#undef MCITEM
#undef OPITEM
} elTokenType;


typedef struct ltokenintel {
	char *name;
	char prec;
	elf_hashint hash;
} ltokenintel;

elf_globaldecl ltokenintel elf_tkintel[] = {
	{"none",-2,0},
#define TKITEM(_,SYM) {SYM,-2,0},
#define OPITEM(_,SYM,PRC) {SYM,PRC,0},
	KWLIST(TKITEM)
	MCLIST(TKITEM)
	TKLIST(TKITEM)
	OPLIST(OPITEM)
};


