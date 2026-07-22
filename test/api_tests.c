static ELF_FUNCTION(test_api_callback)
{
	elf_StrSlice receiver = {};
	elf_Integer argument = 0;

	if (elf_arg_count(S) != 2) {
		test_fail("API callback argument count includes this");
	}
	if (!elf_to_str(S, 0, &receiver)
	|| receiver.size != 4 || memcmp(receiver.data, "this", 4) != 0) {
		test_fail("API argument zero is this");
	}
	if (!elf_to_int(S, 1, &argument) || argument != 41) {
		test_fail("API argument one is first explicit argument");
	}

	elf_push_int(S, argument + 1);
	return 1;
}

static void test_api_call_addressing(elf_State *state)
{
	elf_i32 checkpoint = elf_get_top(state);
	elf_push_fun(state, test_api_callback);
	elf_push_cstr(state, "this");
	elf_push_int(state, 41);
	elf_call(state, 2, 1);

	elf_Integer result = 0;
	if (!elf_to_int(state, -1, &result) || result != 42) {
		test_fail("API native callback result");
	}
	if (!elf_set_top(state, checkpoint)) {
		test_fail("API restores host checkpoint");
	}
}

static void test_api_tables_and_refs(elf_State *state)
{
	elf_i32 checkpoint = elf_get_top(state);
	elf_new_table(state);
	elf_i32 table = elf_abs_index(state, -1);

	elf_push_int(state, 42);
	if (!elf_set_field(state, table, "answer")) {
		test_fail("API sets a generic field value");
	}
	if (!elf_get_field(state, table, "answer")) {
		test_fail("API gets a field");
	}
	elf_Integer answer = 0;
	if (!elf_to_int(state, -1, &answer) || answer != 42) {
		test_fail("API field preserves its type");
	}
	elf_pop(state, 1);

	elf_push_cstr(state, "first");
	if (!elf_append(state, table)) {
		test_fail("API appends a generic array value");
	}
	if (!elf_get_index(state, table, 2) || !elf_is_nil(state, -1)) {
		test_fail("API missing array index produces nil");
	}
	elf_pop(state, 1);

	elf_Ref reference = elf_create_ref(state, table);
	if (reference == ELF_NO_REF) {
		test_fail("API creates a state-owned reference");
	}
	elf_set_top(state, checkpoint);
	state->gc_next_cycle_bytes = 1;
	elf_new_table_rogue(state);
	if (!elf_push_ref(state, reference)) {
		test_fail("API reference survives leaving the stack");
	}
	if (!elf_get_field(state, -1, "answer")
	|| !elf_to_int(state, -1, &answer) || answer != 42) {
		test_fail("API reference restores the original table");
	}
	elf_pop(state, 1);

	elf_u32 cursor = 0;
	elf_u32 entries = 0;
	while (elf_next(state, -1, &cursor)) {
		entries += 1;
		elf_pop(state, 2);
	}
	if (entries != 2) {
		test_fail("API iterates array and keyed table entries");
	}

	if (!elf_push_value(state, -1) || !elf_set_global(state, "api_test")) {
		test_fail("API writes a global from the stack");
	}
	elf_get_global(state, "api_test");
	if (!elf_equal(state, -1, -2)) {
		test_fail("API global preserves table identity");
	}
	elf_pop(state, 1);

	if (!elf_release_ref(state, reference) || elf_push_ref(state, reference)) {
		test_fail("API releases state-owned references");
	}
	elf_push_nil(state);
	elf_set_global(state, "api_test");
	elf_set_top(state, checkpoint);
}

static void run_api_tests(void)
{
	elf_State *state = elf_create_state();
	test_api_call_addressing(state);
	test_api_tables_and_refs(state);
	elf_destroy_state(state);
}
