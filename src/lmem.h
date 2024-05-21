/*
** See Copyright Notice In elf.h
** lmem.h
** Memory Tools
*/


typedef struct Alloc Alloc;


#define FLYTRAP 0x55555555

#define CHUNKSIZE 1024
#define CHUNKCATE(x,y) ((x+y-1)/y*y)




typedef Error (* elf_AllocFn)(Alloc *allocator, int flags, elInteger oldSize, elInteger newSize, void **oldAndNewMemory, ldebugloc loca);


typedef struct Alloc {
	char const *label;
	elf_AllocFn fn;
} Alloc;


#if 0
typedef struct MemBlock MemBlock;
typedef struct MemBlock {
	unsigned int headtrap;
	MemBlock *then;
	ldebugloc loca;
	ldebugloc freeloca;
	elInteger contentssize;
	unsigned int foottrap;
} MemBlock;
#endif


void langM_debugdealloc(void *mem, ldebugloc loca);
void *langM_debugrealloc(void *mem, elInteger contentssize, ldebugloc loca);
void *langM_debugalloc(elInteger contentssize, ldebugloc loca);


elf_api void elf_dealloc_(Alloc *allocator, void const *memory, ldebugloc loca);
elf_api void *elf_realloc_(Alloc *allocator, elInteger size, void *memory, ldebugloc loca);
elf_api void *elf_alloc_(Alloc *allocator, elInteger size, ldebugloc loca);
elf_api void *elf_clearalloc_(Alloc *allocator, elInteger size, ldebugloc loca);


#define elf_delmem(cator,mem) elf_dealloc_(cator,mem,LHERE)
#define langM_realloc(cator,sze,mem) elf_realloc_(cator,sze,mem,LHERE)
#define elf_alloc(cator,sze) elf_alloc_(cator,sze,LHERE)
#define elf_clearalloc(cator,sze) elf_clearalloc_(cator,sze,LHERE)


#define ALLOCFN(NAME) Error NAME (Alloc *allocator, int flags, elInteger oldSize, elInteger newSize, void **oldAndNewMemory, ldebugloc loca)
elf_api ALLOCFN(elf_deftlsallocfn);
elf_api ALLOCFN(elf_defglobalallocfn);


/* todo: better names */
#define lTLOC (&elf_tlsalloc)
#define lHEAP (&langM_globalalloc)
