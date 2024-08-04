/*
** See Copyright Notice In elf.h
** lmem.h
** Memory Tools
*/

#define FLYTRAP 0x55555555

#define CHUNKSIZE 1024
#define CHUNKCATE(x,y) ((x+y-1)/y*y)




void langM_debugdealloc(void *mem, elSourceInfo loca);
void *langM_debugrealloc(void *mem, elInteger contentssize, elSourceInfo loca);
void *langM_debugalloc(elInteger contentssize, elSourceInfo loca);



#define ALLOCFN(NAME) elError NAME (elAllocator *allocator, int flags, elInteger oldSize, elInteger newSize, void **oldAndNewMemory, elSourceInfo loca)
elAPI ALLOCFN(elf_deftlsallocfn);
elAPI ALLOCFN(elf_defglobalallocfn);


/* todo: better names */
#define elTLS_ALLOCATOR (&elf_tlsalloc)
#define elHEAP_ALLOCATOR (&langM_globalalloc)
