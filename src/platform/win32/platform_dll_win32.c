//
// See Copyright Notice In elf.h
//

elf_PlatformFile elf_platform_load_dll(const char *name)
{
	return (elf_PlatformFile)LoadLibraryA(name);
}

void *elf_platform_dll_symbol(elf_PlatformFile dll, const char *name)
{
	return (void *)GetProcAddress((HMODULE)dll, name);
}
