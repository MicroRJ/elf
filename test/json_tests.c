static elf_Value json_test_string_key(elf_State *state, const char *text)
{
	return value_from_string(elf_string_from_data(state, text));
}

static void json_expect_int(elf_Value value, i64 expected, const char *label)
{
	if (!value_is_integer(value) || value_as_integer(value) != expected)
	{
		fprintf(stderr, "FAIL: %s expected int %lld, got type %d value %lld\n",
			label, expected, value.type, value.x_int);
		test_failures += 1;
	}
}

static void json_expect_string(elf_Value value, const char *expected, const char *label)
{
	if (!value_is_string(value) || strcmp(string_data(value_as_string(value)), expected) != 0)
	{
		fprintf(stderr, "FAIL: %s expected string '%s'\n", label, expected);
		test_failures += 1;
	}
}

static elf_Table *json_expect_table(elf_Value value, const char *label)
{
	if (!value_is_table(value))
	{
		test_fail(label);
		return 0;
	}
	return value_as_table(value);
}

static void test_constexpr_table_literal(elf_State *state)
{
	u32 stack_index = (u32)(state->stack_ptr - state->stack);
	const char *text = "{ answer = 42, [7] = 8, 9 }";
	elf_StrSlice source = {(char *)text, (u64)strlen(text)};
	elf_ErrorCode error = elf_push_constant_expr(state, "constexpr-test", source, 0);
	if (error != ELF_ERROR_NONE)
	{
		test_fail("constant expression table parses");
		return;
	}

	elf_Table *table = json_expect_table(pop_value(state), "constant expression table returns table");
	if (!table)
	{
		return;
	}

	json_expect_int(elf_table_get_or_nil(state, table, json_test_string_key(state, "answer")), 42, "constant expression string field");
	json_expect_int(elf_table_get_or_nil(state, table, value_from_integer(7)), 8, "constant expression integer field");
	json_expect_int(elf_array_get(state, table, 2), 9, "constant expression array value");

	if ((u32)(state->stack_ptr - state->stack) != stack_index)
	{
		test_fail("constant expression test restores stack");
	}
}

static void test_json_to_table_value(elf_State *state)
{
	const char *text =
		"{"
		"\"name\":\"elf\","
		"\"count\":3,"
		"\"ok\":true,"
		"\"none\":null,"
		"\"items\":[1,2,{\"nested\":\"yes\"}],"
		"\"escaped\":\"a\\nb\""
		"}";

	u32 stack_index = (u32)(state->stack_ptr - state->stack);
	elf_StrSlice source = {(char *)text, (u64)strlen(text)};
	elf_ErrorCode error = elf_push_json(state, "json-test", source, 0);
	if (error != ELF_ERROR_NONE)
	{
		test_fail("json parses");
		return;
	}

	elf_Table *root = json_expect_table(pop_value(state), "json root is table");
	if (!root)
	{
		return;
	}

	json_expect_string(elf_table_get_or_nil(state, root, json_test_string_key(state, "name")), "elf", "json string field");
	json_expect_int(elf_table_get_or_nil(state, root, json_test_string_key(state, "count")), 3, "json integer field");
	json_expect_int(elf_table_get_or_nil(state, root, json_test_string_key(state, "ok")), 1, "json true field");

	if (!value_is_nil(elf_table_get_or_nil(state, root, json_test_string_key(state, "none"))))
	{
		test_fail("json null field");
	}

	elf_Table *items = json_expect_table(elf_table_get_or_nil(state, root, json_test_string_key(state, "items")), "json array field");
	if (items)
	{
		json_expect_int(elf_array_get(state, items, 0), 1, "json array first");
		json_expect_int(elf_array_get(state, items, 1), 2, "json array second");

		elf_Table *nested = json_expect_table(elf_array_get(state, items, 2), "json nested object");
		if (nested)
		{
			json_expect_string(elf_table_get_or_nil(state, nested, json_test_string_key(state, "nested")), "yes", "json nested field");
		}
	}

	json_expect_string(elf_table_get_or_nil(state, root, json_test_string_key(state, "escaped")), "a\nb", "json escaped string");

	if ((u32)(state->stack_ptr - state->stack) != stack_index)
	{
		test_fail("json test restores stack");
	}
}

static void run_json_tests(elf_State *state)
{
	test_constexpr_table_literal(state);
	test_json_to_table_value(state);
}
