#include <stdio.h>
#include <string.h>

#include "elf.h"
#include "elf_batteries.h"

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
	elf_open_batteries(state);
	if (!elf_push_code_file(state, path))
	{
		fprintf(stderr, "elf: could not load '%s'\n", path);
		elf_destroy_state(state);
		return 1;
	}
	elf_push_nil(state);
	elf_call(state, 1, 0);
	elf_destroy_state(state);

	return 0;
}
