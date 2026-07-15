static void expect_same_atom(elf_Atom *left, elf_Atom *right, const char *label)
{
	if (left != right) {
		test_fail(label);
	}
}

static void expect_different_atom(elf_Atom *left, elf_Atom *right, const char *label)
{
	if (left == right) {
		test_fail(label);
	}
}

static void test_atom_interned_identity(elf_State *state)
{
	elf_Atom *first = elf_atom_from_data(state, "player.position");
	elf_Atom *second = elf_atom_from_data(state, "player.position");
	elf_Atom *other = elf_atom_from_data(state, "player.velocity");

	expect_same_atom(first, second, "same atom data returns same interned pointer");
	expect_different_atom(first, other, "different atom data returns different pointer");
}

static void test_atom_size_limited_interning(elf_State *state)
{
	elf_Atom *first = elf_atom_from_data_size(state, "abcdef", 3);
	elf_Atom *second = elf_atom_from_data(state, "abc");
	elf_Atom *third = elf_atom_from_data_size(state, "abcxyz", 3);

	expect_same_atom(first, second, "size-limited atom matches exact text");
	expect_same_atom(first, third, "size-limited atom ignores trailing data");
}

static void test_atom_public_text_helpers(elf_State *state)
{
	elf_Atom *atom = elf_atom_from_data(state, "copy.me");

	elf_Scratch scratch = elf_get_scratch();
	elf_StrSlice copy = elf_atom_copy_text(scratch.arena, atom);
	if (copy.size != 7 || strcmp(copy.data, "copy.me") != 0) {
		test_fail("atom copy returns arena-owned c string");
	}
	elf_end_scratch(scratch);
}

static void force_atom_gc(elf_State *state)
{
	state->gc_next_cycle_bytes = 1;
	elf_table_new_unrooted(state);
}

static u32 test_atom_hash_data(const char *data, u32 size)
{
	u32 hash = 2166136261u;

	for (u32 i = 0; i < size; ++i) {
		hash ^= (u8)data[i];
		hash *= 16777619u;
	}

	return hash;
}

static void test_atom_stack_roots_survive_gc(void)
{
	elf_State *state = elf_create_state();
	elf_Value *stack_checkpoint = state->stack_ptr;

	elf_Atom *interned = elf_atom_from_data(state, "rooted.atom");
	push_value(state, value_from_atom(interned));

	force_atom_gc(state);

	elf_Atom *again = elf_atom_from_data(state, "rooted.atom");
	expect_same_atom(interned, again, "stack rooted atom survives GC");

	state->stack_ptr = stack_checkpoint;
}

static void test_atom_unrooted_values_are_swept(void)
{
	elf_State *state = elf_create_state();
	u32 baseline = state->atom_count;

	for (int i = 0; i < 4096; ++i) {
		char text[64];
		sprintf(text, "temporary.%d", i);
		elf_atom_from_data(state, text);
	}

	force_atom_gc(state);

	if (state->atom_count > baseline + 8) {
		fprintf(stderr, "FAIL: unrooted atoms are swept expected interner count near %u, got %u\n"
		,	baseline
		,	state->atom_count);
		test_failures += 1;
	}
}

static void test_atom_bucket_unlinks_dead_collisions(void)
{
	elf_State *state = elf_create_state();
	elf_Value *stack_checkpoint = state->stack_ptr;

	elf_Atom *root = elf_atom_from_data(state, "rooted.bucket.atom");
	push_value(state, value_from_atom(root));

	u32 bucket_index = root->hash & (state->atom_bucket_count - 1);
	u32 collisions = 0;

	for (u32 i = 0; collisions < 8 && i < 100000; ++i) {
		char text[64];
		sprintf(text, "dead.bucket.atom.%u", i);

		u32 hash = test_atom_hash_data(text, (u32)strlen(text));
		if ((hash & (state->atom_bucket_count - 1)) == bucket_index) {
			elf_atom_from_data(state, text);
			collisions++;
		}
	}

	if (collisions < 8) {
		test_fail("atom collision test generated enough colliding atoms");
		state->stack_ptr = stack_checkpoint;
		return;
	}

	force_atom_gc(state);

	elf_Atom *again = elf_atom_from_data(state, "rooted.bucket.atom");
	expect_same_atom(root, again, "bucket GC keeps rooted atom behind dead collisions");

	state->stack_ptr = stack_checkpoint;
}

static void test_atom_keyword_ids_survive_gc(void)
{
	elf_State *state = elf_create_state();

	elf_Atom *keyword = elf_atom_from_data(state, "true");
	u16 keyword_id = keyword->id;

	force_atom_gc(state);

	elf_Atom *again = elf_atom_from_data(state, "true");
	expect_same_atom(keyword, again, "keyword atom survives GC");
	if (again->id != keyword_id || keyword_id == 0) {
		test_fail("keyword atom keeps token id after GC");
	}
}

static void run_atom_tests(elf_State *state)
{
	test_atom_interned_identity(state);
	test_atom_size_limited_interning(state);
	test_atom_public_text_helpers(state);
	test_atom_stack_roots_survive_gc();
	test_atom_unrooted_values_are_swept();
	test_atom_bucket_unlinks_dead_collisions();
	test_atom_keyword_ids_survive_gc();
}
