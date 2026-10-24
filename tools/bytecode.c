#include <stdio.h>
#include <string.h>

#include "elf.h"
#include <dy.h>
#include "base.h"
#include "elf_core.h"
#include "bytecode.h"

static b32 read_source_file(elf_State *state, const char *path, elf_StrSlice *source)
{
	dy_File_Info info;
	if (dy_file_info(dy_string_from_cstring(path), &info).error || info.is_directory)
	{
		return 0;
	}
	dy_File file;
	if (dy_file_open(dy_string_from_cstring(path), DY_FILE_OPEN_EXISTING, DY_FILE_READ | DY_FILE_SHARE_READ, &file).error)
	{
		return 0;
	}
	u64 size = info.size;
	char *data = arena_push(&state->arena, size + 16, 1);
	zero_memory(data + size, 16);
	dy_u64 read = 0;
	dy_Result read_result = dy_file_read(file, data, size, &read);
	dy_file_close(file);
	if (read_result.error || read != size)
	{
		return 0;
	}

	source->data = data;
	source->size = size;
	return 1;
}

static elf_Prototype *compile_source(elf_State *state, elf_StrSlice name, elf_StrSlice source)
{
	if (elf_push_code_source(state, 0, name, source) != ELF_ERROR_NONE)
	{
		return 0;
	}
	return value_as_closure(state->stack_ptr[-1])->prototype;
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

	u32 first_prototype = state->prototype_count;
	elf_StrSlice source_path = {(char *)path, strlen(path)};
	elf_Prototype *entry = compile_source(state, source_path, source);
	if (!entry)
	{
		elf_destroy_state(state);
		return 1;
	}
	for (u32 i = first_prototype; i < state->prototype_count; ++i)
	{
		printf("function[%u]\n", i);
		print_bytecode_function(state, i);
	}
	elf_destroy_state(state);

	return 0;
}
