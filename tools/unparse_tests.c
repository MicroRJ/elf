static char *test_serialize_to_text(elf_State *state, Arena *arena, elf_Value value)
{
	char *start = arena_push(arena, 0);
	if (!serialize_value(state, arena, value, 0)) {
		test_fail("serialize accepts value");
	}
	arena_push_zero(arena, 1);
	return start;
}

static void expect_serialize_text(elf_State *state, elf_Value value, const char *expected, const char *label)
{
	Arena arena = create_arena(0);
	char *text = test_serialize_to_text(state, &arena, value);
	if (strcmp(text, expected) != 0)
	{
		fprintf(stderr, "FAIL: %s expected:\n%s\nactual:\n%s\n", label, expected, text);
		test_failures += 1;
	}
	destroy_arena(&arena);
}

static void test_serialize_scalars(elf_State *state)
{
	expect_serialize_text(state, value_nil(), "nil", "serialize nil");
	expect_serialize_text(state, value_from_integer(42), "42", "serialize integer");
	expect_serialize_text(state, value_from_atom(elf_atom_from_data(state, "a\"b\\c\n")), "\"a\\\"b\\\\c\\n\"", "serialize escaped atom");
}

static void test_serialize_table(elf_State *state)
{
	elf_Table *table = elf_push_new_table(state);
	elf_array_add(state, table, value_from_integer(10));
	elf_array_add(state, table, value_from_atom(elf_atom_from_data(state, "row")));
	elf_table_set(state, table, value_from_atom(elf_atom_from_data(state, "answer")), value_from_integer(42));

	const char *expected =
		"{\n"
		"\t10,\n"
		"\t\"row\",\n"
		"\t\"answer\" = 42\n"
		"}";
	expect_serialize_text(state, value_from_table(table), expected, "serialize mixed table");
	pop_value(state);
}

static void run_unparse_tests(elf_State *state)
{
	test_serialize_scalars(state);
	test_serialize_table(state);
}
