#include <stdio.h>
#include <string.h>

#include "elf.h"
#include <dy.h>
#include "base.h"
#include "elf_core.h"
#include "compiler.h"
#include "bytecode.h"
#include "linker.h"

static b32 read_source_file(elf_State *state, const char *path, elf_StrSlice *source)
{
	dy_File_Info info;
	if (dy_get_file_info(dy_string_from_cstring(path), &info).error || info.is_directory) return 0;
	dy_File file;
	if (dy_access_file(dy_string_from_cstring(path), DY_FILE_OPEN_EXISTING,
		DY_FILE_READ | DY_FILE_SHARE_READ, &file).error) return 0;
	u64 size = info.size;
	char *data = arena_push(&state->arena, size + 16, 1);
	zero_memory(data + size, 16);
	dy_u64 read = 0;
	dy_Result read_result = dy_read_file(file, data, size, &read);
	dy_close_file(file);
	if (read_result.error || read != size) return 0;

	source->data = data;
	source->size = size;
	return 1;
}

static elf_PrototypeRef compile_source(elf_State *state, const char *name, elf_StrSlice source)
{
	Scratch scratch = scratch_begin();
	elf_StrSlice source_name = {(char *)name, strlen(name)};
	BcModule *compiled_module = compiler_compile(state, scratch.arena, source_name, source);
	elf_Program *module = elf_link_module(state, compiled_module);
	elf_PrototypeRef entry = {};
	if (module)
	{
		entry.program = module;
		entry.index = 0;
	}
	scratch_end(scratch);
	return entry;
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

	elf_PrototypeRef entry = compile_source(state, path, source);
	elf_Program *module = entry.program;
	for (u32 i = 0; i < module->prototype_count; ++ i)
	{
		printf("function[%u]\n", i);
		elf_PrototypeRef function = {
			.program = module,
			.index  = i,
		};
		print_bytecode_function(state, function);
	}
	elf_destroy_state(state);

	return 0;
}
