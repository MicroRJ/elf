//
// See Copyright Notice In elf.h
//

#ifndef ELF_BC_MODULE_H
#define ELF_BC_MODULE_H

#include "bytecode.h"

typedef struct
{
	const char *data;
	u32         size;
}
BcString;

typedef struct BcModule BcModule;
struct BcModule
{
	// Codex: remove these, now these are stored in BcFunction
	BcString           *strings;
	u32                 string_count;
	i64                *integer_constants;
	u32                 integer_constant_count;
	f64                *number_constants;
	u32                 number_constant_count;
	Bytecode           *bytecode;
	u32                 bytecode_count;
	BcSourceMapEntry   *source_map;
	u32                 source_map_count;

	// Codex: switch this to BcFunction
	elf_Prototype      *functions;
	u32                 function_count;
};

#endif
