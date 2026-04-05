
enum
{
	MAX_ENTITIES = 1024
};


#define NO_ENTITY 0xDEADBEEF



typedef enum
{
	ENTITY_TAG_REFERENCED  = 1 << 0,
	ENTITY_TAG_CONSTANT    = 1 << 1,
	ENTITY_TAG_ASSIGNED    = 1 << 2,
	ENTITY_TAG_PARAMETER   = 1 << 3,
	ENTITY_TAG_FORLOOP     = 1 << 4,
	ENTITY_TAG_STATIC      = 1 << 5,
}
EntityTags;

typedef enum
{
	ENTITY_INVALID = 0,
	ENTITY_DIRECTORY,
	ENTITY_LOCAL_DECLARATION,
}
EntityType;

// Todo, remove name * you idiot, use atoms!
typedef u32 EntityId;
typedef struct
{
	EntityType    type;
	EntityTags    tags;
	u32          scope;
	AstRef        tree;
	char         *name;
	Source        site;
	u32           slot;
}
Entity;

typedef enum
{
	FUNCTION_VARIADIC = 1,
}
GenFunctionTags;

typedef struct
{
	// Todo
	char    *name;
	Source   site;
	TypeRule type;
	AstRef   expr;
	u32      tags;
	//
	u32      slot;
}
GenFunctionParam;

typedef struct
{
	Source              site;
	GenFunctionTags     tags;
	u32                arity;
	AstRef              body;
	GenFunctionParam *params;
	AstRef      function_ast;
}
GenFunction;

typedef struct
{
	elf_Arena *arena;
	elf_Arena *scratch_arena;

	// Todo, we need this to create Asts, because we use the same ast tree for rewritting ...
	// We can instead create a new ir graph for this ... which would be the correct long term move ...
	// But we'll stick with this for now ....
	Parser   *scratch_parser;

	// Todo, we need an off-loadable bytecode module ...
	elf_State *state;

	GenFunction *functions;
	u32           num_functions;
	u32           max_functions;

	u32 bytecode_function_offset_in_module;

	Entity     entities[MAX_ENTITIES];
	EntityId   entity_index;
	EntityId   scope_stack[32];
	EntityId   scope_index;
	EntityId   scope;

	i32       ncaptures;
	i32        memory_usage;
	i32 	     memory_state;
	i32 	     memory_state_stack[256];
	i32 	     memory_state_index;
	AstRef     memory_slots[256];

	// BytecodeBuffer *bytecode_buffer;
}
BytecodeGen;



#if 0
// Todo, remove everything from here on downwards!
Entity             entities[MAX_ENTITIES];
EntityId           entity_index;

EntityId           scope_stack[32];
EntityId           scope_index;
EntityId           scope;


// remember block hierarchy
Block              block_stack[32];
int                block_index;
Block              block;


// remember loop hierarchy
Loop             loop_stack[16];
int              loop_index;

// DURING CODE GENERATION:
// track memory usage
int 	          memory_usage;


// remember memory state
int 	          memory_state;
int 	          memory_state_stack[128];
int 	          memory_state_index;


// remember which trees have memory
AstRef          memory_slots[128];
#endif