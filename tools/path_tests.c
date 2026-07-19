static void expect_path_text(elf_PathStack *stack, const char *expected, const char *label)
{
	if (!stack->path || strcmp(stack->path, expected) != 0) {
		fprintf(stderr, "FAIL: %s expected '%s', got '%s'\n",
			label, expected, stack->path ? stack->path : "(null)");
		test_failures += 1;
	}
}

static void expect_path_state(elf_PathStack *stack, i32 segments, const char *name, const char *label)
{
	if (stack->segs != segments || strcmp(stack->name, name) != 0)
	{
		fprintf(stderr, "FAIL: %s expected segs=%d name='%s', got segs=%d name='%s'\n",
			label, segments, name, stack->segs, stack->name);
		test_failures += 1;
	}
}

static void test_path_push_pull(void)
{
	elf_Arena arena = elf_arena_create(0);
	elf_PathStack stack = elf_alloc_path_stack(&arena, 256);

	elf_path_push(&stack, "root");
	expect_path_text(&stack, "root", "path stack pushes first segment");
	expect_path_state(&stack, 1, "root", "path stack tracks first segment");

	elf_path_push(&stack, "child");
	expect_path_text(&stack, "root\\child", "path stack joins child segment");
	expect_path_state(&stack, 2, "child", "path stack tracks child segment");

	elf_path_pop(&stack);
	expect_path_text(&stack, "root", "path stack pulls child segment");
	expect_path_state(&stack, 1, "root", "path stack restores previous segment");

	elf_path_pop(&stack);
	expect_path_text(&stack, "", "path stack pulls final segment");
	expect_path_state(&stack, 0, "", "path stack resets after final pull");

	elf_arena_destroy(&arena);
}

static void test_path_dot_names(void)
{
	elf_Arena arena = elf_arena_create(0);
	elf_PathStack stack = elf_alloc_path_stack(&arena, 256);

	elf_path_push(&stack, ".");
	expect_path_state(&stack, 1, ".", "path stack tracks current segment name");

	elf_path_pop(&stack);
	elf_path_push(&stack, "..");
	expect_path_state(&stack, 1, "..", "path stack tracks parent segment name");

	elf_path_pop(&stack);
	elf_path_push(&stack, ".config");
	expect_path_state(&stack, 1, ".config", "path stack tracks dotted segment names");

	elf_arena_destroy(&arena);
}

static void test_path_growth_stress(void)
{
	enum { SEGMENT_COUNT = 512 };

	elf_Arena arena = elf_arena_create(0);
	elf_PathStack stack = elf_alloc_path_stack(&arena, 32768);
	char expected[32768] = {};
	i32 expected_size = 0;

	for (i32 i = 0; i < SEGMENT_COUNT; ++i)
	{
		char segment[64];
		snprintf(segment, sizeof(segment), "segment_%03d_abcdefghijklmnopqrstuvwxyz", i);

		if (i > 0) {
			expected[expected_size ++] = '\\';
		}

		i32 written = snprintf(expected + expected_size, sizeof(expected) - expected_size, "%s", segment);
		expected_size += written;

		elf_path_push(&stack, segment);
	}

	expect_path_text(&stack, expected, "path stack grows across many pushes");
	expect_path_state(&stack, SEGMENT_COUNT, "segment_511_abcdefghijklmnopqrstuvwxyz", "path stack tracks final stress segment");

	for (i32 i = SEGMENT_COUNT - 1; i >= 0; --i)
	{
		elf_path_pop(&stack);

		if (i == 0)
		{
			expected_size = 0;
			expected[0] = 0;
		}
		else
		{
			while (expected_size > 0 && expected[expected_size - 1] != '\\') {
				--expected_size;
			}
			if (expected_size > 0) {
				--expected_size;
			}
			expected[expected_size] = 0;
		}

		if (strcmp(stack.path, expected) != 0)
		{
			fprintf(stderr, "FAIL: path stack stress pull %d expected '%s', got '%s'\n",
				i, expected, stack.path);
			test_failures += 1;
			break;
		}
	}

	expect_path_state(&stack, 0, "", "path stack ends stress pull empty");
	elf_arena_destroy(&arena);
}

static void run_path_tests(void)
{
	test_path_push_pull();
	test_path_dot_names();
	test_path_growth_stress();
}
