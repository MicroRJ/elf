#include <stdio.h>

#include "elf.h"

int main(int argc, char **argv)
{
	const char *path = "main.elf";
	if (argc > 2)
	{
		fprintf(stderr, "usage: elf.exe [file]\n");
		return 2;
	}

	if (argc == 2) {
		path = argv[1];
	}

	elf_State *state = elf_create_state();
	if (!elf_push_code_file(state, path))
	{
		fprintf(stderr, "elf: could not load '%s'\n", path);
		return 1;
	}
	elf_push_nil(state);
	elf_call(state, 1, 0);

	return 0;
}
