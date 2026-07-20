static ELF_FUNCTION(test_stack_api_callback)
{
	elf_StrSlice receiver = {};
	elf_Integer argument = 0;

	if (elf_stack_arg_count(S) != 2) {
		test_fail("stack API callback argument count includes this");
	}
	if (!elf_stack_to_str(S, 0, &receiver)
	|| receiver.size != 4 || memcmp(receiver.data, "this", 4) != 0) {
		test_fail("stack API argument zero is this");
	}
	if (!elf_stack_to_int(S, 1, &argument) || argument != 41) {
		test_fail("stack API argument one is first explicit argument");
	}

	elf_push_int(S, argument + 1);
	return 1;
}

static void test_stack_api_call_addressing(elf_State *state)
{
	elf_i32 checkpoint = elf_stack_get_top(state);
	elf_push_fun(state, test_stack_api_callback);
	elf_push_cstr(state, "this");
	elf_push_int(state, 41);
	elf_call(state, 2, 1);

	elf_Integer result = 0;
	if (!elf_stack_to_int(state, -1, &result) || result != 42) {
		test_fail("stack API native callback result");
	}
	if (!elf_stack_set_top(state, checkpoint)) {
		test_fail("stack API restores host checkpoint");
	}
}

static void test_stack_api_tables_and_refs(elf_State *state)
{
	elf_i32 checkpoint = elf_stack_get_top(state);
	elf_stack_new_table(state);
	elf_i32 table = elf_stack_abs_index(state, -1);

	elf_push_int(state, 42);
	if (!elf_stack_set_field(state, table, "answer")) {
		test_fail("stack API sets a generic field value");
	}
	if (!elf_stack_get_field(state, table, "answer")) {
		test_fail("stack API gets a field");
	}
	elf_Integer answer = 0;
	if (!elf_stack_to_int(state, -1, &answer) || answer != 42) {
		test_fail("stack API field preserves its type");
	}
	elf_stack_pop(state, 1);

	elf_push_cstr(state, "first");
	if (!elf_stack_add(state, table)) {
		test_fail("stack API appends a generic array value");
	}
	if (!elf_stack_get_index(state, table, 2) || !elf_stack_is_nil(state, -1)) {
		test_fail("stack API missing array index produces nil");
	}
	elf_stack_pop(state, 1);

	elf_Ref reference = elf_stack_create_ref(state, table);
	if (reference == ELF_NO_REF) {
		test_fail("stack API creates a state-owned reference");
	}
	elf_stack_set_top(state, checkpoint);
	state->gc_next_cycle_bytes = 1;
	elf_new_table_rogue(state);
	if (!elf_push_ref(state, reference)) {
		test_fail("stack API reference survives leaving the stack");
	}
	if (!elf_stack_get_field(state, -1, "answer")
	|| !elf_stack_to_int(state, -1, &answer) || answer != 42) {
		test_fail("stack API reference restores the original table");
	}
	elf_stack_pop(state, 1);

	elf_u32 cursor = 0;
	elf_u32 entries = 0;
	while (elf_stack_next(state, -1, &cursor)) {
		entries += 1;
		elf_stack_pop(state, 2);
	}
	if (entries != 2) {
		test_fail("stack API iterates array and keyed table entries");
	}

	if (!elf_stack_push_value(state, -1) || !elf_stack_set_global(state, "stack_api_test")) {
		test_fail("stack API writes a global from the stack");
	}
	elf_stack_get_global(state, "stack_api_test");
	if (!elf_stack_equal(state, -1, -2)) {
		test_fail("stack API global preserves table identity");
	}
	elf_stack_pop(state, 1);

	if (!elf_stack_release_ref(state, reference) || elf_push_ref(state, reference)) {
		test_fail("stack API releases state-owned references");
	}
	elf_push_nil(state);
	elf_stack_set_global(state, "stack_api_test");
	elf_stack_set_top(state, checkpoint);
}

static void run_stack_api_tests(void)
{
	elf_State *state = elf_create_state();
	test_stack_api_call_addressing(state);
	test_stack_api_tables_and_refs(state);
	elf_destroy_state(state);
}
