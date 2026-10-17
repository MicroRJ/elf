//
// See Copyright Notice In elf.h
//
#ifndef ELF_BASE_H
#define ELF_BASE_H

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <stdarg.h>
#include <string.h>

#include "elf.h"
#include <dy.h>

#if !defined(false) && !defined(true)
enum
{
	false = 0,
	true  = 1,
};
#endif

typedef elf_u8  u8;
typedef elf_u16 u16;
typedef elf_u32 u32;
typedef elf_u64 u64;
typedef elf_i8  i8;
typedef elf_i16 i16;
typedef elf_i32 i32;
typedef elf_i64 i64;
typedef elf_f32 f32;
typedef elf_f64 f64;
typedef elf_b32 b32;


#define __FUNC__ __func__

#define XFUSE_(X,Y) X##Y
#define XFUSE(X,Y) XFUSE_(X,Y)

#define MAX(x,y) ((x) > (y) ? (x) : (y))
#define MIN(x,y) ((x) < (y) ? (x) : (y))
#define KILOBYTES(x) ((x) << 10)
#define MEGABYTES(x) ((x) << 20)
#define GIGABYTES(x) ((x) << 30)
#define ARRAY_COUNT(X) (sizeof(X) / sizeof((X)[0]))

#define ASSERT assert
#define STATIC_ASSERT(x) _Static_assert(x, "no message")
#define NO_CODE ASSERT(!"NO_CODE")


static inline void *zero_memory(void *mem, u64 size)
{
	memset(mem, 0, size);
	return mem;
}

static inline void *copy_memory(void *dst, const void *src, u64 size)
{
	memcpy(dst, src, size);
	return dst;
}

typedef dy_Arena elf_Arena;
typedef dy_Scratch elf_Scratch;

typedef dy_Arena Arena;

#define elf_arena_create dy_arena_create
#define elf_arena_destroy dy_arena_destroy
#define elf_arena_reset dy_arena_reset
#define elf_arena_mark dy_arena_mark
#define elf_arena_restore dy_arena_restore
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
