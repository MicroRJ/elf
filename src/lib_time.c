
static elf_f64 _time_diff_s(elf_i64 time) {
	return (sys_get_clock_time() - time) / (elf_f64) sys_get_clock_freq();
}

int lib_time__get_clock_time(elf_State *R) {
	elf_add_int(R,sys_get_clock_time());
	return 1;
}

int lib_time__get_time_diff_s(elf_State *S) {
	ASSERT(elf_get_num_args(S) == 1);
	elf_Int time = elf_get_int(S,0);
	elf_push_number(S,_time_diff_s(time));
	return 1;
}

int lib_time__get_time_diff_ms(elf_State *S) {
	ASSERT(elf_get_num_args(S) == 1);
	elf_Int time = elf_get_int(S,0);
	elf_push_number(S,_time_diff_s(time) * 1000);
	return 1;
}


static elf_CBinding _lib_time[] = {
	{"get_clock_time", lib_time__get_clock_time},
	{"get_time_diff_s", lib_time__get_time_diff_s},
	{"get_time_diff_ms", lib_time__get_time_diff_ms},
};