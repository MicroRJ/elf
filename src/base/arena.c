//
// See Copyright Notice In elf.h
//

#include <dy.h>

#define STB_SPRINTF_STATIC
#define STB_SPRINTF_IMPLEMENTATION
#include "stb_sprintf.h"

static _Thread_local elf_Arena scratch_arena;

elf_Scratch elf_begin_scratch(void)
{
	if (!scratch_arena.data) {
		scratch_arena = elf_arena_create(0);
	}

	elf_Scratch scratch;
	scratch.arena = &scratch_arena;
	scratch.regress = scratch_arena.in_use;
	return scratch;
}

void elf_end_scratch(elf_Scratch scratch)
{
	ASSERT(scratch.regress <= scratch.arena->in_use);
	scratch.arena->in_use = scratch.regress;
}

void elf_arena_destroy(elf_Arena *arena)
{
	dy_virtual_release(arena->data, arena->size);
}

elf_Arena elf_arena_create(u64 reserve)
{
	if (reserve == 0) {
		reserve = MEGABYTES(64);
	}

	elf_Arena arena = {};
	arena.size = reserve;
	arena.in_reserve = reserve;
	arena.data = dy_virtual_reserve(reserve);
	if (arena.data && !dy_virtual_commit(arena.data, reserve)) {
		dy_virtual_release(arena.data, reserve);
		arena.data = NULL;
	}
	return arena;
}

void *elf_arena_reserve(elf_Arena *arena, u64 size)
{
	if (size > arena->size - arena->in_use) abort();
	if (arena->in_use + size >= arena->in_reserve) {
		arena->in_reserve = arena->in_use + size;
		ASSERT(arena->in_reserve <= arena->size);
	}
	void *data = arena->data + arena->in_use;
	return data;
}

void elf_arena_align(elf_Arena *arena, u64 alignment)
{
	ASSERT(alignment && !(alignment & (alignment - 1)));
	u64 padding = (alignment - (arena->in_use & (alignment - 1))) & (alignment - 1);
	if (padding) elf_arena_push_zero(arena, padding);
}

void *elf_arena_push(elf_Arena *arena, u64 size)
{
	void *data = elf_arena_reserve(arena, size);
	arena->in_use += size;
	return data;
}

void *elf_arena_push_zero(elf_Arena *arena, u64 size)
{
	void *data = elf_arena_push(arena, size);
	memset(data, 0, size);
	return data;
}

void *elf_arena_push_copy(elf_Arena *arena, u64 size, const void *data)
{
	void *copy = elf_arena_push(arena, size);
	memcpy(copy, data, size);
	return copy;
}

char *elf_arena_push_data(elf_Arena *arena, const void *data, u64 size)
{
	char *copy = elf_arena_push(arena, size);
	memcpy(copy, data, size);
	return copy;
}

char *elf_arena_push_text(elf_Arena *arena, const char *text)
{
	return elf_arena_push_data(arena, text, strlen(text));
}

char *elf_arena_push_char(elf_Arena *arena, char chr)
{
	char *data = elf_arena_push(arena, 1);
	*data = chr;
	return data;
}

void elf_arena_push_nchar(elf_Arena *arena, char chr, u32 count)
{
	char *data = elf_arena_push(arena, count);
	memset(data, chr, count);
}

char *elf_arena_pushfv(elf_Arena *arena, const char *format, va_list args)
{
	va_list args_copy;
	va_copy(args_copy, args);
	int length = stbsp_vsnprintf(0, 0, format, args_copy);
	va_end(args_copy);

	ASSERT(length >= 0);
	u64 size = (u64)length;

	char *data = elf_arena_reserve(arena, size + 1);
	arena->in_use += size;

	stbsp_vsnprintf(data, size + 1, format, args);
	return data;
}

char *elf_arena_pushf(elf_Arena *arena, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	char *data = elf_arena_pushfv(arena, format, args);
	va_end(args);
	return data;
}

