
#ifndef ELF_TARGET_H
#define ELF_TARGET_H

#include "elf.h"

// typedef struct BcFunction BcFunction;
// struct BcFunction
// {
// 	u32            source_index;

// 	b32                variadic;
// 	u8                    arity;

// 	u8                 captures;
// 	u16              stack_size;

// 	u16                  length;
// 	u32                  offset;

// 	BcString           *strings;
// 	u32                 string_count;

// 	i64                *integer_constants;
// 	u32                 integer_constant_count;

// 	f64                *number_constants;
// 	u32                 number_constant_count;

// 	Bytecode           *bytecode;
// 	u32                 bytecode_count;

// 	BcSourceMapEntry   *source_map;
// 	u32                 source_map_count;
// };


elf_u32 elf_ensure_global_symbol(elf_State *state, elf_StrSlice name);
elf_u32 elf_reserve_prototype(elf_State *state);

#endif
