#ifndef ELF_ENTITY_H
#define ELF_ENTITY_H

#include "compiler.h"

typedef struct Entity Entity;
typedef struct EntityTable EntityTable;
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
		EntityTable *exports;
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
	EntityState     state;
	u32             users;
	EntityTags      tags;
	Com_SourceId    source;
	struct Ast_T   *parent;
	struct Ast_T   *expr;
	struct Ast_T   *tree;
	struct IrNode  *ir;
	StaticValue     value;
};

struct EntityTable
{
	Arena   *arena;
	Entity **entities;
	u32      count;
	u32      capacity;
};

EntityTable *entity_table_create(Arena *arena);
Entity *entity_table_find(EntityTable *table, struct Atom *name);
Entity *entity_table_insert(EntityTable *table, Entity *entity);
Entity *entity_alloc(Arena *arena, EntityKind kind, struct Atom *name);

#endif
