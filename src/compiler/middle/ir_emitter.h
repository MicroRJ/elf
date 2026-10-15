//
// See Copyright Notice In elf.h
//

#define IMPLICIT_PARAM_INDEX 0
#define IMPLICIT_PARAM_COUNT 1

typedef struct IrEntity IrEntity;
typedef struct IrFunction IrFunction;


enum
{
	MAX_ENTITIES = 4096
};

#define NO_ENTITY 0xDEADBEEF

typedef enum
{
	ENTITY_INVALID = 0,
	// TODO(RJ): to be removed!
	ENTITY_DIRECTORY,
	ENTITY_LOCAL_DECLARATION,
}
IrEntityType;

typedef enum
{
	ENTITY_TAG_REFERENCED  = 1 << 0,
	ENTITY_TAG_CONSTANT    = 1 << 1,
	ENTITY_TAG_ASSIGNED    = 1 << 2,
	ENTITY_TAG_PARAMETER   = 1 << 3,
	ENTITY_TAG_FORLOOP     = 1 << 4,
}
IrEntityTags;

typedef u32 IrEntityId;

typedef struct IrEntity
{
	IrEntityType  type;
	IrEntityTags  tags;
	u32           scope_start;
	Atom         *name;
	SourceSite    site;
	Ir            memory_ir;
}
IrEntity;

struct IrFunction
{
	SourceSite      site;
	b32             variadic;
	b32             is_static;
	Ir              body;
	Ir             *params;
	u32             param_count;
	u32             capture_count;
};

// One lowered compilation unit. Global indices are still module-local here and
// are resolved to runtime slots only when the compiled module is linked.
typedef struct
{
	IrFunction *functions;
	u32         function_count;
	Atom      **globals;
	u32         global_count;
	b32         has_entry;
	// TODO(RJ): just make index zero be the entry function!
	u32         entry_index;
}
IrModule;

typedef struct
{
	IrEntityId previous_scope_start;
	u32        previous_scope_depth;
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

typedef struct IrModuleEmitter IrModuleEmitter;
typedef struct IrFunctionEmitter IrFunctionEmitter;
typedef struct LwScopeFunction LwScopeFunction;
typedef struct LwCapture LwCapture;

// One capture discovered while lowering a function body. The list exists only
// until its ordered source expressions are copied into an IR function value.
struct LwCapture
{
	LwCapture *next;
	IrEntity  *entity;
	Ir         source;
};

struct LwScopeFunction
{
	LwScopeFunction *parent;
	IrFunction      *function;
	LwCapture       *first_capture;
	LwCapture       *last_capture;
	u32              capture_count;
	IrEntityId       scope_start;
};

// Owns the module-wide IR arrays shared by every function emitter.
struct IrModuleEmitter
{
	Compiler  *compiler;
	elf_Arena *arena;
	IrModule   module;
	u32        function_capacity;
	u32        global_capacity;
};

// Temporary state used while emitting one function and any nested closures.
struct IrFunctionEmitter
{
	IrModuleEmitter *module;
	u32          ir_stack_size;
	u32          ir_stack_index;
	Ir          *ir_stack;

	Ast        *defer_stack;
	u32         defer_stack_size;
	u32         defer_count;
	u32         defer_scope_start;
	u32         function_defer_start;

	LwScopeFunction *function;

	IrEntity     entities[MAX_ENTITIES];
	IrEntityId   scope_start;
	IrEntityId   scope_end;
	u32        scope_depth;

	Com_SourceId current_source;

	LoopLabels loop_stack[64];
	u32        loop_count;
};
