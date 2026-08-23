//
// See Copyright Notice In elf.h
//

typedef struct
{
	b32 defined;
	i32 position;
}
BytecodeLabel;

typedef struct
{
	u32 bytecode_position;
	u32 label;
}
BytecodeJumpPatch;

typedef struct
{
	SourceMapEntry *entries;
	u32             capacity;
	u32             count;
}
SourceMapBuffer;

typedef struct
{
	Bytecode   *bytecode;
	u32         bytecode_capacity;
	u32         bytecode_count;
	BcFunction *functions;
	u32         function_count;
	Atom   **atoms;
	u32          atom_capacity;
	u32          atom_count;
	i64        *integer_constants;
	u32         integer_constant_capacity;
	u32         integer_constant_count;
	f64        *number_constants;
	u32         number_constant_capacity;
	u32         number_constant_count;
}
BcModuleBuilder;

typedef struct
{
	elf_Arena        *arena;
	BcModuleBuilder   module;
	Bytecode         *bytecode;
	u32               bytecode_capacity;
	u32               bytecode_count;
	SourceMapBuffer   source_map_buffer;
	u32               stack_size;
	u32               stack_top;
	// TODO(RJ) fixed arrays!
	BytecodeLabel     labels[4096];
	u32               label_count;
	// TODO(RJ) fixed arrays!
	BytecodeJumpPatch jump_patches[4096];
	u32               jump_patch_count;
}
BcGen;
