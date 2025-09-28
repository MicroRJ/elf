//
// See Copyright Notice In elf.h
//

// todo: could we meta-generate this?


#define FORMAT_CHAR '%'

#define KEYWORDDEF(_) \
_(ELF      ,"elf"         ) \
_(JSON     ,"json"        ) \
_(DEFAULT  ,"default"     ) \
_(LOAD     ,"load"        ) \
_(NEW      ,"new"         ) \
_(FUN      ,"fun"         ) \
_(FUNCTION ,"function"    ) \
_(NIL      ,"nil"         ) \
_(TRUE     ,"true"        ) \
_(FALSE    ,"false"       ) \
_(ENUM     ,"enum"        ) \
_(TRY      ,"try"         ) \
_(CATCH    ,"catch"       ) \
_(FINALLY  ,"finally"     ) \
_(DO       ,"do"          ) \
_(WHILE    ,"while"       ) \
_(BREAK    ,"break"       ) \
_(CONTINUE ,"continue"    ) \
_(FOR      ,"for"         ) \
_(DEFER    ,"defer"       ) \
_(RET      ,"ret"         ) \
_(IF       ,"if"          ) \
_(ELSE     ,"else"        ) \
_(ELIF     ,"elif"        ) \
_(THEN     ,"then"        ) \
_(GLOBAL   ,"global"      ) \
/* end */

#define MACRODEF(_) \
_(LINE_NUMBER   , "line_number" ) \
_(LINE_CHAR     , "line_char"   ) \
_(FILE_NAME     , "file_name"   ) \
_(ASSERT        , "assert"      ) \
_(INT           , "int"         ) \
_(NUM           , "num"         ) \
_(LEVEL         , "level"       ) \
_(GETMEM        , "getmem"      ) \
_(GETEXPR       , "getexpr"     ) \
_(INDEX         , "index"       ) \
_(VALUE         , "value"       ) \
_(ARRAY         , "array"       ) \
_(FIELD         , "field"       ) \
_(ENDOFFILE     , "eof"         ) \
_(THIS          , "this"        ) \
/* end */

/* === Binary Operators ======================
	Only binary operators are defined here
	for the parser to generate code for binary
	expression parsing.
	===========================================
*/
#define BOPDEF(_) \
_(     POW, "**", 12) \
_(     MUL,  "*", 11) _( DIV,  "/",  11) _(MOD, "%", 11) \
_(     ADD,  "+", 10) _( SUB,  "-",  10) \
_(     SHR, ">>",  9) _( SHL, "<<",   9) \
_(      LT,  "<",  8) _(LTEQ, "<=",   8) \
_(      GT,  ">",  8) _(GTEQ, ">=",   8) \
_(      EQ, "==",  7) _( NEQ, "!=",   7) \
_( BIT_AND,  "&",  6) \
_(  BIT_OR,  "|",  5) \
_( BIT_XOR,  "^",  4) \
_( LOG_AND, "&&",  3) _( LOG_OR, "||",  2) \
_( NIL_AND, "!!",  3) _( NIL_OR, "??",  2) \
_(ELLIPSIS,"...",  1) _(DOT_DOT, "..",  1) \
/* end */


#define TOKENDEF(_)                 \
_(INTEGER            ,"int")        \
_(NUMBER             ,"num")        \
_(STRING             ,"str")        \
_(FORMAT_STRING      ,"fmt")        \
_(LETTER             ,"chr")        \
_(WORD               ,"word")       \
_(TILDE              ,"~")          \
_(MINUS_MINUS        ,"--")         \
_(PLUS_PLUS          ,"++")         \
_(QMARK              ,"?")          \
_(EXCLAMATION_MARK   ,"!")          \
_(ARROW              ,"->")         \
_(HARD_ARROW         ,"-->")        \
_(BIND               ,":=")         \
_(HARD_BIND           ,"::=")        \
_(STATIC_BIND         ,"::")         \
_(ASSIGN              ,"=")          \
_(NIL_ASSIGN          ,"?=")         \
_(COLON               ,":")          \
_(COLON_COLON         ,"::")         \
_(SEMI_COLON          ,";")          \
_(COMMA               ,",")          \
_(DOT                 ,".")          \
_(SQUARE_LEFT         ,"[")          \
_(SQUARE_RIGHT        ,"]")          \
_(SQUARE_SQUARE_LEFT  ,"[[")         \
_(SQUARE_SQUARE_RIGHT ,"]]")         \
_(CURLY_LEFT          ,"{")          \
_(CURLY_RIGHT         ,"}")          \
_(PAREN_LEFT          ,"(")          \
_(PAREN_RIGHT         ,")")          \
/* end */


typedef enum tokenTy {
	TK_NONE = 0,
#define TKITEM(NAME,_) XFUSE(TK_,NAME),
#define OPITEM(NAME,_,__) XFUSE(TK_,NAME),
#define MCITEM(NAME,_) XFUSE(TK_M_,NAME),
	KEYWORDDEF(TKITEM)
	MACRODEF(MCITEM)
	TOKENDEF(TKITEM)
	BOPDEF(OPITEM)
#undef TKITEM
#undef MCITEM
#undef OPITEM
} tokenTy;


typedef struct token_metadata_t {
	char name[16];
	char rank;
} token_metadata_t;

global token_metadata_t g_token_metadata_table[] = {
	{"none",-2},
#define TKITEM(_,SYM) {SYM,-2},
#define OPITEM(_,SYM,PRC) {SYM,PRC},
	KEYWORDDEF(TKITEM)
	MACRODEF(TKITEM)
	TOKENDEF(TKITEM)
	BOPDEF(OPITEM)
};

#undef TKITEM
#undef MCITEM
#undef OPITEM


typedef struct {
	tokenTy        type;
	Source         line;
	unsigned int eol: 1;
	union {
		elf_i64 integer;
		elf_f64  number;
		char 	    *text;
	};
} Token;
