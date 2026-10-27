static void test_array_value(void)
{
	elf_State *state = elf_create_state();
	elf_Array *array = elf_array_new(state);
	elf_Value value = value_from_array(array);

	if (!value_is_array(value) || !value_is_object(value) || value_as_array(value) != array || value_as_object(value) != (elf_Object *)array)
	{
		test_fail("array value helpers preserve the array object");
	}
	if (value_type(value) != ELF_VALUE_TYPE_ARRAY || strcmp(value_type_name(value_type(value)), "array") != 0)
	{
		test_fail("array value has the array type");
	}

	elf_destroy_state(state);
}

static void run_array_tests(void)
{
	TEST_RUN("array.value", test_array_value);
}
