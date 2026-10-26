#include <stdio.h>
#include <string.h>

#include "elf.h"
#include "elf_runtime.h"
#include "elf_runtime_helpers.h"
#include "elf_value_text.h"

int main(int argc, char **argv)
{
	const char *path = "main.elf";
	if (argc == 2 && strcmp(argv[1], "--version") == 0)
	{
		printf("elf %s\n", elf_version());
		return 0;
	}
	if (argc > 2)
	{
		fprintf(stderr, "usage: elf.exe [file | --version]\n");
		return 2;
	}

	if (argc == 2) {
		path = argv[1];
	}

	elf_State *state = elf_create_state();
	elf_StrSlice source_path = {(char *)path, strlen(path)};
	if (!elf_push_code_file(state, 0, source_path))
	{
		fprintf(stderr, "elf: could not load '%s'\n", path);
		elf_destroy_state(state);
		return 1;
	}
	elf_push_nil(state);
	u32 nrets = elf_call(state, 1);
	if (nrets != 0) {
		printf("elf returned %u result(s):\n", nrets);

		elf_Value *results = state->stack_top - nrets;
		for (u32 i = 0; i < nrets; ++ i) {

			Scratch scratch = scratch_begin();
			char *begin = arena_push(scratch.arena, 0, 1);
			elf_print_value(scratch.arena, results[i]);
			arena_push_zero(scratch.arena, 1, 1);
			scratch_end(scratch);

			printf("  [%u] = %s: %s\n", i, value_type_name(results[i].type), begin);
		}
	}
	elf_destroy_state(state);

	return 0;
}
