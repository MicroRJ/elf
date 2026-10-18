
#ifndef ELF_TARGET_H
#define ELF_TARGET_H

#include "elf.h"
#include "bytecode.h"

typedef struct
{
	const char *data;
	u32         size;
}
BcString;

typedef struct BcFunction BcFunction;
struct BcFunction
{
	b32               variadic;
	u8                 arity;
	u8                 captures;
	u16                stack_size;
	BcString          *strings;
	u32                string_count;
	i64               *integer_constants;
	u32                integer_constant_count;
	f64               *number_constants;
	u32                number_constant_count;
	Bytecode          *bytecode;
	u32                bytecode_count;
	BcSourceMapEntry  *source_map;
	u32                source_map_count;
};

#define ELF_PROTOTYPE_INVALID ((elf_u32)-1)

elf_u32 elf_ensure_global_symbol(elf_State *state, elf_StrSlice name);
elf_u32 elf_reserve_prototype(elf_State *state);
elf_Bool elf_init_prototype(elf_State *state, elf_u32 index, const BcFunction *function);

#endif
