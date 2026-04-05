//
// See Copyright Notice In elf.h
//


//
// this is what we're cooking with people...
//




// todo:
static u32 global_random_state = 305419896;






static u32 xorshift32(u32 x) {
	x = x ^ x << 13;
	x = x ^ x >> 7;
	x = x ^ x << 17;
	return x;
}






ELF_FUNCTION(l_rand_seed) {
	global_random_state = loadint(S, 1);
	return 1;
}





ELF_FUNCTION(l_rand_random) {
	global_random_state = xorshift32(global_random_state);
	elf_push_num(S, global_random_state / (double) UINT_MAX);
	return 1;
}





static elf_Binding lib_random[] = {
	{"random_seed", l_rand_seed  },
	{"random", l_rand_random }
};



