//
// See Copyright Notice In elf.h
//

#define NO_LINE (0)

typedef struct Parser Parser;

typedef struct
{
	elf_State    *state;
	elf_String     *name;
	elf_StrSlice source;
	char        *cursor;
	u32          line_index;
	char        *line_start;
}
Lexer;

struct Parser
{
	Arena         *arena;
	elf_State     *state;
	elf_String      *name;
	Lexer          lexer;
	Token          tok,tok_prev,tok_prox;
	AstContext     ast;
};
