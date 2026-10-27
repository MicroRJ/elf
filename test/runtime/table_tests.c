static elf_Value test_key_string(elf_State *state, const char *text)
{
	elf_Value value = {};
	value = str_value(elf_string_from_data(state, text));
	return value;
}

static elf_Value test_key_int(elf_Int integer)
{
	elf_Value value = {};
	value = int_value(integer);
	return value;
}

static elf_Value test_value_int(elf_Int integer)
{
	return test_key_int(integer);
}

static void expect_int(elf_Value value, elf_Int expected, const char *label)
{
	if (value.type != ELF_VALUE_TYPE_INTEGER || value.x_int != expected) {
		TEST_FAIL("%s expected int %lld, got tag %d elf_Value %lld", label, (long long)expected, value.type, (long long)value.x_int);
	}
}

static void expect_index(u32 value, u32 expected, const char *label)
{
	if (value != expected) {
		TEST_FAIL("%s expected u32 %u, got %u", label, expected, value);
	}
}

static void expect_table_value(elf_Value value, const char *label)
{
	if (value.type != ELF_VALUE_TYPE_TABLE) {
		test_fail(label);
	}
}

static void test_table_resize_stress(elf_State *state)
{
	elf_Table *table = push_new_table(state);
	u32 initial_capacity = table->capacity;
	elf_Value keys[512] = {};

	for (u32 i = 0; i < 512; ++i) {
		char name[64];
		snprintf(name, sizeof(name), "key.%u", i);
		keys[i] = test_key_string(state, name);
		elf_table_set(state, table, keys[i], test_value_int(i + 1000));
	}

	if (table->capacity <= initial_capacity) {
		test_fail("resize increased entry capacity");
	}

	for (u32 i = 0; i < 512; ++i) {
		expect_int(elf_table_get_or_nil(state, table, keys[i]), i + 1000, "resize preserves field");
	}
}

static void test_table_delete_and_reuse_tombstones(elf_State *state)
{
	elf_Table *table = push_new_table(state);
	for (u32 i = 0; i < 128; ++i)
	{
		elf_table_set(state, table, test_key_int(i), test_value_int(i));
	}
	for (u32 i = 0; i < 128; i += 2)
	{
		elf_Value removed = value_nil();
		if (!elf_table_delete(state, table, test_key_int(i), &removed))
		{
			test_fail("table deletes existing key");
		}
		expect_int(removed, i, "table delete returns removed value");
	}
	u32 capacity = table->capacity;
	for (u32 i = 128; i < 192; ++i)
	{
		elf_table_set(state, table, test_key_int(i), test_value_int(i));
	}
	expect_index(table->count, 128, "table count tracks live mappings");
	if (table->capacity != capacity)
	{
		test_fail("table reuses tombstones before growing");
	}
	for (u32 i = 1; i < 192; i += 2)
	{
		expect_int(elf_table_get_or_nil(state, table, test_key_int(i)), i, "table preserves entries across tombstone reuse");
	}
	for (u32 i = 128; i < 192; ++i)
	{
		expect_int(elf_table_get_or_nil(state, table, test_key_int(i)), i, "table finds entries inserted over tombstones");
	}
}

static void run_table_tests(elf_State *state)
{
	TEST_RUN("table.resize_stress", test_table_resize_stress, state);
	TEST_RUN("table.delete_and_reuse_tombstones", test_table_delete_and_reuse_tombstones, state);
}
