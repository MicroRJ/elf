static void force_gc_allocations(elf_State *state, u32 count)
{
	for (u32 i = 0; i < count; ++i) {
		state->gc_next_cycle_bytes = 1;
		elf_table_new(state);
	}
}

static void expect_gc_reference_count_below(elf_State *state, u32 max_count, const char *label)
{
	if (state->gc_reference_count > max_count) {
		TEST_FAIL("%s expected GC reference count <= %u, got %u", label, max_count, state->gc_reference_count);
	}
}

static void test_gc_tracks_owned_storage(void)
{
	elf_State *state = elf_create_state();
	u64 starting_allocated = state->gc_allocated_bytes;
	u64 starting_live = state->gc_live_bytes;

	elf_Table *table = push_new_table(state);
	u64 table_bytes = sizeof(*table) + (u64)table->capacity * sizeof(*table->entries);
	if (state->gc_allocated_bytes - starting_allocated != table_bytes ||
		state->gc_live_bytes - starting_live != table_bytes) {
		test_fail("GC bytes include table object and hash storage");
	}

	u32 initial_capacity = table->capacity;
	u64 before_table_growth = state->gc_live_bytes;
	for (u32 i = 0; i < 4; ++i)
	{
		elf_table_set(state, table, test_key_int(i), test_value_int(i));
	}
	u64 growth_bytes = (u64)(table->capacity - initial_capacity) * sizeof(*table->entries);
	if (state->gc_live_bytes - before_table_growth != growth_bytes) {
		test_fail("GC bytes include table hash growth");
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
	elf_table_new(state);
	if (state->gc_collection_count != collections + 1 ||
		state->gc_last_collection_time_ns > state->gc_collection_time_ns ||
		state->gc_max_collection_time_ns < state->gc_last_collection_time_ns) {
		test_fail("GC collection timing counters remain consistent");
	}

	elf_destroy_state(state);
}

static void test_gc_tracks_array_storage(void)
{
	elf_State *state = elf_create_state();
	u64 starting_live = state->gc_live_bytes;
	elf_Array *array = elf_array_new(state);
	if (state->gc_live_bytes - starting_live != sizeof(*array))
	{
		test_fail("GC bytes include array object storage");
	}

	u64 before_items = state->gc_live_bytes;
	elf_array_add(state, array, test_value_int(1));
	u64 item_bytes = (u64)array->capacity * sizeof(*array->items);
	if (state->gc_live_bytes - before_items != item_bytes)
	{
		test_fail("GC bytes include array item storage");
	}

	elf_destroy_state(state);
}

static void test_gc_keeps_stack_rooted_array_graph(void)
{
	elf_State *state = elf_create_state();
	u32 baseline_count = state->gc_reference_count;
	elf_Value *stack_checkpoint = state->stack_ptr;
	elf_Array *root = elf_array_new(state);
	push_value(state, value_from_array(root));
	elf_Array *child = elf_array_new(state);
	elf_array_add(state, root, value_from_array(child));
	elf_array_add(state, child, value_from_string(elf_string_from_data(state, "array child")));

	force_gc_allocations(state, 256);
	elf_Value child_value = elf_array_get(state, root, 0);
	if (!value_is_array(child_value))
	{
		test_fail("GC preserves rooted child array type");
	}
	else
	{
		elf_Value string_value = elf_array_get(state, value_as_array(child_value), 0);
		if (!value_is_string(string_value) || elf_string_size(value_as_string(string_value)) != 11 || memcmp(elf_string_data(value_as_string(string_value)), "array child", 11) != 0)
		{
			test_fail("GC preserves rooted array contents");
		}
	}

	state->stack_ptr = stack_checkpoint;
	force_gc_allocations(state, 256);
	expect_gc_reference_count_below(state, baseline_count + 8, "released array graph becomes collectible");
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

	elf_Value first_child = elf_table_get_or_nil(state, root, test_key_int(0));
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

	elf_Value child_key = test_key_string(state, "child");
	elf_Value value_key = test_key_string(state, "value");
	elf_table_set(state, child, value_key, test_value_int(42));
	elf_table_set(state, root, child_key, value_from_table(child));
	elf_Ref reference = elf_create_ref(state, -2);
	elf_set_top(state, stack_checkpoint);

	force_gc_allocations(state, 256);
	if (!elf_push_ref(state, reference)) {
		test_fail("table reference survives GC");
		elf_destroy_state(state);
		return;
	}
	root = value_as_table(state->stack_ptr[-1]);
	elf_Value child_value = elf_table_get_or_nil(state, root, child_key);
	expect_table_value(child_value, "table reference preserves child table");
	expect_int(elf_table_get_or_nil(state, value_as_table(child_value), value_key), 42,
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
	elf_Table *data = push_new_table(state);
	elf_Value kind_key = value_from_string(elf_string_from_data(state, "kind"));
	elf_Value kind_value = value_from_string(elf_string_from_data(state, "retained"));
	elf_table_set(state, data, kind_key, kind_value);
	elf_String *message = elf_string_from_data(state, "retained error");
	push_value(state, value_from_string(message));
	elf_GCError *created_error = elf_error_new(state, message, data);
	pop_value(state);
	pop_value(state);
	push_value(state, value_from_error(created_error));
	elf_Ref reference = elf_create_ref(state, -1);
	elf_pop(state, 1);

	force_gc_allocations(state, 256);
	if (!elf_push_ref(state, reference) || !elf_is_error(state, -1))
	{
		test_fail("error reference survives GC");
	}
	else
	{
		elf_GCError *error = value_as_error(state->stack_ptr[-1]);
		if (elf_string_size(error->message) != 14 || memcmp(elf_string_data(error->message), "retained error", 14) != 0)
		{
			test_fail("error reference preserves its message through GC");
		}
		elf_Value kind = elf_table_get_or_nil(state, error->data, kind_key);
		if (!value_is_string(kind) || elf_string_size(value_as_string(kind)) != 8 || memcmp(elf_string_data(value_as_string(kind)), "retained", 8) != 0)
		{
			test_fail("error reference preserves its attached data through GC");
		}
	}

	elf_set_top(state, checkpoint);
	elf_release_ref(state, reference);
	force_gc_allocations(state, 256);
	expect_gc_reference_count_below(state, baseline_count + 8,
		"released error becomes collectible");
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
	if (!elf_push_ref(state, reference) || elf_type(state, -1) != ELF_VALUE_TYPE_BUFFER) {
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
		if (elf_push_code_source(state, 0, ELF_LIT("lifecycle"), source) != ELF_ERROR_NONE) {
			test_fail("state lifecycle source compiles");
		}

		elf_destroy_state(state);
	}
}

static void run_gc_tests(void)
{
	TEST_RUN("gc.tracks_owned_storage", test_gc_tracks_owned_storage);
	TEST_RUN("gc.tracks_array_storage", test_gc_tracks_array_storage);
	TEST_RUN("gc.keeps_stack_rooted_array_graph", test_gc_keeps_stack_rooted_array_graph);
	TEST_RUN("gc.sweeps_unreachable_tables", test_gc_sweeps_unreachable_tables);
	TEST_RUN("gc.keeps_stack_rooted_table_graph", test_gc_keeps_stack_rooted_table_graph);
	TEST_RUN("gc.keeps_referenced_table_graph", test_gc_keeps_referenced_table_graph);
	TEST_RUN("gc.keeps_referenced_string", test_gc_keeps_referenced_string);
	TEST_RUN("gc.keeps_referenced_error_info", test_gc_keeps_referenced_error_info);
	TEST_RUN("gc.keeps_referenced_buffer", test_gc_keeps_referenced_buffer);
	TEST_RUN("gc.state_create_destroy_repeatedly", test_state_create_destroy_repeatedly);
}
