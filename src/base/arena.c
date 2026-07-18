//
// See Copyright Notice In elf.h
//

#define STB_SPRINTF_STATIC
#define STB_SPRINTF_IMPLEMENTATION
#include "stb_sprintf.h"

static _Thread_local Arena scratch_arena;

Scratch get_scratch(void)
{
	if (!scratch_arena.data) {
		scratch_arena = create_arena(0);
	}

	Scratch scratch;
	scratch.arena = &scratch_arena;
	scratch.regress = scratch_arena.in_use;
	return scratch;
}

void end_scratch(Scratch scratch)
{
	ASSERT(scratch.regress <= scratch.arena->in_use);
	scratch.arena->in_use = scratch.regress;
}

void destroy_arena(Arena *arena)
{
	sys_virtual_free(arena->data);
}

Arena create_arena(u64 reserve)
{
	if (reserve == 0) {
		reserve = MEGABYTES(64);
	}

	Arena arena = {};
	arena.size = reserve;
	arena.in_reserve = reserve;
	arena.data = sys_virtual_alloc(reserve);
	return arena;
}

void *arena_reserve(Arena *arena, u64 size)
{
	if (arena->in_use + size >= arena->in_reserve) {
		arena->in_reserve = arena->in_use + size;
		ASSERT(arena->in_reserve <= arena->size);
	}
	void *data = arena->data + arena->in_use;
	return data;
}

void *arena_push(Arena *arena, u64 size)
{
	void *data = arena_reserve(arena, size);
	arena->in_use += size;
	return data;
}

void *arena_push_zero(Arena *arena, u64 size)
{
	void *data = arena_push(arena, size);
	memset(data, 0, size);
	return data;
}

void *arena_push_copy(Arena *arena, u64 size, const void *data)
{
	void *copy = arena_push(arena, size);
	memcpy(copy, data, size);
	return copy;
}

char *arena_push_data(Arena *arena, const void *data, u64 size)
{
	char *copy = arena_push(arena, size);
	memcpy(copy, data, size);
	return copy;
}

char *arena_push_text(Arena *arena, const char *text)
{
	return arena_push_data(arena, text, strlen(text));
}

char *arena_push_char(Arena *arena, char chr)
{
	char *data = arena_push(arena, 1);
	*data = chr;
	return data;
}

void arena_push_repeat(Arena *arena, char chr, u32 count)
{
	char *data = arena_push(arena, count);
	memset(data, chr, count);
}

char *arena_pushfv(Arena *arena, const char *format, va_list args)
{
	va_list args_copy;
	va_copy(args_copy, args);
	int length = stbsp_vsnprintf(0, 0, format, args_copy);
	va_end(args_copy);

	ASSERT(length >= 0);
	u64 size = (u64)length;

	char *data = arena_reserve(arena, size + 1);
	arena->in_use += size;

	stbsp_vsnprintf(data, size + 1, format, args);
	return data;
}

char *arena_pushf(Arena *arena, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	char *data = arena_pushfv(arena, format, args);
	va_end(args);
	return data;
}

