//
// See Copyright Notice In elf.h
//
#ifndef ARENA_H
#define ARENA_H

#include <dy.h>

typedef dy_Arena elf_Arena;
typedef dy_Scratch elf_Scratch;

#define elf_arena_create dy_arena_create
#define elf_arena_destroy dy_arena_destroy
#define elf_arena_reserve dy_arena_reserve
#define elf_arena_align(arena, alignment) ((void)dy_arena_push((arena), 0, (alignment)))

static inline void *elf_arena_push(elf_Arena *arena, u64 size)
{
	void *result = dy_arena_reserve(arena, size);
	if (result) {
		arena->used += size;
	}
	return result;
}

static inline void *elf_arena_push_zero(elf_Arena *arena, u64 size)
{
	void *result = elf_arena_push(arena, size);
	if (result && size) {
		dy_memory_zero(result, size);
	}
	return result;
}

static inline void *elf_arena_push_copy(elf_Arena *arena, u64 size, const void *data)
{
	void *result = elf_arena_push(arena, size);
	if (result && size) {
		dy_memory_copy(result, data, size);
	}
	return result;
}

#define elf_arena_push_data dy_arena_push_data
#define elf_arena_push_text dy_arena_push_text
#define elf_arena_push_char dy_arena_push_char
#define elf_arena_push_nchar dy_arena_push_nchar
#define elf_arena_pushfv dy_arena_pushfv
#define elf_arena_pushf dy_arena_pushf
#define elf_begin_scratch dy_begin_scratch
#define elf_begin_scratch_excluding dy_begin_different_scratch
#define elf_end_scratch dy_end_scratch

#endif
