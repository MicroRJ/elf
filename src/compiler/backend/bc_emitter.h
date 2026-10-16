//
// See Copyright Notice In elf.h
//

typedef struct
{
	u32 bytecode_index;
	u32 global_index;
}
BcGlobalRelocation;

typedef struct
{
	Bytecode           *bytecode;
	u32                 bytecode_count;
	SourceMapEntry     *source_map;
	u32                 source_map_count;
	BcFunction         *functions;
	u32                 function_count;
	Atom              **atoms;
	u32                 atom_count;
	i64                *integer_constants;
	u32                 integer_constant_count;
	f64                *number_constants;
	u32                 number_constant_count;
	u32                *global_names;
	u32                 global_count;
	BcGlobalRelocation *global_relocations;
	u32                 global_relocation_count;
}
BcModule;

BcModule *bc_emit_module(IrModule *input);
