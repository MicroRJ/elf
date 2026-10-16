//
// See Copyright Notice In elf.h
//

typedef struct IrEntity IrEntity;
typedef struct IrFunctionEmitter IrFunctionEmitter;

enum
{
	MAX_ENTITIES = 4096
};

typedef enum
{
	ENTITY_INVALID = 0,
	ENTITY_STATIC_SYMBOL,
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
	union
	{
		Ir                memory_ir;
		Com_StaticSymbol *static_symbol;
	};
}
IrEntity;

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

// Temporary state used while emitting one function and any nested closures.
struct IrFunctionEmitter
{
	IrModule         *module;
	u32              ir_stack_size;
	u32              ir_stack_index;
	Ir              *ir_stack;

	Ast             *defer_stack;
	u32              defer_stack_size;
	u32              defer_count;
	u32              defer_scope_start;
	u32              function_defer_start;

	IrEntityId       scope_start;
	IrEntityId       scope_end;
	u32              static_entity_count;
	u32              scope_depth;

	Com_SourceId     current_source;

	LwScopeFunction *function;

	u32              loop_count;

	IrEntity         entities[MAX_ENTITIES];
	LoopLabels       loop_stack[64];
};

IrFunctionEmitter *ir_new_function_emitter(IrModule *module);
void ir_emit_function(IrFunctionEmitter *ctx, Com_Function *input);
