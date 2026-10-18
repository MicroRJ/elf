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

typedef struct
{
	BcString name;
	// NOTE(RJ): after canonicalization!
	BcString path;
	BcSourceBuffer source;
}
BcSource;

typedef struct
{
	u32 bytecode_index;
	u32 name_index;
}
BcGlobalRelocation;

typedef struct BcModule BcModule;
struct BcModule
{
	BcSource           *sources;
	u32                 source_count;
	u32                 root_source;

	BcString           *strings;
	u32                 string_count;

	BcGlobalRelocation *global_relocations;
	u32                 global_relocation_count;

	i64                *integer_constants;
	u32                 integer_constant_count;
	f64                *number_constants;
	u32                 number_constant_count;

	elf_Prototype         *functions;
	u32                 function_count;
	Bytecode           *bytecode;
	u32                 bytecode_count;
	BcSourceMapEntry   *source_map;
	u32                 source_map_count;
};

#endif
