#include <stdio.h>

#include "elf.h"
#include "base.h"
#include "platform.h"
#include "core.h"
#include "compiler.h"
#include "bytecode_debug.h"

static b32 read_source_file(elf_State *state, const char *path, elf_StrSlice *source)
{
	Platform_File_Info info;
	if (!platform_get_file_info(path, &info) || info.is_directory) return 0;
	Platform_File file = platform_access_file(path, PLATFORM_FILE_OPEN_EXISTING, PLATFORM_FILE_READ | PLATFORM_FILE_SHARE_READ);
	if (!platform_file_is_valid(file)) return 0;
	u64 size = info.size;
	char *data = elf_arena_push(&state->arena, size + 16);
	zero_memory(data + size, 16);
	U64 read = 0;
	B32 success = platform_read_file(file, data, size, &read);
	platform_close_file(file);
	if (!success || read != size) return 0;

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
