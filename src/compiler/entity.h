#ifndef ELF_ENTITY_H
#define ELF_ENTITY_H

#include "compiler.h"

typedef struct Entity Entity;
typedef struct Scope Scope;
typedef struct IrFunction IrFunction;

typedef enum
{
	STATIC_VALUE_NONE = 0,
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
		f64          number;
		i64          integer;
		struct Atom *string;
		struct Atom *unident;
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
	ENTITY_STATIC,
	ENTITY_PARAMETER,
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
	ENTITY_TAG_PARAMETER  = 1 << 3,
	ENTITY_TAG_FORLOOP    = 1 << 4,
	ENTITY_TAG_EXPORTED   = 1 << 5,
}
EntityTags;

typedef enum
{
	ENTITY_UNRESOLVED = 0,
	ENTITY_RESOLVING,
	ENTITY_RESOLVED,
}
EntityState;

struct Entity
{
	EntityKind      kind;
	struct Atom    *name;
	Scope          *scope;
	EntityState     state;
	u32             users;
	EntityTags      tags;
	struct Ast_T   *parent;
	struct Ast_T   *expr;
	struct Ast_T   *tree;
	struct IrNode  *ir;
	StaticValue     value;
};

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
Entity *scope_declare(Scope *scope, EntityKind kind, struct Atom *name);

#endif
