/*
** See Copyright Notice In elf.h
** alloc.h
*/


#define elf_dealloc(cator,mem) elf_dealloc_(cator,mem,DBG_SOURCE)
#define elf_realloc(cator,sze,mem) elf_realloc_(cator,sze,mem,DBG_SOURCE)
#define elf_alloc(cator,sze) elf_alloc_(cator,sze,DBG_SOURCE)
#define elf_calloc(cator,sze) elf_calloc_(cator,sze,DBG_SOURCE)

#define ALLOCATOR_FN(NAME) int (NAME)(void *user, int flags, elf_Int old_size, elf_Int new_size, void **memory, DBGSource debug)
typedef ALLOCATOR_FN(* elAllocator);

static void elf_dealloc_(elAllocator fn, void const *memory, DBGSource info);
static void *elf_realloc_(elAllocator fn, elf_Int size, void *memory, DBGSource info);
static void *elf_alloc_(elAllocator fn, elf_Int size, DBGSource info);
static void *elf_calloc_(elAllocator fn, elf_Int size, DBGSource info);

static ALLOCATOR_FN(tls_allocfn);
static ALLOCATOR_FN(heap_allocfn);

#define TLS_ALLOCATOR (tls_allocfn)
#define HEAP_ALLOCATOR (heap_allocfn)

