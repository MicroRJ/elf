// /*
// ** See Copyright Notice In elf.h
// ** elf-tkn.h
// ** tokens...
// */

// typedef struct elToken {
// 	unsigned char type;
// 	elFileline line;
// 	unsigned int eol: 1;
// 	union {
// 		elInteger integer, /* @DEPRECATED */  i;
// 		elNumber   number, /* @DEPRECATED */  n;
// 		char 			*text, /* @DEPRECATED */ *s;
// 	};
// } elToken;

// #define KWLIST(_) \
// _(ELF,"elf") /* <- most likely to be removed */ \
// _(ENUM,"enum") \
// _(LOAD,"load") \
// _(TRY,"try") _(CATCH,"catch") _(FINALLY,"finally") \
// _(NEW,"new") _(THIS,"this") \
// _(FUN,"fun") \
// _(LET,"let") _(FOR,"for") \
// _(DO,"do") _(WHILE,"while") \
// _(BREAK,"break") _(CONTINUE,"continue") \
// _(LASTLY,"lastly") \
// _(LEAVE,"leave") \
// _(IF,"if") _(IFF,"iff") _(ELSE,"else") _(ELIF,"elif") \
// _(THEN,"then") \
// _(NIL,"nil") _(TRUE,"true") _(FALSE,"false")

// #define MCLIST(_) \
// _(LINE_NUMBER,"line_number") _(LINE_CHAR,"line_char") _(FILE_NAME,"file_name") \
// _(LEVEL,"level") _(REGISTER,"register") \
// _(INDEX,"index") _(VALUE,"value") _(ARRAY,"array") _(FIELD,"field") _(ENDOFFILE,"eof")


// //
// // --------------------------------------
// // GRID[ny] !! GRID[ny,nx]
// // --------------------------------------
// // if GRID[ny] is nil ? leave nil
// // leave GRID[ny,nx]
// //
// #define OPLIST(_) \
// _(POW,"**",12) _(MUL,"*",11) _(DIV,"/",11) _(MOD,"%",11) \
// _(ADD,"+",10) _(SUB,"-",10) \
// _(SHR,">>",9) _(SHL,"<<",9) \
// _(LT,"<",8) _(LTEQ,"<=",8) _(GT,">",8) _(GTEQ,">=",8) \
// _(EQ,"==",7) _(NEQ,"!=",7) \
// _(BIT_AND,"&",6) _(BIT_OR,"|",5) _(BIT_XOR,"^",4) \
// _(LOG_AND,"&&",3) _(LOG_OR,"||",2) \
// _(NIL_AND,"!!",3) _(NIL_OR,"??",2) \
// _(ELLIPSIS,"...",1) _(DOT_DOT,"..",1)

// #define TKLIST(_) \
// _(INTEGER,"integer") _(NUMBER,"number") _(STRING,"string") _(LETTER,"letter") _(WORD,"word") \
// _(QMARK,"?") _(EXCLAMATION_MARK,"!") \
// _(ASSIGN,"=") _(NIL_ASSIGN,"?=") \
// _(COLON,":") _(SEMI_COLON,";") \
// _(COMMA,",") _(DOT,".") \
// _(SQUARE_LEFT,"[") _(SQUARE_RIGHT,"]") _(CURLY_LEFT,"{") _(CURLY_RIGHT,"}") \
// _(PAREN_LEFT,"(") _(PAREN_RIGHT,")") \

// typedef enum elTokenType {
// 	TK_NONE = 0,
// #define TKITEM(NAME,_) elFUSE(TK_,NAME),
// #define OPITEM(NAME,_,__) elFUSE(TK_,NAME),
// #define MCITEM(NAME,_) elFUSE(TK_M_,NAME),
// 	KWLIST(TKITEM)
// 	MCLIST(MCITEM)
// 	TKLIST(TKITEM)
// 	OPLIST(OPITEM)
// #undef TKITEM
// #undef MCITEM
// #undef OPITEM
// } elTokenType;


// elTokenType elf_textiswordormacro(char *name) {
// 	#define MCITEM(NAME,SYM) if (elf_texteq(SYM,name)) return elFUSE(TK_M_,NAME);
// 		MCLIST(MCITEM)
// 	#undef MCITEM
// 	return TK_WORD;
// }


// elTokenType elf_is_word_or_keyword(char *name) {
// 	#define KWITEM(NAME,SYM) if (elf_texteq(SYM,name)) return elFUSE(TK_,NAME);
// 		KWLIST(KWITEM)
// 	#undef KWITEM
// 	return TK_WORD;
// }


// typedef struct ltokenintel {
// 	char *name;
// 	char prec;
// } ltokenintel;

// elGLOBAL ltokenintel elf_tkintel[] = {
// 	{"none",-2},
// #define TKITEM(_,SYM) {SYM,-2},
// #define OPITEM(_,SYM,PRC) {SYM,PRC},
// 	KWLIST(TKITEM)
// 	MCLIST(TKITEM)
// 	TKLIST(TKITEM)
// 	OPLIST(OPITEM)
// };


