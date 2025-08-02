//
// See Copyright Notice In elf.h
//

#define NO_SLOT (-1)
#define NO_JUMP (-0)
#define NO_LINE ( 0)

enum {
	ENTITY_REFERENCED = (1 << 0),
	ENTITY_CONSTANT   = (1 << 1),
	ENTITY_ASSIGNED   = (1 << 2),
	ENTITY_PARAMETER  = (1 << 3),
	ENTITY_FORLOOP    = (1 << 4),
};

typedef enum {
	ENTITY_INVALID = 0, ENTITY_DIRECTORY, ENTITY_LOCAL, ENTITY_GLOBAL,
} entityKi;

typedef int entityID;
#define NO_ENTITY -1

typedef struct {
	entityKi kind;
	int    status;
	int     scope;
	treeID   tree;
	char    *name;
	Source   line;
} entityT;

//
// todo: this uses dynamic arrays, which isn't
// needed!
//
typedef struct {
	int *t,*f;
} jumpS;


typedef struct BranchJumps {
	/* Conditional false jump instructions
	to be patched so that they jump to
	the next block or instruction */
	int *jz;
	/* list of exit jump instructions from
	each consecutive block to be patched */
	int *j;
} BranchJumps;

typedef struct Block Block;
struct Block {
	treeID *body;
	treeID *defers;
	int     ended;
	int     has_ret;
};

typedef struct Loop Loop;
struct Loop {
	treeID *breaks;
	treeID *continues;
	treeID  name;
};

enum {
	MAX_ENTITIES = 1024
};
// todo:
// we use the parser for everything, but we may want to split
// it because there are some functions that all they want to
// do is parse a constant expression, in which case the only
// thing they want is a lexer
// NOTE: maybe don't allocate on the stack, this thing might be big!
typedef struct elf_Parser elf_Parser;
struct elf_Parser {
	union {
		elf_State                  *R;
		elf_State              *inter;
	};

	// name is only for error reporting, allocate here to remove
	// ambiguity of who owns what..
	// also the name may not fit so make sure to handle that properly
	char                      name[256];
	// source must be kept alive by user
	char                     *source;

	char                     *cursor;
	Token      tok,tok_prev,tok_prox;

	// active source location to prevent the annoyance
	// of having to pass to all functions that need it.
	char                  *sourceloc;
	treeID                       enc;
	treeID                *functions;
	// entities are named objects, variables,
	// symbols, special names, and such...
	// todo: allocate within struct
	entityT          entities[MAX_ENTITIES];
	entityID         entity_index;
	// remember scope hierarchy, 'scope'
	// is the first visible entity
	entityID         scope_stack[32];
	entityID         scope_index;
	entityID         scope;
	// remember block hierarchy
	Block            block_stack[32];
	int              block_index;
	Block            block;
	// remember loop hierarchy
	Loop             loop_stack[32];
	int              loop_index;
	Loop             loop;
	// DURING CODE GENERATION:
	// track memory usage
	int 	          memory_usage;
	// remember memory state
	int 	          memory_state;
	int 	          memory_state_stack[128];
	int 	          memory_state_index;
	// remember which trees have memory
	treeID          memory_slots[1024];

	// immediate temporary buffer, some temporary results are placed here
	char            tempbuf[1024];
};




