static void test_smoke_script(const char *path)
{
	elf_State *state = elf_create_state();
	elf_open_batteries(state);
	elf_push_code_file(state, path);
	elf_push_nil(state);
	elf_call(state, 1, 0);
}

static void run_smoke_tests(void)
{
	test_smoke_script("smoke/basic.elf");
	test_smoke_script("smoke/return_int.elf");
	test_smoke_script("smoke/return_nil.elf");
	test_smoke_script("smoke/return_table.elf");
	test_smoke_script("smoke/nil_assign.elf");
}
