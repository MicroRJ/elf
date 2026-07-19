
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
	elf_State      *state;
	elf_Arena          *arena;
	Bytecode       *bytecode;
	u32             bytecode_capacity;
	u32             bytecode_count;
	SourceMapBuffer source_map_buffer;
	u32             bytecode_function_base;
	u32             stack_size;
	u32             stack_top;
	BytecodeLabel   labels[4096];
	u32             label_count;
	BytecodeJumpPatch jump_patches[4096];
	u32               jump_patch_count;
}
BcGen;
