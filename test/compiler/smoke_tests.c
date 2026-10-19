static void test_smoke_script(const char *path)
{
	elf_State *state = elf_create_state();
	elf_push_code_file(state, path);
	elf_push_nil(state);
	elf_call(state, 1, 0);
	elf_destroy_state(state);
}

static void run_smoke_tests(void)
{
	test_smoke_script("test/compiler/smoke/basic.elf");
	test_smoke_script("test/compiler/smoke/return_int.elf");
	test_smoke_script("test/compiler/smoke/return_nil.elf");
	test_smoke_script("test/compiler/smoke/return_table.elf");
	test_smoke_script("test/compiler/smoke/nil_assign.elf");
}
