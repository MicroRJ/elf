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

typedef struct BcGen BcGen;

// Accumulates module-wide bytecode, constants, strings, and function records.
typedef struct
{
	Compiler   *compiler;
	elf_Arena *arena;
	Bytecode   *bytecode;
	u32         bytecode_capacity;
	u32         bytecode_count;
	BcFunction *functions;
	u32         function_count;
	u32         generated_function_count;
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

// Temporary state for the one IR function currently being generated.
struct BcGen
{
	BcModuleBuilder  *module;
	SourceMapBuffer   source_map_buffer;
	u32               stack_size;
	u32               stack_top;
	BytecodeLabel     labels[4096];
	u32               label_count;
	BytecodeJumpPatch jump_patches[4096];
	u32               jump_patch_count;
};
