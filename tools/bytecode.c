#include <stdio.h>

#include "elf.h"
#include <dy.h>
#include "base.h"
#include "core.h"
#include "compiler.h"
#include "bytecode_debug.h"

static b32 read_source_file(elf_State *state, const char *path, elf_StrSlice *source)
{
	dy_File_Info info;
	if (dy_get_file_info(dy_string_from_cstring(path), &info).error || info.is_directory) return 0;
	dy_File file;
	if (dy_access_file(dy_string_from_cstring(path), DY_FILE_OPEN_EXISTING,
		DY_FILE_READ | DY_FILE_SHARE_READ, &file).error) return 0;
	u64 size = info.size;
	char *data = elf_arena_push(&state->arena, size + 16);
	zero_memory(data + size, 16);
	dy_u64 read = 0;
	dy_Result read_result = dy_read_file(file, data, size, &read);
	dy_close_file(file);
	if (read_result.error || read != size) return 0;

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
		elf_destroy_state(state);
		return 1;
	}

	BcFunctionRef entry = elf_compile_source(state, path, source, 0);
	elf_Module *module = entry.module;
	for (u32 i = 0; i < module->bytecode_function_count; ++ i)
	{
		printf("function[%u]\n", i);
		BcFunctionRef function = {
			.module = module,
			.index  = i,
		};
		print_bytecode_function(state, function);
	}
	elf_destroy_state(state);

	return 0;
}
