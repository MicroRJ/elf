
static elf_u32 global_random_state = 305419896;

static elf_u32 xorshift32(elf_u32 x) {
	x = x ^ x << 13;
	x = x ^ x >> 7;
	x = x ^ x << 17;
	return x;
}

static int _random(elf_State *S) {
	global_random_state = xorshift32(global_random_state);
	elf_push_num(S, global_random_state / (double) UINT_MAX);
	return 1;
}

static elf_Binding lib_random[] = {
	{"random", _random}
};



