//
// Optional standard output operations.
//

static int core_print_values(elf_State *state, int nargs, b32 newline)
{
	elf_Scratch scratch = elf_begin_scratch();
	char *start = elf_arena_push(scratch.arena, 0);
	for (i64 index = 1; index < nargs; ++index)
	{
		elf_print_value(scratch.arena, load_value(state, index));
	}
	if (newline) elf_arena_push_char(scratch.arena, '\n');
	char *end = elf_arena_push_zero(scratch.arena, 1);
	u32 size = (u32)(end - start);

	elf_PlatformFile output = elf_platform_std_file(ELF_PLATFORM_STD_OUTPUT);
	elf_platform_write_file(output, start, size);
	elf_end_scratch(scratch);

	push_value(state, value_from_integer(size));
	return 1;
}

ELF_FUNCTION(l_core_print)
{
	return core_print_values(S, nargs, false);
}

ELF_FUNCTION(l_core_println)
{
	return core_print_values(S, nargs, true);
}
