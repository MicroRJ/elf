
typedef struct
{
	int jumps[256];
	u32   count;
}
JumpList;

typedef struct
{
	JumpList t;
	JumpList f;
}
jumpS;

typedef struct JBuf
{
	JumpList jz;
	JumpList j;
}
JBuf;

typedef struct
{
	b32      defined;
	i32      position;
	JumpList pending;
}
BytecodeLabel;

typedef struct
{
	Bytecode *bytecode;
	u32       capacity;
	u32       position;
}
BytecodeBuffer;

typedef struct
{
	SourceMapEntry *entries;
	u32             capacity;
	u32             count;
}
SourceMapBuffer;

typedef struct
{
	Arena      *arena;
	elf_State      *state;
	BytecodeBuffer  bytecode_buffer;
	SourceMapBuffer source_map_buffer;
	u32             bytecode_function_base;
	GenMemory       memory_usage;
	GenMemory       memory;
	IR           memory_slots[256];
	BytecodeLabel   labels[1024];
}
BytecodeGen;
