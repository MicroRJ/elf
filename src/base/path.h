//
// See Copyright Notice In elf.h
//
#ifndef ELF_PATH_H
#define ELF_PATH_H

#ifndef PATH_BUILDER
#define PATH_BUILDER
#endif

typedef struct
{
	int   pcap;
	int   pcur;
	char *path;
	short segs;
	char *name;
}
elf_PathStack;

elf_PathStack elf_alloc_path_stack(elf_Arena *arena, u32 capacity);
void elf_path_pop(elf_PathStack *path);
void elf_path_push(elf_PathStack *path, const char *name);

#endif
