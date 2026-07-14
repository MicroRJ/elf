//
// See Copyright Notice In elf.h
//
#ifndef BASE_H
#define BASE_H

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <stdarg.h>
#include <string.h>

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
#include "path.h"
#include "matcher.h"

#endif
