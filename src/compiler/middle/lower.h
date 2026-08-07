//
// See Copyright Notice In elf.h
//

#define IMPLICIT_PARAM_INDEX 0
#define IMPLICIT_PARAM_COUNT 1

typedef struct Entity Entity;

typedef struct
{
	SourceSite         site;
	b32            variadic;
	u32                arity;
	Ir               body;
	Ir              *captures;
	u32              capture_count;
	Entity         **capture_entities;
}
IrFunction;

typedef struct
{
	IrFunction *functions;
	u32         function_count;
	u32         entry_index;
}
IrModule;

// Todo, dude!
enum
{
	MAX_ENTITIES = 4096
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

typedef u32 EntityId;

typedef struct
{
	EntityId previous_scope_start;
}
EntityScope;

typedef struct
{
	u32 previous_defer_start;
}
DeferScope;

typedef struct
{
	Ir continue_label;
	Ir break_label;
	u32 defer_start;
}
LoopLabels;

typedef struct LowerContext LowerContext;
typedef struct FunctionLowerContext FunctionLowerContext;

struct FunctionLowerContext
{
	FunctionLowerContext *parent;
	IrFunction          *function;
	EntityId             scope_start;
};

typedef struct Entity
{
	EntityType    type;
	EntityTags    tags;
	u32          scope_start;
	elf_String     *name;
	SourceSite   site;
	Ir         memory_ir;
}
Entity;

struct LowerContext
{
	elf_State   *state;
	elf_Arena       *arena;
	elf_String    *source_name;
	u32          ir_stack_size;
	u32          ir_stack_index;
	Ir       *ir_stack;

	Ast      *defer_stack;
	u32          defer_stack_size;
	u32          defer_count;
	u32          defer_scope_start;
	u32          function_defer_start;

	IrFunction *functions;
	u32         num_functions;
	u32         max_functions;
	FunctionLowerContext *function;

	Entity     entities[MAX_ENTITIES];
	EntityId   scope_start;
	EntityId   scope_end;

	LoopLabels loop_stack[64];
	u32        loop_count;
};
