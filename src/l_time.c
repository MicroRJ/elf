//
// See Copyright Notice In elf.h
//

ELF_FUNCTION(lib_time_counter)
{
	elf_push_int(S, (elf_Int)dy_counter());
	return 1;
}

ELF_FUNCTION(lib_time_frequency)
{
	elf_push_int(S, (elf_Int)dy_counter_frequency());
	return 1;
}

ELF_FUNCTION(lib_time_elapsed)
{
	i64 start = lib_load_integer(S, 1);
	f64 elapsed = (dy_counter() - start) / (f64)dy_counter_frequency();
	elf_push_num(S, elapsed);
	return 1;
}

ELF_FUNCTION(lib_time_unix_ms)
{
	elf_push_int(S, (elf_Int)dy_unix_time_ms());
	return 1;
}

ELF_FUNCTION(lib_time_sleep)
{
	dy_sleep((dy_u64)lib_load_integer(S, 1));
	return 0;
}

static const elf_Binding l_time[] = {
	{"counter",   lib_time_counter},
	{"frequency", lib_time_frequency},
	{"elapsed",   lib_time_elapsed},
	{"unix_ms",   lib_time_unix_ms},
	{"sleep",     lib_time_sleep},
};

static elf_SymTable *elf_lib_time(void)
{
	return new_symbol_table(l_time, ARRAY_COUNT(l_time));
}
