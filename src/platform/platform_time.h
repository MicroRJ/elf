//
// See Copyright Notice In elf.h
//

#ifndef ELF_PLATFORM_TIME_H
#define ELF_PLATFORM_TIME_H

#include "platform_types.h"

void elf_platform_sleep(i64 ms);
i64 elf_platform_counter_frequency(void);
i64 elf_platform_counter(void);

#endif
