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
	Atom_Table       *atoms;
	SourceBuffer      source;

	const char       *cursor;
	const char       *end;

	u32               line_index;
	const char       *line_start;
	b32               failed;

	Lexer_TokenFIFO   tokens;
}
Lexer;

struct Parser
{
	elf_Arena          *arena;
	Compiler           *compiler;
	Atom_Table          atoms;
	Lexer               lexer;
	// TODO(RJ): probably redundant now!
	Token               tok,tok_prev,tok_prox;
	AstContext          ast;
	// TODO(RJ): why are we storing this here!?
	elf_DiagnosticPhase phase;
	b32                 failed;
};

static inline b32 parser_has_failed(Parser *parser)
{
	return parser->failed || parser->lexer.failed;
}
