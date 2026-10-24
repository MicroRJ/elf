static void array_expect_int(elf_Value value, i64 expected, const char *label)
{
	if (!value_is_integer(value) || value_as_integer(value) != expected)
	{
		TEST_FAIL("%s expected int %lld, got tag %d value %lld", label, (long long)expected, value.type, (long long)value.x_i64);
	}
}

static void test_array_value(void)
{
	elf_State *state = elf_create_state();
	elf_Array *array = elf_array_new(state);
	elf_Value value = value_from_array(array);
	u32 length = UINT32_MAX;

	if (!value_is_array(value) || !value_is_object(value) || value_as_array(value) != array || value_as_object(value) != (elf_Object *)array)
	{
		test_fail("array value helpers preserve the array object");
	}
	if (value_type(value) != ELF_VALUE_TYPE_ARRAY || strcmp(value_type_name(value_type(value)), "array") != 0)
	{
		test_fail("array value has the array type");
	}
	if (!value_measure(value, &length) || length != 0)
	{
		test_fail("empty array value has zero length");
	}
	if (elf_get_type_metatable(state, value) != state->metatables.array)
	{
		test_fail("array value resolves the array metatable");
	}
	expect_function_value(elf_table_get_or_nil(state, state->metatables.array, test_key_string(state, "length")), "array.length is a method");
	expect_function_value(elf_table_get_or_nil(state, state->metatables.array, test_key_string(state, "clone")), "array.clone is a method");

	elf_destroy_state(state);
}

static void test_array_operations(void)
{
	elf_State *state = elf_create_state();
	elf_Array *array = elf_array_new(state);

	for (u32 i = 0; i < 20; ++i)
	{
		u32 index = elf_array_add(state, array, value_from_integer(i));
		if (index != i)
		{
			test_fail("array append returns the appended index");
		}
	}
	if (elf_array_length(array) != 20 || array->capacity < array->count)
	{
		test_fail("array append grows dense storage");
	}

	elf_array_set(state, array, 5, value_from_integer(50));
	array_expect_int(elf_array_get(state, array, 5), 50, "array set replaces an item");

	elf_destroy_state(state);
}

static void run_array_tests(void)
{
	TEST_RUN("array.value", test_array_value);
	TEST_RUN("array.operations", test_array_operations);
}
