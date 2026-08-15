//
// See Copyright Notice In elf.h
//

#ifndef ELF_CORE_GC_H
#define ELF_CORE_GC_H

typedef struct elf_State elf_State;

typedef enum
{
	ELF_GC_ACTIVE = 0,
	ELF_GC_PAUSED,
}
elf_GCMode;


#define ELF_OBJECT_REACHABLE      1
#define ELF_OBJECT_READONLY       2

struct elf_Object
{
	u8    type;
	u8    status;
	u16   unused;
	u32   size;
};

typedef enum
{
	ELF_OBJECT_BASE = 0,
	ELF_OBJECT_CLOSURE,
	ELF_OBJECT_TABLE,
	ELF_OBJECT_STRING,
}
elf_ObjectType;

#define ELF_GC_NEXT_MIN ((u32)MEGABYTES(1))
#define ELF_GC_NEXT_MAX ((u32)GIGABYTES(1))

void *elf_gc_alloc(elf_State *state, elf_ObjectType type, u32 size);

#endif
