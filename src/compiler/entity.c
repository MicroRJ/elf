//
// See Copyright Notice In elf.h
//

#include <dy.h>

#include "elf.h"
#include "base.h"
#include "atom_view.h"
#include "entity.h"
#include "ast.h"

Scope *scope_create(Arena *arena, ScopeKind kind, Scope *parent, struct Ast_T *owner, Com_Source *source)
{
	ASSERT(!owner || owner->source == source);
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

Entity *scope_find_current(Scope *scope, Atom *name)
{
	for (u32 i = 0; i < scope->count; ++i)
	{
		Entity *entity = scope->entities[i];
		if (entity->name == name) {
			return entity;
		}
	}
	return 0;
}

Entity *scope_find(Scope *scope, Atom *name)
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

Entity *scope_declare(Scope *scope, EntityKind kind, Atom *name)
{
	Entity *result = scope_find_current(scope, name);
	if (!result)
	{
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
		entity->kind = kind;
		entity->name = name;
		entity->scope = scope;
		scope->entities[scope->count++] = entity;
		result = entity;
	}
	return result;
}
