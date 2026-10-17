static void expect_arena_u64(u64 actual, u64 expected, const char *label)
{
	if (actual != expected)
	{
		fprintf(stderr, "FAIL: %s expected %llu, got %llu\n", label, expected, actual);
		test_failures += 1;
	}
}

static void expect_arena_text(const char *actual, const char *expected, u64 size, const char *label)
{
	if (memcmp(actual, expected, size) != 0)
	{
		fprintf(stderr, "FAIL: %s expected '%.*s', got '%.*s'\n",
			label, (i32)size, expected, (i32)size, actual);
		test_failures += 1;
	}
}

static void test_arena_push_and_reserve(void)
{
	Arena arena = arena_create(KILOBYTES(1));

	char *start = arena_push(&arena, 0, 1);
	char *reserved = arena_reserve(&arena, 16);
	expect_arena_u64(arena.used, 0, "arena reserve does not consume bytes");
	if (reserved != start) {
		test_fail("arena reserve returns current cursor");
	}

	char *pushed = arena_push(&arena, 16, 1);
	if (pushed != start) {
		test_fail("arena push returns current cursor");
	}
	expect_arena_u64(arena.used, 16, "arena push advances cursor");

	arena_destroy(&arena);
}

static void test_arena_zero_copy_and_data(void)
{
	Arena arena = arena_create(KILOBYTES(1));

	char *zero = arena_push_zero(&arena, 4, 1);
	for (u32 i = 0; i < 4; ++i) {
		if (zero[i] != 0) {
			test_fail("arena push zero clears memory");
			break;
		}
	}

	char source[] = {'a', 'b', 'c', 0};
	char *copy = arena_push_copy(&arena, sizeof(source), 1, source);
	expect_arena_text(copy, source, sizeof(source), "arena push copy copies bytes");

	char *data = arena_push_data(&arena, "xyz", 3);
	expect_arena_text(data, "xyz", 3, "arena push data copies unterminated bytes");
	expect_arena_u64(arena.used, 4 + sizeof(source) + 3, "arena copy helpers advance by byte count");

	arena_destroy(&arena);
}

static void test_arena_text_char_repeat(void)
{
	Arena arena = arena_create(KILOBYTES(1));
	char *start = arena_push(&arena, 0, 1);

	arena_push_text(&arena, "elf");
	arena_push_char(&arena, '-');
	arena_push_nchar(&arena, 'x', 3);
	char *end = arena_push_zero(&arena, 1, 1);

	expect_arena_text(start, "elf-xxx", 7, "arena text char repeat append in order");
	expect_arena_u64((u64)(end - start), 7, "arena explicit terminator starts after appended text");
	if (strcmp(start, "elf-xxx") != 0) {
		test_fail("arena explicit terminator produces c string");
	}

	arena_destroy(&arena);
}

static void test_arena_align(void)
{
	Arena arena = arena_create(KILOBYTES(1));
	arena_push(&arena, 3, 1);
	void *aligned = arena_push(&arena, 8, 8);
	if ((u64)((u8 *)aligned - arena.data) & 7) {
		test_fail("arena align advances to the requested boundary");
	}
	expect_arena_u64(arena.used, 16, "arena aligned allocation advances past padding");
	arena_destroy(&arena);
}

static void test_arena_pushf(void)
{
	Arena arena = arena_create(KILOBYTES(1));
	char *start = arena_push(&arena, 0, 1);

	char *first = arena_pushf(&arena, "%s:%d", "hp", 42);
	expect_arena_u64(arena.used, 5, "arena pushf advances by formatted byte count only");
	if (first != start || strcmp(first, "hp:42") != 0) {
		test_fail("arena pushf writes formatted text");
	}

	arena_push_text(&arena, "|");
	arena_pushf(&arena, "%.2f", 1.5);
	char *end = arena_push_zero(&arena, 1, 1);

	if (strcmp(start, "hp:42|1.50") != 0) {
		test_fail("arena pushf composes with later arena appends");
	}
	expect_arena_u64((u64)(end - start), 10, "arena pushf composed text size");

	arena_destroy(&arena);
}

static void test_scratch_regression(void)
{
	Scratch outer = scratch_begin();
	u64 outer_start = outer.arena->used;
	arena_push_text(outer.arena, "outer");

	Scratch inner = scratch_begin();
	u64 inner_start = inner.arena->used;
	arena_push_text(inner.arena, "inner");
	expect_arena_u64(inner.arena->used, inner_start + 5, "scratch inner advances cursor");
	scratch_end(inner);
	expect_arena_u64(outer.arena->used, inner_start, "scratch inner regresses cursor");

	scratch_end(outer);
	expect_arena_u64(outer.arena->used, outer_start, "scratch outer regresses cursor");
}

static void test_scratch_exclusion(void)
{
	Scratch first = scratch_begin();
	Scratch second = scratch_begin_excluding(first.arena);
	if (first.arena == second.arena) {
		test_fail("excluded scratch uses a distinct arena");
	}
	scratch_end(second);
	scratch_end(first);
}

static void run_arena_tests(void)
{
	test_arena_push_and_reserve();
	test_arena_zero_copy_and_data();
	test_arena_text_char_repeat();
	test_arena_align();
	test_arena_pushf();
	test_scratch_regression();
	test_scratch_exclusion();
}
