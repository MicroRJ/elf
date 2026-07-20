//
// Minimal portable fallback platform. Optional host services live in batteries.
//

#include <stdlib.h>
#include <time.h>

b32 elf_platform_debug_break(void)
{
	return false;
}

void elf_platform_enable_console_colors(void)
{
}

void elf_platform_exit_process(int errorcode)
{
	exit(errorcode);
}

void *elf_platform_virtual_alloc(i64 size)
{
	return calloc(1, (size_t)size);
}

void elf_platform_virtual_free(void *memory)
{
	free(memory);
}

i64 elf_platform_counter_frequency(void)
{
	return CLOCKS_PER_SEC;
}

i64 elf_platform_counter(void)
{
	return (i64)clock();
}
