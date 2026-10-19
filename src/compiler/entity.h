#ifndef ELF_ENTITY_H
#define ELF_ENTITY_H

#include "compiler.h"

typedef struct Entity Entity;
typedef struct Scope Scope;
typedef struct IrFunction IrFunction;

typedef enum
{
	STATIC_VALUE_NONE = 0,
	STATIC_VALUE_AST,
	STATIC_VALUE_RESOLVING,
	STATIC_VALUE_ERROR,
	STATIC_VALUE_NIL,
	STATIC_VALUE_NUMBER,
	STATIC_VALUE_INTEGER,
	STATIC_VALUE_STRING,
	STATIC_VALUE_FUNCTION,
	STATIC_VALUE_EXPORTS,
}
StaticValueType;

typedef struct
{
	StaticValueType type;
	union
	{
		Com_Status   status;
		struct Ast_T *ast;
		f64          number;
		i64          integer;
		struct Atom *string;
		Scope       *exports;
		struct
		{
			struct Ast_T *function;
			IrFunction   *definition;
		};
	};
}
StaticValue;

typedef enum
{
	ENTITY_NONE = 0,
	ENTITY_ERROR,
	ENTITY_STATIC,
	ENTITY_LOCAL,
	ENTITY_GLOBAL,
}
EntityKind;

typedef enum
{
	ENTITY_TAG_NONE       = 0,
	ENTITY_TAG_REFERENCED = 1 << 0,
	ENTITY_TAG_CONSTANT   = 1 << 1,
	ENTITY_TAG_ASSIGNED   = 1 << 2,
	ENTITY_TAG_EXPORTED   = 1 << 5,
}
EntityTags;

struct Entity
{
	EntityKind      kind;
	Com_Status      status;
	struct Atom    *name;
	Scope          *scope;
	EntityTags      tags;
	union
	{
		struct
		{
			struct Ast_T *declaration;
			StaticValue   value;
		}
		en_static;
		struct
		{
			struct Ast_T  *function;
			struct IrNode *storage;
		}
		en_local;
	};
};

extern Entity entity_error_sentinel;
extern Entity entity_oom_sentinel;

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

typedef enum
{
	SCOPE_NONE = 0,
	SCOPE_GLOBAL,
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

Scope *scope_create(Arena *arena, ScopeKind kind, Scope *parent, struct Ast_T *owner, Com_Source *source);
Entity *scope_find_current(Scope *scope, struct Atom *name);
Entity *scope_find(Scope *scope, struct Atom *name);
Entity *scope_declare(Scope *scope, EntityKind kind, struct Atom *name, b32 *created);

#endif
