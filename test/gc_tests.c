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

static void test_gc_tracks_owned_storage(void)
{
	elf_State *state = elf_create_state();
	u64 starting_allocated = state->gc_allocated_bytes;
	u64 starting_live = state->gc_live_bytes;

	elf_Table *table = push_new_table(state);
	u64 table_bytes = sizeof(*table) +
		(u64)table->nentries * sizeof(*table->entries);
	if (state->gc_allocated_bytes - starting_allocated != table_bytes ||
		state->gc_live_bytes - starting_live != table_bytes) {
		test_fail("GC bytes include table object and hash storage");
	}

	u64 before_array = state->gc_live_bytes;
	elf_array_add(state, table, test_value_int(1));
	u64 array_bytes = (u64)table->capacity * sizeof(*table->array);
	if (state->gc_live_bytes - before_array != array_bytes) {
		test_fail("GC bytes include table array storage");
	}

	u64 before_buffer = state->gc_live_bytes;
	elf_ByteBuffer *buffer = elf_buffer_new_object(state, 13);
	push_value(state, value_from_buffer(buffer));
	if (state->gc_live_bytes - before_buffer != sizeof(*buffer) + 13) {
		test_fail("GC bytes include buffer payload storage");
	}

	u64 before_growth = state->gc_live_bytes;
	elf_buffer_reserve_object(state, buffer, 40);
	if (state->gc_live_bytes - before_growth != buffer->capacity - 13) {
		test_fail("GC bytes include buffer capacity growth");
	}

	if (state->gc_allocated_bytes - state->gc_freed_bytes != state->gc_live_bytes) {
		test_fail("GC allocated and freed bytes balance to live bytes");
	}
	if (state->gc_peak_live_bytes < state->gc_live_bytes) {
		test_fail("GC peak live bytes covers current live bytes");
	}

	u64 collections = state->gc_collection_count;
	state->gc_next_cycle_bytes = 1;
	elf_new_table_rogue(state);
	if (state->gc_collection_count != collections + 1 ||
		state->gc_last_collection_time_ns > state->gc_collection_time_ns ||
		state->gc_max_collection_time_ns < state->gc_last_collection_time_ns) {
		test_fail("GC collection timing counters remain consistent");
	}

	elf_destroy_state(state);
}

static void test_gc_sweeps_unreachable_tables(void)
{
	elf_State *state = elf_create_state();
	u32 baseline_count = state->gc_reference_count;

	force_gc_allocations(state, 256);

	expect_gc_reference_count_below(state, baseline_count + 8, "GC sweeps unreachable table allocations");
	elf_destroy_state(state);
}

static void test_gc_keeps_stack_rooted_table_graph(void)
{
	elf_State *state = elf_create_state();
	elf_Value *stack_checkpoint = state->stack_ptr;

	elf_Table *root = push_new_table(state);
	elf_Value child_key = test_key_string(state, "child");
	elf_Value value_key = test_key_string(state, "value");

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
	elf_destroy_state(state);
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
		elf_destroy_state(state);
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
	elf_destroy_state(state);
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
	elf_destroy_state(state);
}

static void test_gc_keeps_referenced_error_info(void)
{
	elf_State *state = elf_create_state();
	elf_Index checkpoint = elf_get_top(state);
	u32 baseline_count = state->gc_reference_count;
	elf_push_error(state, "retained error");
	elf_Ref reference = elf_create_ref(state, -1);
	elf_pop(state, 1);

	force_gc_allocations(state, 256);
	elf_StrSlice message = {};
	if (!elf_push_ref(state, reference)
	|| !elf_is_error(state, -1)
	|| !elf_get_field(state, -1, "message")
	|| !elf_to_str(state, -1, &message)
	|| message.size != 14
	|| memcmp(message.data, "retained error", 14) != 0) {
		test_fail("error reference preserves its backing table through GC");
	}

	elf_set_top(state, checkpoint);
	elf_release_ref(state, reference);
	force_gc_allocations(state, 256);
	expect_gc_reference_count_below(state, baseline_count + 8,
		"released error backing table becomes collectible");
	elf_destroy_state(state);
}

static void test_gc_keeps_referenced_buffer(void)
{
	elf_State *state = elf_create_state();
	elf_Index checkpoint = elf_get_top(state);
	u32 baseline_count = state->gc_reference_count;
	elf_ByteBuffer *buffer = elf_buffer_new_object(state, 8);
	elf_buffer_append_object(state, buffer, "bytes", 5);
	push_value(state, value_from_buffer(buffer));
	elf_Ref reference = elf_create_ref(state, -1);
	elf_pop(state, 1);

	force_gc_allocations(state, 256);
	if (!elf_push_ref(state, reference) || !elf_is_buffer(state, -1)) {
		test_fail("buffer reference preserves its allocation through GC");
	}
	else
	{
		elf_ByteBuffer *retained = value_as_buffer(state->stack_ptr[-1]);
		if (retained->size != 5 || memcmp(retained->data, "bytes", 5) != 0) {
			test_fail("buffer retains its bytes through GC");
		}
	}

	elf_set_top(state, checkpoint);
	elf_release_ref(state, reference);
	force_gc_allocations(state, 256);
	expect_gc_reference_count_below(state, baseline_count + 8,
		"released buffer becomes collectible");
	elf_destroy_state(state);
}

static void test_state_create_destroy_repeatedly(void)
{
	for (u32 i = 0; i < 64; ++i)
	{
		elf_State *state = elf_create_state();
		elf_new_table(state);
		elf_push_cstr(state, "value");
		elf_set_field(state, -2, "key");

		char source_text[] = "ret {answer = 42, text = \"elf\"}";
		elf_StrSlice source = {source_text, sizeof(source_text) - 1};
		if (elf_push_code_source(state, "lifecycle", source, 0) != ELF_ERROR_NONE) {
			test_fail("state lifecycle source compiles");
		}

		elf_destroy_state(state);
	}
}

static void run_gc_tests(void)
{
	test_gc_tracks_owned_storage();
	test_gc_sweeps_unreachable_tables();
	test_gc_keeps_stack_rooted_table_graph();
	test_gc_keeps_referenced_table_graph();
	test_gc_keeps_referenced_string();
	test_gc_keeps_referenced_error_info();
	test_gc_keeps_referenced_buffer();
	test_state_create_destroy_repeatedly();
}
