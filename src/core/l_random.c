//
// See Copyright Notice In elf.h
//

// todo: how cute
static elf_u32 global_random_state = 305419896;

static elf_u32 xorshift32(elf_u32 x) {
	x = x ^ x << 13;
	x = x ^ x >> 7;
	x = x ^ x << 17;
	return x;
}

ELF_FUNCTION(l_rand_seed) {
	global_random_state = f_checkint(S, 0);
	return 1;
}

ELF_FUNCTION(l_rand_random) {
	global_random_state = xorshift32(global_random_state);
	elf_pushnum(S, global_random_state / (double) UINT_MAX);
	return 1;
}

static elf_Binding lib_random[] = {
	{"random_seed", l_rand_seed  },
	{"random", l_rand_random }
};



