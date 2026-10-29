#ifndef ELF_ENTITY_H
#define ELF_ENTITY_H

typedef struct Entity Entity;
typedef struct Scope Scope;


typedef struct
{
	Ast         ast;
	Com_Source *source;
	SourceSite  site;
}
Com_Type;

#define COM_VALUE_TYPES(X)               \
	X(NONE,            "none")            \
	X(ERROR,           "error")           \
	X(TYPE,            "type")            \
	X(AST,             "ast")             \
	X(RESOLVING,       "resolving")       \
	X(NIL,             "nil")             \
	X(NUMBER,          "number")          \
	X(INTEGER,         "integer")         \
	X(STRING,          "string")          \
	X(FUNCTION,        "function")        \
	X(EXPORTS,         "exports")         \
	X(SCOPE,           "scope")           \
	X(NATIVE_FUNCTION, "native function") \
/* end */

typedef enum
{
#define COM_VALUE_ENUM(name, text) COM_VALUE_##name,
	COM_VALUE_TYPES(COM_VALUE_ENUM)
#undef COM_VALUE_ENUM
	COM_VALUE_COUNT,
}
Com_ValueType;

static inline const char *com_value_type_name(Com_ValueType type)
{
	switch (type)
	{
#define COM_VALUE_NAME(name, text) case COM_VALUE_##name: return text;
		COM_VALUE_TYPES(COM_VALUE_NAME)
#undef COM_VALUE_NAME
		default: return "unknown";
	}
}

typedef struct
{
	Com_ValueType type;
	union
	{
		Com_Status    status;
		struct Ast_T *ast;
		Com_Type     *com_type;
		f64           number;
		i64           integer;
		struct Atom  *string;
		Scope        *exports;
		Scope        *scope;
		struct Ast_T *function;
		u32           native_function_index;
	};
}
Com_Value;

typedef enum
{
	ENTITY_NONE = 0,
	ENTITY_ERROR,
	ENTITY_STATIC,
	ENTITY_LOCAL,
}
EntityKind;

struct Entity
{
	EntityKind      kind;
	Com_Status      status;

	u32            count;
	Entity        *first;
	Entity         *last;

	Entity         *next;

	struct Atom    *name;
	Scope          *scope;
	b32             referenced;
	union
	{
		struct
		{
			struct Ast_T *declaration;
			Com_Value     value;
		}
		en_static;
		struct
		{
			struct Ast_T  *declaration;
			struct Ast_T  *function;
			struct IrNode *storage;
			b32            assigned;
		}
		en_local;
	};
};

static Entity entity_error_sentinel;
static Entity entity_oom_sentinel;

#define ERROR_ENTITY     (&entity_error_sentinel)
#define ERROR_OOM_ENTITY (&entity_oom_sentinel)

static inline Entity *entity_from_status(Com_Status status)
{
	switch (status)
	{
		case COM_STATUS_REPORTED: return ERROR_ENTITY;
		case COM_STATUS_OOM:      return ERROR_OOM_ENTITY;
		default: ASSERT(0);       return ERROR_ENTITY;
	}
}

static inline b32 entity_is_error(const Entity *entity)
{
	return entity && entity->kind == ENTITY_ERROR;
}

static b32 entity_is_constant(const Entity *entity);
static b32 entity_is_exported(const Entity *entity);

typedef enum
{
	SCOPE_NONE = 0,
	SCOPE_GLOBAL,
	SCOPE_NAMESPACE,
	SCOPE_SOURCE,
	SCOPE_FUNCTION,
	SCOPE_BLOCK,
}
ScopeKind;

struct Scope
{
	Arena          *arena;
	Scope          *parent;
	struct Ast_T   *owner;
	Com_Source     *source;
	ScopeKind       kind;
	Entity        **entities;
	u32             count;
	u32             capacity;
};

static Scope *scope_create(Arena *arena, ScopeKind kind, Scope *parent, struct Ast_T *owner, Com_Source *source);
static Entity *scope_find_current(Scope *scope, struct Atom *name);
static Entity *scope_find(Scope *scope, struct Atom *name);
#endif
