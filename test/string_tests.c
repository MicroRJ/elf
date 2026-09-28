static void expect_same_string(elf_String *left, elf_String *right, const char *label)
{
	if (left != right) {
		test_fail(label);
	}
}

static void expect_different_string(elf_String *left, elf_String *right, const char *label)
{
	if (left == right) {
		test_fail(label);
	}
}

static void test_string_interned_identity(elf_State *state)
{
	elf_String *first = elf_string_from_data(state, "player.position");
	elf_String *second = elf_string_from_data(state, "player.position");
	elf_String *other = elf_string_from_data(state, "player.velocity");

	expect_same_string(first, second, "same string data returns same interned pointer");
	expect_different_string(first, other, "different string data returns different pointer");
}

static void test_string_size_limited_interning(elf_State *state)
{
	elf_String *first = elf_string_from_data_size(state, "abcdef", 3);
	elf_String *second = elf_string_from_data(state, "abc");
	elf_String *third = elf_string_from_data_size(state, "abcxyz", 3);

	expect_same_string(first, second, "size-limited string matches exact text");
	expect_same_string(first, third, "size-limited string ignores trailing data");
	if (string_size(first) != 3 || first->obj.size != ELF_STRING_HEADER_SIZE + 4) {
		test_fail("string length derives from object allocation size");
	}
}

#if 0
static void test_string_public_text_helpers(elf_State *state)
{
	elf_String *string = elf_string_from_data(state, "copy.me");

	elf_Scratch scratch = elf_begin_scratch();
	elf_StrSlice copy = elf_string_copy_text(scratch.arena, string);
	if (copy.size != 7 || strcmp(copy.data, "copy.me") != 0) {
		test_fail("string copy returns arena-owned c string");
	}
	elf_end_scratch(scratch);
}
#endif

static void force_string_gc(elf_State *state)
{
	state->gc_next_cycle_bytes = 1;
	elf_new_table_rogue(state);
}

static u32 test_string_hash_data(const char *data, u32 size)
{
	u32 hash = 2166136261u;

	for (u32 i = 0; i < size; ++i) {
		hash ^= (u8)data[i];
		hash *= 16777619u;
	}

	return hash;
}

static void test_string_stack_roots_survive_gc(void)
{
	elf_State *state = elf_create_state();
	elf_Value *stack_checkpoint = state->stack_ptr;

	elf_String *interned = elf_string_from_data(state, "rooted.string");
	push_value(state, value_from_string(interned));

	force_string_gc(state);

	elf_String *again = elf_string_from_data(state, "rooted.string");
	expect_same_string(interned, again, "stack rooted string survives GC");

	state->stack_ptr = stack_checkpoint;
	elf_destroy_state(state);
}

static void test_string_unrooted_values_are_swept(void)
{
	elf_State *state = elf_create_state();
	u32 baseline = state->string_count;

	for (int i = 0; i < 4096; ++i) {
		char text[64];
		sprintf(text, "temporary.%d", i);
		elf_string_from_data(state, text);
	}

	force_string_gc(state);

	if (state->string_count > baseline + 8) {
		fprintf(stderr, "FAIL: unrooted strings are swept expected interner count near %u, got %u\n"
		,	baseline
		,	state->string_count);
		test_failures += 1;
	}
	elf_destroy_state(state);
}

static void test_string_bucket_unlinks_dead_collisions(void)
{
	elf_State *state = elf_create_state();
	elf_Value *stack_checkpoint = state->stack_ptr;

	elf_String *root = elf_string_from_data(state, "rooted.bucket.string");
	push_value(state, value_from_string(root));

	u32 bucket_index = root->hash & (state->string_bucket_count - 1);
	u32 collisions = 0;

	for (u32 i = 0; collisions < 8 && i < 100000; ++i) {
		char text[64];
		sprintf(text, "dead.bucket.string.%u", i);

		u32 hash = test_string_hash_data(text, (u32)strlen(text));
		if ((hash & (state->string_bucket_count - 1)) == bucket_index) {
			elf_string_from_data(state, text);
			collisions++;
		}
	}

	if (collisions < 8) {
		test_fail("string collision test generated enough colliding strings");
		state->stack_ptr = stack_checkpoint;
		elf_destroy_state(state);
		return;
	}

	force_string_gc(state);

	elf_String *again = elf_string_from_data(state, "rooted.bucket.string");
	expect_same_string(root, again, "bucket GC keeps rooted string behind dead collisions");

	state->stack_ptr = stack_checkpoint;
	elf_destroy_state(state);
}

static void run_string_tests(elf_State *state)
{
	test_string_interned_identity(state);
	test_string_size_limited_interning(state);
	// test_string_public_text_helpers(state);
	test_string_stack_roots_survive_gc();
	test_string_unrooted_values_are_swept();
	test_string_bucket_unlinks_dead_collisions();
}
