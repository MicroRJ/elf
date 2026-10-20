static char *test_serialize_to_text(elf_State *state, Arena *arena, elf_Value value)
{
	char *start = arena_push(arena, 0, 1);
	if (!elf_unparse_value(state, arena, value, 0)) {
		test_fail("serialize accepts value");
	}
	arena_push_zero(arena, 1, 1);
	return start;
}

static void expect_serialize_text(elf_State *state, elf_Value value, const char *expected, const char *label)
{
	Arena arena = arena_create(0);
	char *text = test_serialize_to_text(state, &arena, value);
	if (strcmp(text, expected) != 0)
	{
		TEST_FAIL("%s expected:\n%s\nactual:\n%s", label, expected, text);
	}
	arena_destroy(&arena);
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

static void run_unparse_tests(elf_State *state)
{
	TEST_RUN("unparse.scalars", test_serialize_scalars, state);
	TEST_RUN("unparse.table", test_serialize_table, state);
	TEST_RUN("unparse.array", test_serialize_array, state);
}
