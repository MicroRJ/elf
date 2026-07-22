static void force_gc_allocations(elf_State *state, u32 count)
{
	for (u32 i = 0; i < count; ++i) {
		state->gc_next_cycle_bytes = 1;
		elf_new_table_rogue(state);
	}
}

static void expect_gc_reference_count_below(elf_State *state, u32 max_count, const char *label)
{
	if (state->gc_reference_count > max_count) {
		fprintf(stderr, "FAIL: %s expected GC reference count <= %u, got %u\n",
			label, max_count, state->gc_reference_count);
		test_failures += 1;
	}
}

static void test_gc_sweeps_unreachable_tables(void)
{
	elf_State *state = elf_create_state();
	u32 baseline_count = state->gc_reference_count;

	force_gc_allocations(state, 256);

	expect_gc_reference_count_below(state, baseline_count + 8, "GC sweeps unreachable table allocations");
}

static void test_gc_keeps_stack_rooted_table_graph(void)
{
	elf_State *state = elf_create_state();
	elf_Value *stack_checkpoint = state->stack_ptr;

	elf_Table *root = push_new_table(state);
	elf_Value child_key = test_key_atom(state, "child");
	elf_Value value_key = test_key_atom(state, "value");

	for (u32 i = 0; i < 128; ++i) {
		elf_Table *child = push_new_table(state);
		elf_Value child_value = {};
		child_value = value_from_table(child);

		elf_table_set(state, child, value_key, test_value_int(1000 + i));
		elf_array_add(state, root, child_value);
		elf_table_set(state, root, test_key_int(i), child_value);
	}

	force_gc_allocations(state, 512);

	for (u32 i = 0; i < 128; ++i) {
		elf_Value child_value = elf_table_get_or_nil(state, root, test_key_int(i));
		if (child_value.type != ELF_VALUE_TYPE_TABLE) {
			test_fail("GC preserves rooted child table type");
			continue;
		}

		elf_Table *child = value_as_table(child_value);
		expect_int(elf_table_get_or_nil(state, child, value_key), 1000 + i, "GC preserves rooted child table contents");
	}

	elf_Value first_child = elf_array_get(state, root, 0);
	elf_table_set(state, root, child_key, first_child);
	expect_table_value(elf_table_get_or_nil(state, root, child_key), "GC preserves rooted table field references");

	state->stack_ptr = stack_checkpoint;
	force_gc_allocations(state, 256);
}

static void test_gc_keeps_referenced_table_graph(void)
{
	elf_State *state = elf_create_state();
	u32 baseline_count = state->gc_reference_count;
	elf_i32 stack_checkpoint = elf_get_top(state);
	elf_Table *root = push_new_table(state);
	elf_Table *child = push_new_table(state);

	elf_array_add(state, child, test_value_int(42));
	elf_array_add(state, root, value_from_table(child));
	elf_Ref reference = elf_create_ref(state, -2);
	elf_set_top(state, stack_checkpoint);

	force_gc_allocations(state, 256);
	if (!elf_push_ref(state, reference)) {
		test_fail("table reference survives GC");
		return;
	}
	root = value_as_table(state->stack_ptr[-1]);
	elf_Value child_value = elf_array_get(state, root, 0);
	expect_table_value(child_value, "table reference preserves child table");
	expect_int(elf_array_get(state, value_as_table(child_value), 0), 42,
		"table reference preserves child contents");

	elf_pop(state, 1);
	elf_release_ref(state, reference);
	force_gc_allocations(state, 256);
	expect_gc_reference_count_below(state, baseline_count + 8,
		"released table reference becomes collectible");
}

static void test_gc_keeps_referenced_string(void)
{
	elf_State *state = elf_create_state();
	u32 baseline_count = state->gc_reference_count;
	elf_push_cstr(state, "externally rooted string");
	elf_Ref reference = elf_create_ref(state, -1);
	elf_pop(state, 1);

	force_gc_allocations(state, 256);
	elf_StrSlice string = {};
	if (!elf_push_ref(state, reference)
	|| !elf_to_str(state, -1, &string)
	|| string.size != 24
	|| memcmp(string.data, "externally rooted string", 24) != 0) {
		test_fail("string reference survives GC");
	}

	elf_pop(state, 1);
	elf_release_ref(state, reference);
	force_gc_allocations(state, 256);
	expect_gc_reference_count_below(state, baseline_count + 8,
		"released string reference becomes collectible");
}

static void run_gc_tests(void)
{
	test_gc_sweeps_unreachable_tables();
	test_gc_keeps_stack_rooted_table_graph();
	test_gc_keeps_referenced_table_graph();
	test_gc_keeps_referenced_string();
}
