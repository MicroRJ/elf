//
// Optional standard output operations.
//

static int core_print_values(elf_State *state, int nargs, b32 newline)
{
	i64 size = 0;
	for (elf_i32 index = 1; index < nargs; ++index)
	{
		if (!elf_push_value_text(state, index)) continue;
		elf_StrSlice text = {0};
		if (elf_to_str(state, -1, &text)) {
			day_u64 written;
			if (!day_write_console(DAY_STANDARD_OUTPUT, text.data, text.size, &written).error) size += (i64)written;
		}
		elf_pop(state, 1);
	}
	if (newline) {
		day_u64 written;
		if (!day_write_console(DAY_STANDARD_OUTPUT, "\n", 1, &written).error) size += (i64)written;
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
