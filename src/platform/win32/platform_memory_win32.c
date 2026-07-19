//
// See Copyright Notice In elf.h
//

void *elf_platform_virtual_alloc(i64 size)
{
	return VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

void elf_platform_virtual_free(void *memory)
{
	VirtualFree(memory, 0, MEM_RELEASE);
}
