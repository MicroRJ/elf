/*
** See Copyright Notice In elf.h
** lmem.h
** Memory Tools
*/


typedef struct elAllocator elAllocator;
#define FLYTRAP 0x55555555

#define CHUNKSIZE 1024
#define CHUNKCATE(x,y) ((x+y-1)/y*y)




void langM_debugdealloc(void *mem, elSourceInfo loca);
void *langM_debugrealloc(void *mem, elInteger contentssize, elSourceInfo loca);
void *langM_debugalloc(elInteger contentssize, elSourceInfo loca);


elAPI void elf_dealloc_(elAllocator *allocator, void const *memory, elSourceInfo loca);
elAPI void *elf_realloc_(elAllocator *allocator, elInteger size, void *memory, elSourceInfo loca);
elAPI void *elf_alloc_(elAllocator *allocator, elInteger size, elSourceInfo loca);
elAPI void *elf_clearalloc_(elAllocator *allocator, elInteger size, elSourceInfo loca);

#define elf_dealloc(cator,mem) elf_dealloc_(cator,mem,LHERE)
#define langM_realloc(cator,sze,mem) elf_realloc_(cator,sze,mem,LHERE)
#define elf_alloc(cator,sze) elf_alloc_(cator,sze,LHERE)
#define elf_clear_alloc(cator,sze) elf_clearalloc_(cator,sze,LHERE)

#define ALLOCFN(NAME) elError NAME (elAllocator *allocator, int flags, elInteger oldSize, elInteger newSize, void **oldAndNewMemory, elSourceInfo loca)
elAPI ALLOCFN(elf_deftlsallocfn);
elAPI ALLOCFN(elf_defglobalallocfn);


/* todo: better names */
#define elTLS_ALLOCATOR (&elf_tlsalloc)
#define elHEAP_ALLOCATOR (&langM_globalalloc)
