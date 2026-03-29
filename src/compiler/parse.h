//
// See Copyright Notice In elf.h
//


#define NO_LINE (0)


// Todo, remove dynamic arrays!
typedef struct
{
	int *t,*f;
}
jumpS;


// Todo, remove dynamic arrays!
typedef struct JBuf
{
	BCPos *jz;
	/* list of exit jump instructions from each consecutive block to be patched */
	BCPos *j;
}
JBuf;


typedef enum
{
	BLOCK_ENDED  = 1,
	BLOCK_HASRET = 2,
}
BlockBits;

typedef struct Block Block;
struct Block
{
	BlockBits  status;
	AstRef     *stats;
	u32        nstats;
};

typedef struct Loop Loop;
struct Loop {
	AstRef *breaks;
	AstRef *continues;
	AstRef  index, *values;
	AstRef  name;
};




// todo:
// we use the parser for everything, but we may want to split
// it because there are some functions that all they want to
// do is parse a constant expression, in which case the only
// thing they want is a lexer
// don't allocate on the stack, this thing might is big!
typedef struct Parser Parser;
struct Parser
{
	elf_Arena                 *arena;
	union
	{
		elf_State              *R, *S;
		elf_State              *inter;
		elf_State              *state;
	};

	char                      name[256];


	// source is kept alive by the user, typically a BytecodeFile, but we
	// don't include a pointer to it because there's no need for that
	// to be here... not everyone is as privileged... no
	char                     *source;
	char                     *cursor;
	Token      tok,tok_prev,tok_prox;

	u32                tree_stack_size;
	u32                tree_stack_index;
	AstRef            *tree_stack;

	AstRef            *functions;
	u32               nfunctions;

	// immediate buffer?
	char            tempbuf[1024];
};


