//
// See Copyright Notice In elf.h
//
#ifndef ELF_OS_SERVICES_H
#define ELF_OS_SERVICES_H

#include "elf_os.h"

elf_u64 elf_os_write_console(elf_OS_StandardStream stream, const void *data, elf_u64 size);
void elf_os_sleep(elf_u64 milliseconds);

#endif
