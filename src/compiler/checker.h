#ifndef CHECKER_H
#define CHECKER_H
//
// TODO(RJ): we're exposing too much here but this is just because I don't quite know what this is going
// to turn out to be like!
//
typedef struct Entity      Entity;
typedef struct EntityTable EntityTable;
typedef struct IrFunction    IrFunction;

// TODO(RJ): def use a hash-table for globals!
struct EntityTable {
	Arena   *arena;
	Entity **entities;
	u32      count;
	u32      capacity;
};


EntityTable *entity_table_create(Arena *arena);
Entity *entity_table_find(EntityTable *table, Atom *name);
Entity *entity_table_insert(EntityTable *table, Entity *entity);

typedef enum {
	STATIC_VALUE_NONE = 0,
	STATIC_VALUE_NIL,
	STATIC_VALUE_NUMBER,
	STATIC_VALUE_INTEGER,
	STATIC_VALUE_STRING,
	STATIC_VALUE_FUNCTION,
	STATIC_VALUE_EXPORTS,
}
StaticValueType;


typedef struct {
	StaticValueType type;
	union {
		f64          number;
		i64          integer;
		Atom        *string;
		Atom        *unident;
		EntityTable *exports;
		struct {
			Ast         function;
			IrFunction *definition;
		};
	};
}
StaticValue;

typedef enum {
	ENTITY_NONE = 0,
	ENTITY_STATIC,
	ENTITY_PARAMETER,
	ENTITY_LOCAL,
	ENTITY_GLOBAL,
}
EntityKind;

typedef enum {
	ENTITY_TAG_NONE       = 0,
	ENTITY_TAG_REFERENCED = 1 << 0,
	ENTITY_TAG_CONSTANT   = 1 << 1,
	ENTITY_TAG_ASSIGNED   = 1 << 2,
	ENTITY_TAG_PARAMETER  = 1 << 3,
	ENTITY_TAG_FORLOOP    = 1 << 4,
}
EntityTags;

// NOTE(RJ): anything that has a name, whether a local, a symbol, a global, anything ...
// This is persistent across the compilation process and it's locally unique, unless it's
// a global, in which case, it's globally unique, but not universally unique? It's unique
// within whatever scale it's in ...
// Ast's reference entities, so that we can conveniently do ast -> entity -> ir.
struct Entity {
	EntityKind  kind;
	Atom       *name;
	Ast         tree;
	Ast         parent;
	Ir          ir;
	EntityTags  tags;
	u32         users;
	StaticValue value;
};

enum {MAX_ENTITIES=4096};

typedef struct Checker Checker;
struct Checker {
	Compiler     *compiler;
	EntityTable  *globals;
	Ast           current_function;
	u32           scope_start;
	u32           entity_index;
	Entity       *entity_stack[MAX_ENTITIES];
};

Checker *checker_run(Compiler *compiler, Ast tree);
Entity *entity_alloc(Arena *arena, EntityKind kind, Atom *name);

#endif
