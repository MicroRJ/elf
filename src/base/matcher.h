//
// See Copyright Notice In elf.h
//
#ifndef MATCHER_H
#define MATCHER_H

const char *matcher_match(const char *text, const char *pattern);
b32 matcher_match_sized(const char *text, u32 text_size, const char *pattern, u32 pattern_size);

#endif
