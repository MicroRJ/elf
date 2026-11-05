#ifndef ELF_ENTITY_H
#define ELF_ENTITY_H

typedef struct Entity Entity;
typedef struct Scope Scope;


typedef struct
{
	// TODO(RJ): this is because the VM uses type rules for runtime checking ...
	TypeRule    rule;
}
Com_Type;

// Todo, we don't need another constant value type here, we just need to ensure that the AST
// can be evaluated statically and serve the value as an AST value, let the ir stage handle
// the rest ...
#define COM_VALUE_TYPES(X)               \
	X(NONE,            "none")            \
	X(ERROR,           "error")           \
	X(TYPE,            "type")            \
	X(TYPE_RULE,       "type_rule")       \
	X(FUNCTION,        "function")        \
	X(IR,              "ir")              \
	X(AST,             "ast")             \
	X(RESOLVING,       "resolving")       \
	X(NUMBER,          "number")          \
	X(INTEGER,         "integer")         \
	X(STRING,          "string")          \
	X(EXPORTS,         "exports")         \
	X(SCOPE,           "scope")           \
	X(NATIVE_FUNCTION, "native_function") \
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

// Todo, prefix fields with cv_ for easier grepping!
typedef struct
{
	// Todo, rename to kind
	Com_ValueType type;
	union
	{
		Com_Status    status;
		IrFunction   *function;
		Ir            ir;
		Ast           ast;
		// Todo, rename to type
		Com_Type     *com_type;
		f64           number;
		i64           integer;
		struct Atom  *string;
		Scope        *exports;
		Scope        *scope;
		u32           native_function_index;
	};
}
Com_Value;

typedef enum
{
	ENTITY_TAG_NONE = 0,
	ENTITY_TAG_CONSTANT = 1 << 0,
	ENTITY_TAG_STATIC   = 1 << 1,
	ENTITY_TAG_EXPORTED = 1 << 2,
}
Entity_Tags;

struct Entity
{
	Com_Status    status;

	u32            count;
	Entity        *first;
	Entity         *last;

	Entity         *next;
	Entity         *next_in_compiler;

	Entity_Tags     tags;

	struct Atom    *name;
	Scope          *scope;
	Ast             declaration;
	Com_Value       value;
	b32             referenced;
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
	return entity && entity->status != COM_STATUS_OK;
}

static inline Com_Status entity_status(const Entity *entity)
{
	ASSERT(entity_is_error(entity));
	return entity->status;
}

static inline b32 entity_is_constant(const Entity *entity)
{
	ASSERT(entity);
	ASSERT(!entity_is_error(entity));
	return (entity->tags & ENTITY_TAG_CONSTANT) != 0;
}

static inline b32 entity_is_exported(const Entity *entity)
{
	ASSERT(entity);
	ASSERT(!entity_is_error(entity));
	return (entity->tags & ENTITY_TAG_EXPORTED) != 0;
}

struct Scope
{
	Arena          *arena;
	Scope          *parent;
	Ast             owner;
	Com_Source     *source;
	Entity        **entities;
	u32             count;
	u32             capacity;
};

static Scope *scope_create(Arena *arena, Scope *parent, Ast owner, Com_Source *source);
static Entity *scope_find_current(Scope *scope, struct Atom *name);
static Entity *scope_find(Scope *scope, struct Atom *name);
#endif
