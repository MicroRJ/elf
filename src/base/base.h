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

#include "arena.h"
#include "profiler.h"

#endif
