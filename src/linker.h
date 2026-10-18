//
// See Copyright Notice In elf.h
//

#ifndef ELF_LINKER_H
#define ELF_LINKER_H

typedef struct elf_State elf_State;
typedef struct elf_Program elf_Program;
typedef struct BcModule BcModule;

elf_Program *elf_link_module(elf_State *state, const BcModule *compiled_module);

#endif
