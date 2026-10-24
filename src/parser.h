//
// See Copyright Notice In elf.h
//

#ifndef ELF_COMPILER_PARSER_H
#define ELF_COMPILER_PARSER_H

#define NO_LINE (0)

typedef struct Parser Parser;
typedef struct Ast_Node Ast_Node;

struct Parser
{
	Arena          *result_arena;
	Arena           scratch_arena;
	Com_Source         *source;
	Token_Array         tokens;
	u32                 token_index;

	Ast                *ast_stack;
	u32                 ast_stack_size;
	u32                 ast_stack_index;
	Ast                 current_function;

	Ast_List             imports;
};

static b32 parser_init(Parser *parser);
static void parser_destroy(Parser *parser);
static Ast parser_parse_source(Parser *parser, Com_Source *source);
static Ast parser_parse_constant_expr(Parser *parser, Com_Source *source);
static Ast parser_parse_json(Parser *parser, Com_Source *source);
static Ast parser_parse_jsonl(Parser *parser, Com_Source *source);

#endif
