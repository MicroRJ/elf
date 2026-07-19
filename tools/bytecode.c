#include <stdio.h>

#include "elf.h"
#include "base.h"
#include "platform.h"
#include "core.h"
#include "compiler.h"
#include "bytecode_debug.h"

static b32 read_source_file(elf_State *state, const char *path, elf_StrSlice *source)
{
	elf_PlatformFile file = elf_platform_open_file(path, ELF_PLATFORM_OPEN_READ, ELF_PLATFORM_OPEN_EXISTING);
	if (!file) {
		return 0;
	}

	u64 size = elf_platform_file_size(file);
	char *data = elf_arena_push(&state->arena, size + 16);
	zero_memory(data + size, 16);
	elf_platform_read_file(file, data, (u32)size);
	elf_platform_close_file(file);

	source->data = data;
	source->size = size;
	return 1;
}

int main(int argc, char **argv)
{
	const char *path = "main.elf";
	if (argc > 2)
	{
		fprintf(stderr, "usage: bytecode.exe [file]\n");
		return 2;
	}

	if (argc == 2) {
		path = argv[1];
	}

	elf_State *state = elf_create_state();
	elf_StrSlice source = {};
	if (!read_source_file(state, path, &source))
	{
		fprintf(stderr, "bytecode: could not load '%s'\n", path);
		return 1;
	}

	elf_compile_source(state, path, source);
	for (u32 i = 0; i < state->bytecode_function_count; ++ i)
	{
		printf("function[%u]\n", i);
		print_bytecode_function(state, state->bytecode_functions[i]);
	}

	return 0;
}
