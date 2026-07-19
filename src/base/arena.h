//
// See Copyright Notice In elf.h
//
#ifndef ARENA_H
#define ARENA_H

typedef struct elf_Arena
{
	u64 size;
	u64 in_reserve;
	u64 in_use;
	u8 *data;
}
elf_Arena;

typedef struct elf_Scratch
{
	elf_Arena *arena;
	u64 regress;
}
elf_Scratch;

elf_Arena elf_arena_create(u64 initial_reserve);
void elf_arena_destroy(elf_Arena *arena);

void *elf_arena_reserve(elf_Arena *arena, u64 size);
void *elf_arena_push(elf_Arena *arena, u64 size);
void *elf_arena_push_zero(elf_Arena *arena, u64 size);
void *elf_arena_push_copy(elf_Arena *arena, u64 size, const void *data);
char *elf_arena_push_data(elf_Arena *arena, const void *data, u64 size);
char *elf_arena_push_text(elf_Arena *arena, const char *text);
char *elf_arena_push_char(elf_Arena *arena, char chr);
void elf_arena_push_nchar(elf_Arena *arena, char chr, u32 count);

char *elf_arena_pushfv(elf_Arena *arena, const char *format, va_list args);
char *elf_arena_pushf(elf_Arena *arena, const char *format, ...);

elf_Scratch elf_begin_scratch(void);
void elf_end_scratch(elf_Scratch scratch);

#endif
