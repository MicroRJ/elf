//
// See Copyright Notice In elf.h
//

#define FORMAT_CHAR '%'

#define KEYWORDDEF(_)       \
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

#define MACRODEF(_)                 \
_(M_LINE_NUMBER   , "line_number" ) \
_(M_LINE_CHAR     , "line_char"   ) \
_(M_FILE_NAME     , "file_name"   ) \
_(M_ASSERT        , "assert"      ) \
_(M_INT           , "int"         ) \
_(M_NUM           , "num"         ) \
_(M_LEVEL         , "level"       ) \
_(M_GETMEM        , "getmem"      ) \
_(M_GETEXPR       , "getexpr"     ) \
_(M_INDEX         , "index"       ) \
_(M_VALUE         , "value"       ) \
_(M_ARRAY         , "array"       ) \
_(M_FIELD         , "field"       ) \
_(M_ENDOFFILE     , "eof"         ) \
_(M_THIS          , "this"        ) \
/* end */


#define TOKEN_XDEF(_)                  \
_(INTEGER             , "int")         \
_(NUMBER              , "num")         \
_(STRING              , "str")         \
_(FORMAT_STRING       , "fmt")         \
_(LETTER              , "chr")         \
_(IDENTIFIER          , "idn")         \
_(TILDE               , "~"  )         \
_(MINUS_MINUS         , "--" )         \
_(PLUS_PLUS           , "++" )         \
_(QMARK               , "?"  )         \
_(EXCLAMATION_MARK    , "!"  )         \
_(ARROW               , "->" )         \
_(LONG_ARROW          , "-->")         \
_(BIND                , ":=" )         \
_(HARD_BIND           , "::=")         \
_(STATIC_BIND         , "::" )         \
_(ASSIGN              , "="  )         \
_(NIL_ASSIGN          , "?=" )         \
_(COLON               , ":"  )         \
_(COLON2              , "::" )         \
_(SEMICOLON           , ";"  )         \
_(COMMA               , ","  )         \
_(DOT                 , "."  )         \
_(SQUARE_LEFT         , "["  )         \
_(SQUARE_RIGHT        , "]"  )         \
_(LEFT_BRACE          , "{"  )         \
_(RIGHT_BRACE         , "}"  )         \
_(LEFT_PAREN          , "("  )         \
_(PAREN_RIGHT         , ")"  )         \
_(POW                 , "**" )         \
_(MUL                 , "*"  )         \
_(DIV                 , "/"  )         \
_(MOD                 , "%"  )         \
_(ADD                 , "+"  )         \
_(ADD_ASSIGN          , "+=" )         \
_(SUB                 , "-"  )         \
_(SUB_ASSIGN          , "-=" )         \
_(SHR                 , ">>" )         \
_(SHR_ASSIGN          , ">>=" )        \
_(SHL                 , "<<" )         \
_(SHL_ASSIGN          , "<<=" )        \
_(LT                  , "<"  )         \
_(LTEQ                , "<=" )         \
_(GT                  , ">"  )         \
_(GTEQ                , ">=" )         \
_(EQ                  , "==" )         \
_(NEQ                 , "!=" )         \
_(BIT_AND             , "&"  )         \
_(BIT_OR              , "|"  )         \
_(BIT_XOR             , "^"  )         \
_(LOG_AND             , "&&" )         \
_(LOG_OR              , "||" )         \
_(NIL_AND             , "!!" )         \
_(NIL_OR              , "??" )         \
_(ELLIPSIS            ,"..." )         \
KEYWORDDEF(_)                          \
MACRODEF(_)                            \
// END /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


typedef enum
{
	TOK_NONE = 0,
#define XPAND(ENUM, NAME) TOK_##ENUM,
	TOKEN_XDEF(XPAND)
#undef XPAND
}
TokenType;


// Todo, put this somewhere proper!
Static_Data char *static__str_from_token_type[] =
{
	"none",
#define XPAND(ENUM, NAME) NAME,
	TOKEN_XDEF(XPAND)
#undef XPAND
};

typedef struct
{
	TokenType    type;
	union
	{
		Source    line;
		Source    site;
	};
	unsigned int eol: 1;
	union {
		elf_i64 integer;
		elf_f64  number;
		char 	    *text;
	};
}
Token;
