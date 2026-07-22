//
// See Copyright Notice In elf.h
//
#ifndef ELF_BATTERY_TIME_H
#define ELF_BATTERY_TIME_H

#include "battery_helpers.h"

void elf_platform_sleep(i64 ms);
i64 battery_counter(void);
i64 battery_counter_frequency(void);

#endif
