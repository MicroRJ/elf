//
// See Copyright Notice In elf.h
//

#define IMPLICIT_PARAM_INDEX 0
#define IMPLICIT_PARAM_COUNT 1

typedef struct LwScopeEntity LwScopeEntity;

typedef struct
{
	SourceSite         site;
	b32            variadic;
	Ir               body;
	Ir              *params;
	u32              param_count;
	Ir              *captures;
	u32              capture_count;
	LwScopeEntity         **capture_entities;
}
IrFunction;

typedef struct
{
	IrFunction *functions;
	u32         function_count;
	u32         entry_index;
}
IrModule;

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
}
EntityTags;

typedef enum
{
	ENTITY_INVALID = 0,
	ENTITY_DIRECTORY,
	ENTITY_LOCAL_DECLARATION,
}
EntityType;

typedef enum
{
	STATIC_SYMBOL_UNRESOLVED,
	STATIC_SYMBOL_RESOLVING,
	STATIC_SYMBOL_RESOLVED,
}
StaticSymbolState;

typedef enum
{
	STATIC_VALUE_INVALID,
	STATIC_VALUE_NIL,
	STATIC_VALUE_INTEGER,
	STATIC_VALUE_NUMBER,
	STATIC_VALUE_STRING,
	STATIC_VALUE_FUNCTION,
}
StaticValueKind;

typedef struct
{
	StaticValueKind kind;
	union
	{
		i64   integer;
		f64   number;
		Atom *string;
		Ir    function;
	};
}
StaticValue;

typedef u32 EntityId;

typedef struct
{
	EntityId previous_scope_start;
	u32      previous_scope_depth;
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
typedef struct LwScopeFunction LwScopeFunction;

struct LwScopeFunction
{
	LwScopeFunction *parent;
	IrFunction      *function;
	EntityId         scope_start;
};

typedef struct LwScopeEntity
{
	EntityType   type;
	EntityTags   tags;
	u32          scope_start;
	Atom    *name;
	SourceSite   site;
	Ir           memory_ir;
}
LwScopeEntity;

typedef struct
{
	Atom             *name;
	SourceSite        site;
	Ast               expr;
	StaticSymbolState state;
	StaticValue       value;
	b32               referenced;
}
LwFileStatic;

struct LowerContext
{
	Compiler    *compiler;
	elf_State   *state;
	elf_Arena   *arena;
	u32          ir_stack_size;
	u32          ir_stack_index;
	Ir          *ir_stack;

	Ast        *defer_stack;
	u32         defer_stack_size;
	u32         defer_count;
	u32         defer_scope_start;
	u32         function_defer_start;

	IrFunction           *functions;
	u32                   num_functions;
	u32                   max_functions;
	LwScopeFunction *function;

	LwScopeEntity     entities[MAX_ENTITIES];
	EntityId   scope_start;
	EntityId   scope_end;
	u32        scope_depth;

	LwFileStatic *file_statics;
	u32           file_static_count;
	u32           file_static_capacity;

	LoopLabels loop_stack[64];
	u32        loop_count;
};
