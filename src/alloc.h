/*
** See Copyright Notice In elf.h
** alloc.h
*/


#define elf_dealloc(cator,mem) elf_dealloc_(cator,mem,DEBUG_HERE)
#define elf_realloc(cator,sze,mem) elf_realloc_(cator,sze,mem,DEBUG_HERE)
#define elf_alloc(cator,sze) elf_alloc_(cator,sze,DEBUG_HERE)
#define elf_calloc(cator,sze) elf_calloc_(cator,sze,DEBUG_HERE)

#define ALLOC_FUNCTION(NAME) int (NAME)(int flags, elInteger old_size, elInteger new_size, void **old_memory, SourceInfo debug)
typedef ALLOC_FUNCTION(* elAllocator);

static void elf_dealloc_(elAllocator fn, void const *memory, SourceInfo info);
static void *elf_realloc_(elAllocator fn, elInteger size, void *memory, SourceInfo info);
static void *elf_alloc_(elAllocator fn, elInteger size, SourceInfo info);
static void *elf_calloc_(elAllocator fn, elInteger size, SourceInfo info);

static ALLOC_FUNCTION(tls_allocfn);
static ALLOC_FUNCTION(heap_allocfn);

#define TLS_ALLOCATOR (tls_allocfn)
#define HEAP_ALLOCATOR (heap_allocfn)

static void *elf_copy_memory(void *dst, void const *src, elInteger length);
static void *elf_clear_memory(void *target, elInteger length);

