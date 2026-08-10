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
	elf_Arena arena = elf_arena_create(KILOBYTES(1));

	char *start = elf_arena_push(&arena, 0);
	char *reserved = elf_arena_reserve(&arena, 16);
	expect_arena_u64(arena.in_use, 0, "arena reserve does not commit bytes");
	if (reserved != start) {
		test_fail("arena reserve returns current cursor");
	}

	char *pushed = elf_arena_push(&arena, 16);
	if (pushed != start) {
		test_fail("arena push returns current cursor");
	}
	expect_arena_u64(arena.in_use, 16, "arena push advances cursor");

	elf_arena_destroy(&arena);
}

static void test_arena_zero_copy_and_data(void)
{
	elf_Arena arena = elf_arena_create(KILOBYTES(1));

	char *zero = elf_arena_push_zero(&arena, 4);
	for (u32 i = 0; i < 4; ++i) {
		if (zero[i] != 0) {
			test_fail("arena push zero clears memory");
			break;
		}
	}

	char source[] = {'a', 'b', 'c', 0};
	char *copy = elf_arena_push_copy(&arena, sizeof(source), source);
	expect_arena_text(copy, source, sizeof(source), "arena push copy copies bytes");

	char *data = elf_arena_push_data(&arena, "xyz", 3);
	expect_arena_text(data, "xyz", 3, "arena push data copies unterminated bytes");
	expect_arena_u64(arena.in_use, 4 + sizeof(source) + 3, "arena copy helpers advance by byte count");

	elf_arena_destroy(&arena);
}

static void test_arena_text_char_repeat(void)
{
	elf_Arena arena = elf_arena_create(KILOBYTES(1));
	char *start = elf_arena_push(&arena, 0);

	elf_arena_push_text(&arena, "elf");
	elf_arena_push_char(&arena, '-');
	elf_arena_push_nchar(&arena, 'x', 3);
	char *end = elf_arena_push_zero(&arena, 1);

	expect_arena_text(start, "elf-xxx", 7, "arena text char repeat append in order");
	expect_arena_u64((u64)(end - start), 7, "arena explicit terminator starts after appended text");
	if (strcmp(start, "elf-xxx") != 0) {
		test_fail("arena explicit terminator produces c string");
	}

	elf_arena_destroy(&arena);
}

static void test_arena_align(void)
{
	elf_Arena arena = elf_arena_create(KILOBYTES(1));
	elf_arena_push(&arena, 3);
	elf_arena_align(&arena, 8);
	void *aligned = elf_arena_push(&arena, 8);
	if ((u64)((u8 *)aligned - arena.data) & 7) {
		test_fail("arena align advances to the requested boundary");
	}
	expect_arena_u64(arena.in_use, 16, "arena aligned allocation advances past padding");
	elf_arena_destroy(&arena);
}

static void test_arena_pushf(void)
{
	elf_Arena arena = elf_arena_create(KILOBYTES(1));
	char *start = elf_arena_push(&arena, 0);

	char *first = elf_arena_pushf(&arena, "%s:%d", "hp", 42);
	expect_arena_u64(arena.in_use, 5, "arena pushf advances by formatted byte count only");
	if (first != start || strcmp(first, "hp:42") != 0) {
		test_fail("arena pushf writes formatted text");
	}

	elf_arena_push_text(&arena, "|");
	elf_arena_pushf(&arena, "%.2f", 1.5);
	char *end = elf_arena_push_zero(&arena, 1);

	if (strcmp(start, "hp:42|1.50") != 0) {
		test_fail("arena pushf composes with later arena appends");
	}
	expect_arena_u64((u64)(end - start), 10, "arena pushf composed text size");

	elf_arena_destroy(&arena);
}

static void test_scratch_regression(void)
{
	elf_Scratch outer = elf_begin_scratch();
	u64 outer_start = outer.arena->in_use;
	elf_arena_push_text(outer.arena, "outer");

	elf_Scratch inner = elf_begin_scratch();
	u64 inner_start = inner.arena->in_use;
	elf_arena_push_text(inner.arena, "inner");
	expect_arena_u64(inner.arena->in_use, inner_start + 5, "scratch inner advances cursor");
	elf_end_scratch(inner);
	expect_arena_u64(outer.arena->in_use, inner_start, "scratch inner regresses cursor");

	elf_end_scratch(outer);
	expect_arena_u64(outer.arena->in_use, outer_start, "scratch outer regresses cursor");
}

static void run_arena_tests(void)
{
	test_arena_push_and_reserve();
	test_arena_zero_copy_and_data();
	test_arena_text_char_repeat();
	test_arena_align();
	test_arena_pushf();
	test_scratch_regression();
}
