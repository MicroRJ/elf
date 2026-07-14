//
// See Copyright Notice In elf.h
//
#ifndef ARENA_H
#define ARENA_H

typedef struct
{
	u64 size, in_reserve, in_use;
	u8 *data;
}
Arena;

typedef struct
{
	Arena *arena;
	u64 regress;
}
Scratch;

Arena create_arena(u64 initial_reserve);
void destroy_arena(Arena *arena);

void *arena_reserve(Arena *arena, u64 size);
void *arena_push(Arena *arena, u64 size);
void *arena_push_zero(Arena *arena, u64 size);
void *arena_push_copy(Arena *arena, u64 size, const void *data);
char *arena_push_data(Arena *arena, const void *data, u64 size);
char *arena_push_text(Arena *arena, const char *text);
char *arena_push_char(Arena *arena, char chr);
void arena_push_repeat(Arena *arena, char chr, u32 count);

char *arena_pushfv(Arena *arena, const char *format, va_list args);
char *arena_pushf(Arena *arena, const char *format, ...);

Scratch get_scratch(void);
void end_scratch(Scratch scratch);

#endif
