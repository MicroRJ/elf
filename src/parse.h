//
// See Copyright Notice In elf.h
//

#define NO_LINE (0)

// todo:
// we use the parser for everything, but we may want to split
// it because there are some functions that all they want to
// do is parse a constant expression, in which case the only
// thing they want is a lexer
// don't allocate on the stack, this thing might is big!
typedef struct Parser Parser;
struct Parser
{
	elf_Arena         *arena;
	elf_State         *state;
	char              *name;
	u32                line_index;
	char              *line_start;
	char              *source;
	char              *cursor;
	Token              tok,tok_prev,tok_prox;
	u32                ast_stack_size;
	u32                ast_stack_index;
	AstRef            *ast_stack;
};


