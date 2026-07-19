//
// See Copyright Notice In elf.h
//
#ifndef ELF_GLOB_MATCHER_H
#define ELF_GLOB_MATCHER_H

const char *elf_glob_match(const char *text, const char *pattern);
b32 elf_glob_match_sized(const char *text, u32 text_size, const char *pattern, u32 pattern_size);

#endif
