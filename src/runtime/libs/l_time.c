//
// Timing and sleeping.
//

ELF_FUNCTION(lib_time_counter)
{
	push_value(S, value_from_integer(elf_platform_counter()));
	return 1;
}

ELF_FUNCTION(lib_time_frequency)
{
	push_value(S, value_from_integer(elf_platform_counter_frequency()));
	return 1;
}

ELF_FUNCTION(lib_time_elapsed)
{
	i64 start = lib_load_integer(S, 1);
	f64 elapsed = (elf_platform_counter() - start) /
		(f64)elf_platform_counter_frequency();
	push_value(S, value_from_number(elapsed));
	return 1;
}

ELF_FUNCTION(lib_time_sleep)
{
	elf_platform_sleep(lib_load_integer(S, 1));
	return 0;
}

static const elf_Binding l_time[] = {
	{"counter",   lib_time_counter},
	{"frequency", lib_time_frequency},
	{"elapsed",   lib_time_elapsed},
	{"sleep",     lib_time_sleep},
};

static elf_Table *elf_lib_time(elf_State *state)
{
	return new_binding_table(state, l_time, ARRAY_COUNT(l_time));
}
