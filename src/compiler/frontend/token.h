//
// See Copyright Notice In elf.h
//

#define FORMAT_CHAR '$'

#define KEYWORDDEF(_)       \
_(JSON     ,"json"        ) \
_(DEFAULT  ,"default"     ) \
_(LOAD     ,"load"        ) \
_(FUN      ,"fun"         ) \
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
_(RECURSE  ,"recurse"     ) \
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
_(M_GET_MEM       , "get_mem"     ) \
_(M_GETEXPR       , "getexpr"     ) \
_(M_INDEX         , "index"       ) \
_(M_VALUE         , "elf_Value"       ) \
_(M_ARRAY         , "elf_Value *"       ) \
_(M_FIELD         , "field"       ) \
_(M_ENDOFFILE     , "eof"         ) \
_(M_THIS          , "this"        ) \
/* end */


#define TOKEN_XDEF(_)                  \
_(INTEGER             , "int")         \
_(NUMBER              , "num")         \
_(STRING              , "str")         \
_(FORMAT_STRING       , "fmt")         \
_(STRING_START        , "str_start")   \
_(STRING_PART         , "str_part")    \
_(STRING_END          , "str_end")     \
_(LETTER              , "chr")         \
_(IDENTIFIER          , "idn")         \
_(TILDE               , "~"  )         \
_(MINUS_MINUS         , "--" )         \
_(PLUS_PLUS           , "++" )         \
_(QMARK               , "?"  )         \
_(EXCLAMATION_MARK    , "!"  )         \
_(ARROW               , "->" )         \
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
_(MUL_ASSIGN          , "*=" )         \
_(DIV                 , "/"  )         \
_(DIV_ASSIGN          , "/=" )         \
_(MOD                 , "%"  )         \
_(MOD_ASSIGN          , "%=" )         \
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
_(XOR_ASSIGN          , "^=" )         \
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
	TOK_COUNT_,
}
TokenType;

static const char *token_type_name(TokenType type);

typedef struct
{
	TokenType    type;
	SourceSite  site;
	unsigned int eol: 1;
	union {
		u64 integer_magnitude;
		f64 number;
		elf_String *atom;
	};
}
Token;
