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
	elf_Arena        *scratch_arena;
	BcSourceBuffer    source;

	const char       *cursor;
	const char       *end;

	u32               line_index;
	const char       *line_start;

	Lexer_TokenFIFO   tokens;
}
Lexer;

struct Parser
{
	elf_Arena          *result_arena;
	elf_Arena           scratch_arena;
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

Parser *parser_create(Compiler *compiler);
void parser_destroy(Parser *parser);
b32 parser_set_source(Parser *parser, Com_SourceId source_id);
Ast parser_parse_file(Parser *parser);
Ast parser_parse_constant_expr(Parser *parser);
Ast parser_parse_json(Parser *parser);
Ast parser_parse_jsonl(Parser *parser);
