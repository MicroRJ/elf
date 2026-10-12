static char *test_serialize_to_text(elf_State *state, elf_Arena *arena, elf_Value value)
{
	char *start = elf_arena_push(arena, 0);
	if (!elf_unparse_value(state, arena, value, 0)) {
		test_fail("serialize accepts value");
	}
	elf_arena_push_zero(arena, 1);
	return start;
}

static void expect_serialize_text(elf_State *state, elf_Value value, const char *expected, const char *label)
{
	elf_Arena arena = elf_arena_create(0);
	char *text = test_serialize_to_text(state, &arena, value);
	if (strcmp(text, expected) != 0)
	{
		fprintf(stderr, "FAIL: %s expected:\n%s\nactual:\n%s\n", label, expected, text);
		test_failures += 1;
	}
	elf_arena_destroy(&arena);
}

static void test_serialize_scalars(elf_State *state)
{
	expect_serialize_text(state, value_nil(), "nil", "serialize nil");
	expect_serialize_text(state, value_from_integer(42), "42", "serialize integer");
	expect_serialize_text(state, value_from_string(elf_string_from_data(state, "a\"b\\c\n")), "\"a\\\"b\\\\c\\n\"", "serialize escaped string");
}

static void test_serialize_table(elf_State *state)
{
	elf_Table *table = push_new_table(state);
	elf_table_set(state, table, value_from_string(elf_string_from_data(state, "answer")), value_from_integer(42));

	const char *expected =
		"{\n"
		"\t\"answer\" = 42\n"
		"}";
	expect_serialize_text(state, value_from_table(table), expected, "serialize keyed table");
	pop_value(state);
}

static void test_serialize_array(elf_State *state)
{
	elf_Array *array = push_new_array(state);
	elf_array_add(state, array, value_from_integer(10));
	elf_array_add(state, array, value_from_string(elf_string_from_data(state, "row")));

	const char *expected = "[10, \"row\"]";
	expect_serialize_text(state, value_from_array(array), expected, "serialize array");
	pop_value(state);
}

static void test_serialize_rejects_unkeyed_table(elf_State *state)
{
	elf_Table *table = push_new_table(state);
	elf_deprecated_array_add(state, table, value_from_integer(10));
	elf_Arena arena = elf_arena_create(0);
	if (elf_unparse_value(state, &arena, value_from_table(table), 0))
	{
		test_fail("serialize rejects table storage without keys");
	}
	elf_arena_destroy(&arena);
	pop_value(state);
}

static void run_unparse_tests(elf_State *state)
{
	test_serialize_scalars(state);
	test_serialize_table(state);
	test_serialize_array(state);
	test_serialize_rejects_unkeyed_table(state);
}
