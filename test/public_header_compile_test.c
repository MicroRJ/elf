#include <elf.h>

_Static_assert(sizeof(elf_Size) == sizeof(size_t), "elf_Size must match size_t");
