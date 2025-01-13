/*
** See Copyright Notice In elf.h
** alloc.h
*/



#define ALLOCATOR_FN(NAME) int (NAME)(void *user, int flags, elf_Int old_size, elf_Int new_size, void **memory, DBGSource debug)
typedef ALLOCATOR_FN(* Allocator);


static ALLOCATOR_FN(thread_allocator);
static ALLOCATOR_FN(global_allocator);


#define THREAD_ALLOCATOR (thread_allocator)
#define GLOBAL_ALLOCATOR (global_allocator)


#define dealloc_memory(allocator,memory)       dealloc_memory_debug(allocator,memory,DBG_SOURCE)
#define realloc_memory(allocator,size,memory)  realloc_memory_debug(allocator,size,memory,DBG_SOURCE)
#define alloc_memory(allocator,size)           alloc_memory_debug(allocator,size,DBG_SOURCE)
#define calloc_memory(allocator,size)          calloc_memory_debug(allocator,size,DBG_SOURCE)


static void  dealloc_memory_debug(Allocator fn, void const *memory, DBGSource debug);
static void *realloc_memory_debug(Allocator fn, elf_Int size, void *memory, DBGSource debug);
static void *alloc_memory_debug(Allocator fn, elf_Int size, DBGSource debug);
static void *calloc_memory_debug(Allocator fn, elf_Int size, DBGSource debug);
