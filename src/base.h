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
#include <dy_fs.h>

#if !defined(false) && !defined(true)
enum
{
	false = 0,
	true  = 1,
};
#endif

typedef dy_u8  u8;
typedef dy_u16 u16;
typedef dy_u32 u32;
typedef dy_u64 u64;
typedef dy_i8  i8;
typedef dy_i16 i16;
typedef dy_i32 i32;
typedef dy_i64 i64;
typedef dy_f32 f32;
typedef dy_f64 f64;
typedef dy_b32 b32;


#define __FUNC__ __func__

#define XFUSE_(X,Y) X##Y
#define XFUSE(X,Y) XFUSE_(X,Y)

#define MAX(x,y) ((x) > (y) ? (x) : (y))
#define MIN(x,y) ((x) < (y) ? (x) : (y))
#define KILOBYTES DY_KILOBYTES
#define MEGABYTES DY_MEGABYTES
#define GIGABYTES DY_GIGABYTES
#define ARRAY_COUNT DY_ARRAY_COUNT

#define ASSERT assert
#define STATIC_ASSERT(x) _Static_assert(x, "no message")
#define NO_CODE ASSERT(!"NO_CODE")

#define ASSERT assert

typedef dy_Arena Arena;
typedef dy_Scratch Scratch;

#define zero_memory dy_memory_zero
#define copy_memory dy_memory_copy

#define arena_create dy_arena_create
#define arena_destroy dy_arena_destroy
#define arena_reset dy_arena_reset
#define arena_mark dy_arena_mark
#define arena_restore dy_arena_restore
#define arena_reserve dy_arena_reserve
#define arena_push dy_arena_push
#define arena_push_zero dy_arena_push_zero
#define arena_push_copy dy_arena_push_copy
#define arena_push_data dy_arena_push_data
#define arena_push_text dy_arena_push_text
#define arena_pushfv dy_arena_pushfv
#define arena_pushf dy_arena_pushf

#define scratch_begin dy_begin_scratch
#define scratch_begin_excluding dy_begin_different_scratch
#define scratch_end dy_end_scratch

#endif
