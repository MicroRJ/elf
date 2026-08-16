//
// Optional script-visible timing and sleeping.
//

ELF_FUNCTION(lib_time_counter)
{
	elf_push_int(S, (elf_Int)platform_counter());
	return 1;
}

ELF_FUNCTION(lib_time_frequency)
{
	elf_push_int(S, (elf_Int)platform_counter_frequency());
	return 1;
}

ELF_FUNCTION(lib_time_elapsed)
{
	i64 start = lib_load_integer(S, 1);
	f64 elapsed = (platform_counter() - start) / (f64)platform_counter_frequency();
	elf_push_num(S, elapsed);
	return 1;
}

ELF_FUNCTION(lib_time_sleep)
{
	platform_sleep((U64)lib_load_integer(S, 1));
	return 0;
}

static const Battery_Binding l_time[] = {
	{"counter",   lib_time_counter},
	{"frequency", lib_time_frequency},
	{"elapsed",   lib_time_elapsed},
	{"sleep",     lib_time_sleep},
};

static void elf_lib_time(elf_State *state)
{
	new_binding_table(state, l_time, battery_array_count(sizeof(l_time), sizeof(l_time[0])));
}
