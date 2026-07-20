//
// See Copyright Notice In elf.h
//

#ifndef ELF_PLATFORM_TYPES_H
#define ELF_PLATFORM_TYPES_H

#include "base.h"

typedef elf_u64 elf_PlatformFile;
typedef elf_u64 elf_PlatformProcess;

#define ELF_HINVALID              (0)
#define ELF_IS_HANDLE_INVALID(H) ((H) == ELF_HINVALID)

#endif
