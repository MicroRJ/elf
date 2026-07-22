//
// Optional standard output operations.
//

static int core_print_values(elf_State *state, int nargs, b32 newline)
{
	i64 size = 0;
	elf_PlatformFile output = elf_platform_std_file(ELF_PLATFORM_STD_OUTPUT);
	for (elf_i32 index = 1; index < nargs; ++index)
	{
		if (!elf_push_value_text(state, index)) continue;
		elf_StrSlice text = {0};
		if (elf_to_str(state, -1, &text)) {
			elf_platform_write_file(output, text.data, (i64)text.size);
			size += (i64)text.size;
		}
		elf_pop(state, 1);
	}
	if (newline) {
		elf_platform_write_file(output, "\n", 1);
		size += 1;
	}

	elf_push_int(state, size);
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
