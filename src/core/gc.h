//
// See Copyright Notice In elf.h
//

#ifndef ELF_CORE_GC_H
#define ELF_CORE_GC_H

#define ELF_OBJECT_REACHABLE      1
#define ELF_OBJECT_READONLY       2

#define ELF_GC_NEXT_MIN ((u32)MEGABYTES(1))
#define ELF_GC_NEXT_MAX ((u32)GIGABYTES(1))

void *elf_gc_alloc(elf_State *state, elf_ObjectType type, u32 size);

#endif
