//
// See Copyright Notice In elf.h
//

static Entity entity_error_sentinel =
{
	.kind = ENTITY_ERROR,
	.status = COM_STATUS_REPORTED,
};

static Entity entity_oom_sentinel =
{
	.kind = ENTITY_ERROR,
	.status = COM_STATUS_OOM,
};

static b32 entity_is_constant(const Entity *entity)
{
	ASSERT(entity);
	b32 result = entity->kind == ENTITY_STATIC;
	if (entity->kind == ENTITY_LOCAL)
	{
		Ast declaration = entity->en_local.declaration;
		ASSERT(declaration);
		ASSERT(declaration->kind == AST_DECL_STAT);
		result = declaration->decl.constant;
	}
	return result;
}

static b32 entity_is_exported(const Entity *entity)
{
	ASSERT(entity);
	b32 result = false;
	if (entity->kind == ENTITY_STATIC)
	{
		Ast declaration = entity->en_static.declaration;
		ASSERT(declaration);
		ASSERT(declaration->kind == AST_STATIC_DECL);
		result = declaration->static_decl.exported;
	}
	return result;
}

static Scope *scope_create(Arena *arena, ScopeKind kind, Scope *parent, struct Ast_T *owner, Com_Source *source)
{
	ASSERT(!parent || kind == SCOPE_SOURCE || parent->source == source);
	Scope *scope = arena_push_zero(arena, sizeof(*scope), _Alignof(Scope));
	if (scope)
	{
		scope->arena = arena;
		scope->parent = parent;
		scope->owner = owner;
		scope->source = source;
		scope->kind = kind;
	}
	return scope;
}

// Todo, not sure if we should really consider buckets for entities with the same name ...
static Entity *scope_find_current(Scope *scope, Atom *name)
{
	for (u32 i = 0; i < scope->count; ++i)
	{
		Entity *entity = scope->entities[i];

		if (entity->name == name) {
			ASSERT(entity->count >= 1);
			ASSERT(entity->count != 1 || (entity->first == entity && entity->last == entity));
			return entity;
		}
	}
	return 0;
}

static Entity *scope_find(Scope *scope, Atom *name)
{
	for (Scope *current = scope; current; current = current->parent)
	{
		Entity *entity = scope_find_current(current, name);
		if (entity) {
			return entity;
		}
	}
	return 0;
}

static Entity *scope_declare(Scope *scope, EntityKind kind, Atom *name)
{
	ASSERT(kind != ENTITY_ERROR);
	Entity *anchor = scope_find_current(scope, name);
	ASSERT(!anchor || (anchor->count >= 1 && anchor->first == anchor));

	if (scope->count == scope->capacity)
	{
		u32 capacity = scope->capacity ? scope->capacity * 2 : 16;
		Entity **entities = arena_push(scope->arena, sizeof(*entities) * capacity, _Alignof(Entity *));
		if (!entities) {
			return 0;
		}
		if (scope->count) {
			copy_memory(entities, scope->entities, sizeof(*entities) * scope->count);
		}
		scope->entities = entities;
		scope->capacity = capacity;
	}

	Entity *entity = arena_push_zero(scope->arena, sizeof(*entity), _Alignof(Entity));

	if (!entity) {
		return 0;
	}

	if (!anchor) {
		anchor = entity;
	}
	anchor->count ++;
	entity->next = anchor->last;
	anchor->last = entity;

	entity->kind = kind;
	entity->name = name;
	entity->scope = scope;
	entity->first = entity;
	entity->last  = entity;

	scope->entities[scope->count ++] = entity;
	return entity;
}
