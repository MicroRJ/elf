//
// See Copyright Notice In elf.h
//

static elf_i64 prof_get_time() {
	return sys_get_clock_time();
}

static elf_f64 prof_time_diff_s(elf_i64 time) {
	return (sys_get_clock_time() - time) / (elf_f64) sys_get_clock_freq();
}

static elf_f64 prof_time_diff_ms(elf_i64 time) {
	return prof_time_diff_s(time) * 1000.0;
}