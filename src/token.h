//
// See Copyright Notice In elf.h
//

#ifndef ELF_COMPILER_TOKEN_H
#define ELF_COMPILER_TOKEN_H

#define FORMAT_CHAR '$'

#define KEYWORD_DEFINITIONS(X) \
	X(JSON     ,"json"        ) \
	X(DEFAULT  ,"default"     ) \
	X(FUN      ,"fun"         ) \
	X(EXPORT   ,"export"      ) \
	X(IMPORT   ,"import"      ) \
	X(NIL      ,"nil"         ) \
	X(TRUE     ,"true"        ) \
	X(FALSE    ,"false"       ) \
	X(ENUM     ,"enum"        ) \
	X(TRY      ,"try"         ) \
	X(CATCH    ,"catch"       ) \
	X(FINALLY  ,"finally"     ) \
	X(DO       ,"do"          ) \
	X(WHILE    ,"while"       ) \
	X(BREAK    ,"break"       ) \
	X(CONTINUE ,"continue"    ) \
	X(FOR      ,"for"         ) \
	X(DEFER    ,"defer"       ) \
	X(RET      ,"ret"         ) \
	X(RECURSE  ,"recurse"     ) \
	X(IF       ,"if"          ) \
	X(ELSE     ,"else"        ) \
	X(ELIF     ,"elif"        ) \
	X(THEN     ,"then"        ) \
	X(GLOBAL   ,"global"      ) \
// END

#define MACRO_DEFINITIONS(X)           \
	X(M_LINE_NUMBER   , "line_number" ) \
	X(M_LINE_CHAR     , "line_char"   ) \
	X(M_FILE_NAME     , "file_name"   ) \
	X(M_INT           , "int"         ) \
	X(M_GET_MEM       , "get_mem"     ) \
	X(M_INDEX         , "index"       ) \
	X(M_VALUE         , "value"       ) \
	X(M_ARRAY         , "array"       ) \
	X(M_ENDOFFILE     , "eof"         ) \
// END

#define TOKEN_DEFINITIONS(X)              \
	X(ERROR               , "error")       \
	X(INTEGER             , "int")         \
	X(NUMBER              , "num")         \
	X(STRING              , "str")         \
	X(INTERPOLATION_START , "fmt_start")   \
	X(INTERPOLATION_END   , "fmt_end")     \
	X(LETTER              , "chr")         \
	X(IDENTIFIER          , "idn")         \
	X(TILDE               , "~"  )         \
	X(MINUS_MINUS         , "--" )         \
	X(PLUS_PLUS           , "++" )         \
	X(QMARK               , "?"  )         \
	X(EXCLAMATION_MARK    , "!"  )         \
	X(BIND                , ":=" )         \
	X(HARD_BIND           , "::=")         \
	X(STATIC_BIND         , "::" )         \
	X(ASSIGN              , "="  )         \
	X(NIL_ASSIGN          , "?=" )         \
	X(COLON               , ":"  )         \
	X(SEMICOLON           , ";"  )         \
	X(COMMA               , ","  )         \
	X(DOT                 , "."  )         \
	X(SQUARE_LEFT         , "["  )         \
	X(SQUARE_RIGHT        , "]"  )         \
	X(LEFT_BRACE          , "{"  )         \
	X(RIGHT_BRACE         , "}"  )         \
	X(LEFT_PAREN          , "("  )         \
	X(PAREN_RIGHT         , ")"  )         \
	X(POW                 , "**" )         \
	X(MUL                 , "*"  )         \
	X(MUL_ASSIGN          , "*=" )         \
	X(DIV                 , "/"  )         \
	X(DIV_ASSIGN          , "/=" )         \
	X(MOD                 , "%"  )         \
	X(MOD_ASSIGN          , "%=" )         \
	X(ADD                 , "+"  )         \
	X(ADD_ASSIGN          , "+=" )         \
	X(SUB                 , "-"  )         \
	X(SUB_ASSIGN          , "-=" )         \
	X(SHR                 , ">>" )         \
	X(SHR_ASSIGN          , ">>=" )        \
	X(SHL                 , "<<" )         \
	X(SHL_ASSIGN          , "<<=" )        \
	X(LT                  , "<"  )         \
	X(LTEQ                , "<=" )         \
	X(GT                  , ">"  )         \
	X(GTEQ                , ">=" )         \
	X(EQ                  , "==" )         \
	X(NEQ                 , "!=" )         \
	X(BIT_AND             , "&"  )         \
	X(BIT_OR              , "|"  )         \
	X(BIT_XOR             , "^"  )         \
	X(XOR_ASSIGN          , "^=" )         \
	X(LOG_AND             , "&&" )         \
	X(LOG_OR              , "||" )         \
	X(NIL_AND             , "!!" )         \
	X(NIL_OR              , "??" )         \
	X(ELLIPSIS            ,"..." )         \
	KEYWORD_DEFINITIONS(X)                 \
	MACRO_DEFINITIONS(X)                   \
// END

typedef enum
{
	TOK_NONE = 0,
#define XPAND(ENUM, NAME) TOK_##ENUM,
	TOKEN_DEFINITIONS(XPAND)
#undef XPAND
	TOK_COUNT_,
}
Token_Type;

static const char *token_type_name(Token_Type type);

typedef struct
{
	Token_Type  type;
	SourceSite  site;
	b32         line_break_before;
	union
	{
		u64   integer_magnitude;
		f64   number;
		struct Atom *atom;
		Com_Status status;
	};
}
Token;

typedef struct
{
	Token *items;
	u32    count;
	u32    capacity;
}
Token_Array;

static inline b32 token_is_error(Token token)
{
	return token.type == TOK_ERROR;
}

static Token token_from_status(Com_Status status, SourceSite site)
{
	ASSERT(status != COM_STATUS_OK);
	Token token = {.type = TOK_ERROR, .site = site, .status = status};
	if (token.site.size == 0) {
		token.site.size = 1;
	}
	return token;
}

#endif
