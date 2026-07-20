//
// See Copyright Notice In elf.h
//

#define NO_LINE (0)

typedef struct Parser Parser;

typedef enum
{
	LEXER_MODE_NORMAL = 0,
	LEXER_MODE_INTERPOLATION,
}
LexerModeType;

typedef struct
{
	LexerModeType type;
	i32           depth;
	SourceSite    string_site;
	SourceSite    interpolation_site;
	b32           is_block_string;
}
LexerMode;

typedef struct
{
	elf_State        *state;
	elf_String        *name;
	elf_StrSlice     source;
	char            *cursor;
	u32          line_index;
	char        *line_start;

	LexerMode mode_stack[16];
	u32       mode_index;
	LexerMode mode;
}
Lexer;

struct Parser
{
	elf_Arena     *arena;
	elf_State     *state;
	elf_String     *name;
	Lexer          lexer;
	Token          tok,tok_prev,tok_prox;
	AstContext       ast;
};
