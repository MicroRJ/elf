//
// See Copyright Notice In elf.h
//
#ifndef PATH_H
#define PATH_H

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
Path_Stack;

Path_Stack path_new_stack(Arena *arena, u32 capacity);
void path_push_raw(Path_Stack *pb, const char *text);
void path_pop(Path_Stack *pb);
void path_push(Path_Stack *pb, const char *name);

#endif
