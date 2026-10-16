//
// See Copyright Notice In elf.h
//

#define NO_LINE (0)

typedef struct Parser Parser;

typedef struct
{
	Token *items;
	u32    index;
	u32    count;
	u32    capacity;
}
Lexer_TokenFIFO;

typedef struct
{
	Compiler         *compiler;
	Com_SourceId      source_id;
	Atom_Table       *atoms;
	SourceBuffer      source;

	const char       *cursor;
	const char       *end;

	u32               line_index;
	const char       *line_start;

	Lexer_TokenFIFO   tokens;
}
Lexer;

struct Parser
{
	elf_Arena          *arena;
	Compiler           *compiler;
	Com_SourceId        source_id;
	Atom_Table         *atoms;
	Lexer               lexer;
	// TODO(RJ): probably redundant now!
	Token               tok,tok_prev,tok_prox;
	Ast                *ast_stack;
	u32                 ast_stack_size;
	u32                 ast_stack_index;
};
