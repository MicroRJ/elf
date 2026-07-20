//
// Pseudorandom numbers.
//

static u32 random_state = 305419896;

static u32 random_xorshift32(u32 value)
{
	value ^= value << 13;
	value ^= value >> 7;
	value ^= value << 17;
	return value;
}

ELF_FUNCTION(lib_random_seed)
{
	lib_check_arg_count(S, "random.random_seed", nargs, 1, 1);
	random_state = (u32)lib_load_integer(S, 1);
	return 0;
}

ELF_FUNCTION(lib_random_random)
{
	lib_check_arg_count(S, "random.random", nargs, 0, 0);
	random_state = random_xorshift32(random_state);
	elf_push_num(S, random_state / (double)UINT_MAX);
	return 1;
}

static const elf_Binding l_random[] = {
	{"random_seed", lib_random_seed},
	{"random",      lib_random_random},
};

static elf_Table *elf_lib_random(elf_State *state)
{
	return new_binding_table(state, l_random, ARRAY_COUNT(l_random));
}
