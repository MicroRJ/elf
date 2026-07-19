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

typedef struct
{
	u16 year;
	u16 month;
	u16 dayofweek;
	u16 day;
	u16 hour;
	u16 minute;
	u16 second;
	u16 milliseconds;
}
elf_PlatformSystemTime;

typedef union
{
	struct { u32 low, high; };
	u64 time;
}
elf_PlatformFileTime;

#endif
