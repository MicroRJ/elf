//
// See Copyright Notice In elf.h
//

#define NO_LINE (0)

typedef struct Parser Parser;

typedef struct
{
	elf_State        *state;
	elf_String        *name;
	elf_StrSlice     source;
	char            *cursor;
	u32          line_index;
	char        *line_start;

	char    *in_expr_start;
	char    *in_string_start;
	b32      in_string_block;
	b32      in_string_expr;
	b32      in_string;
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
