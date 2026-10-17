//
// See Copyright Notice In elf.h
//

#include <dy.h>

#include "elf.h"
#include "base.h"
#include "atom_view.h"
#include "entity.h"

EntityTable *entity_table_create(Arena *arena)
{
	EntityTable *table = dy_arena_push_zero(arena, sizeof(*table), _Alignof(EntityTable));
	if (table) {
		table->arena = arena;
	}
	return table;
}

Entity *entity_table_find(EntityTable *table, Atom *name)
{
	for (u32 i = 0; i < table->count; ++i)
	{
		Entity *entity = table->entities[i];
		if (entity->name == name) {
			return entity;
		}
	}
	return 0;
}

Entity *entity_table_insert(EntityTable *table, Entity *entity)
{
	Entity *result = entity_table_find(table, entity->name);
	if (!result)
	{
		if (table->count == table->capacity)
		{
			u32 capacity = table->capacity ? table->capacity * 2 : 16;
			Entity **entities = dy_arena_push(table->arena, sizeof(*entities) * capacity, _Alignof(Entity *));
			if (!entities) {
				return 0;
			}
			if (table->count) {
				dy_memory_copy(entities, table->entities, sizeof(*entities) * table->count);
			}
			table->entities = entities;
			table->capacity = capacity;
		}
		table->entities[table->count++] = entity;
		result = entity;
	}
	return result;
}

Entity *entity_alloc(Arena *arena, EntityKind kind, Atom *name)
{
	Entity *entity = dy_arena_push_zero(arena, sizeof(*entity), _Alignof(Entity));
	if (entity)
	{
		entity->kind = kind;
		entity->name = name;
	}
	return entity;
}
