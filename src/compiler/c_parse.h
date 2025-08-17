//
// See Copyright Notice In elf.h
//


#define NO_LINE ( 0)


enum {
	ENTITY_REFERENCED = (1 << 0),
	ENT_FLAG_CONSTANT = (1 << 1),
	ENTITY_ASSIGNED   = (1 << 2),
	ENTITY_PARAMETER  = (1 << 3),
	ENTITY_FORLOOP    = (1 << 4),
	ENT_FLAG_STATIC   = (1 << 5),
};


typedef enum {
	ENTITY_INVALID = 0,
	ENTITY_DIRECTORY,
	ENTITY_LOCAL,
} entityKi;


typedef int entID;
#define NO_ENTITY -1


typedef struct {
	entityKi kind;
	int    status;
	int     scope;
	treeID   tree;
	char    *name;
	Source   line;
} Entity;

//
// todo: this uses dynamic arrays, which isn't
// needed!
//
typedef struct {
	int *t,*f;
} jumpS;


typedef struct JBuf {
	BCPos *jz;
	/* list of exit jump instructions from
	each consecutive block to be patched */
	BCPos *j;
} JBuf;

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
	treeID  index, value;
	// so this is intersting, because for us to find a loop
	// by name in the first place - for instance, #index(i),
	// the parser has to resolve i by looking up the entity,
	// but then what we get is a tree, so we could look up
	// the entity by the tree, or we could just lookup the
	// loop by the tree directly (which ends up being faster).
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
// don't allocate on the stack, this thing might is big!
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
	// source is kept alive by the user, typically a Proto_File, but we
	// don't include a pointer to it because there's no need for that
	// to be here... not everyone is as privileged... no
	char                     *source;
	char                     *cursor;
	Token      tok,tok_prev,tok_prox;

	// todo: turn this into a proper compressor
	// active source location to prevent the annoyance
	// of having to pass to all functions that need it.
	char                  *sourceloc;

	// todo: proper arena
	Tree                  *tree_memory;
	int                    tree_index;

	// the current function
	treeID                       enc;
	// Record all the functions we've come across, to help
	// during code generation.
	// Because we write to the same contiguous buffer, we
	// need to queue functions so that they generate one
	// after the other.
	// Note that the generator will eventually see all these
	// functions regardless.
	// By tracking the functions as we parse them, we avoid
	// having to re-discover them during code gen.
	//
	treeID          *functions;
	// entities are named objects, variables,
	// symbols, special names, and such...
	// todo: allocate within struct
	Entity           entities[MAX_ENTITIES];
	entID            entity_index;
	// remember scope hierarchy, 'scope'
	// is the first visible entity
	entID            scope_stack[32];
	entID            scope_index;
	entID            scope;
	// remember block hierarchy
	Block            block_stack[32];
	int              block_index;
	Block            block;
	// remember loop hierarchy
	Loop             loop_stack[32];
	int              loop_index;
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




