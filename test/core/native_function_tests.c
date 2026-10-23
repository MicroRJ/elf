static ELF_FUNCTION(native_function_test_a)
{
	(void)S;
	(void)nargs;
	(void)nrets;
	return 0;
}

static ELF_FUNCTION(native_function_test_b)
{
	(void)S;
	(void)nargs;
	(void)nrets;
	return 0;
}

static void test_native_functions_assign_stable_indices(elf_State *state)
{
	u32 initial_count = state->native_function_count;
	u32 first = UINT32_MAX;
	u32 second = UINT32_MAX;
	u32 repeated = UINT32_MAX;
	ErrorCode first_error = elf_native_function_ensure(state, native_function_test_a, &first);
	ErrorCode second_error = elf_native_function_ensure(state, native_function_test_b, &second);
	ErrorCode repeated_error = elf_native_function_ensure(state, native_function_test_a, &repeated);

	if (first_error != ERROR_NONE || second_error != ERROR_NONE || repeated_error != ERROR_NONE ||
		first == second || repeated != first || state->native_function_count != initial_count + 2 ||
		state->native_functions[first] != native_function_test_a || state->native_functions[second] != native_function_test_b)
	{
		test_fail("native functions assign stable indices");
	}
}

static void test_native_functions_reject_invalid_arguments(elf_State *state)
{
	u32 count = state->native_function_count;
	u32 index = 0;
	if (elf_native_function_ensure(state, 0, &index) != ERROR_INVALID_ARGUMENT || index != UINT32_MAX ||
		elf_native_function_ensure(state, native_function_test_a, 0) != ERROR_INVALID_ARGUMENT ||
		state->native_function_count != count)
	{
		test_fail("native functions reject invalid arguments");
	}
}

static void run_native_function_tests(elf_State *state)
{
	TEST_RUN("native_functions.assign_stable_indices", test_native_functions_assign_stable_indices, state);
	TEST_RUN("native_functions.reject_invalid_arguments", test_native_functions_reject_invalid_arguments, state);
}
