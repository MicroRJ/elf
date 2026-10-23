static ELF_FUNCTION(publication_test_function)
{
	(void)S;
	(void)nargs;
	(void)nrets;
	return 0;
}

static void test_public_bindings_store_owned_values(elf_State *state)
{
	u32 initial_count = state->public_binding_count;
	char integer_name[] = "app.answer";
	char number_name[] = "app.ratio";
	char string_name[] = "app.message";
	char function_name[] = "app.run";
	char string_value[] = "hello";

	elf_ErrorCode integer_error = elf_pub_int(state, (elf_StrSlice) {integer_name, sizeof(integer_name) - 1}, 42);
	elf_ErrorCode number_error = elf_pub_num(state, (elf_StrSlice) {number_name, sizeof(number_name) - 1}, 1.5);
	elf_ErrorCode string_error = elf_pub_str(state, (elf_StrSlice) {string_name, sizeof(string_name) - 1}, (elf_StrSlice) {string_value, sizeof(string_value) - 1});
	elf_ErrorCode function_error = elf_pub_fun(state, (elf_StrSlice) {function_name, sizeof(function_name) - 1}, publication_test_function);

	integer_name[0] = 'x';
	string_value[0] = 'x';

	elf_PublicBinding *integer = state->public_bindings + initial_count;
	elf_PublicBinding *number = state->public_bindings + initial_count + 1;
	elf_PublicBinding *string = state->public_bindings + initial_count + 2;
	elf_PublicBinding *function = state->public_bindings + initial_count + 3;
	b32 valid_function_index = function->native_function_index < state->native_function_count;
	b32 calls_succeeded = integer_error == ELF_ERROR_NONE && number_error == ELF_ERROR_NONE && string_error == ELF_ERROR_NONE && function_error == ELF_ERROR_NONE;
	b32 integer_is_valid = integer->type == ELF_PUBLIC_INTEGER && integer->integer == 42 && integer->name.size == sizeof(integer_name) - 1 && !memcmp(integer->name.data, "app.answer", sizeof(integer_name) - 1);
	b32 number_is_valid = number->type == ELF_PUBLIC_NUMBER && number->number == 1.5;
	b32 string_is_valid = string->type == ELF_PUBLIC_STRING && string->string.size == sizeof(string_value) - 1 && !memcmp(string->string.data, "hello", sizeof(string_value) - 1);
	b32 function_is_valid = function->type == ELF_PUBLIC_FUNCTION && valid_function_index && state->native_functions[function->native_function_index] == publication_test_function;
	if (!calls_succeeded || state->public_binding_count != initial_count + 4 || !integer_is_valid || !number_is_valid || !string_is_valid || !function_is_valid)
	{
		test_fail("public bindings store owned values");
	}
}

static void test_public_bindings_reject_invalid_and_duplicate_names(elf_State *state)
{
	u32 count = state->public_binding_count;
	elf_StrSlice duplicate_name = {(char *)"app.answer", sizeof("app.answer") - 1};
	elf_StrSlice empty_name = {(char *)"", 0};
	elf_StrSlice invalid_string = {0, 1};
	elf_ErrorCode duplicate_error = elf_pub_int(state, duplicate_name, 0);
	elf_ErrorCode empty_name_error = elf_pub_int(state, empty_name, 0);
	elf_ErrorCode invalid_string_error = elf_pub_str(state, (elf_StrSlice) {(char *)"app.invalid", sizeof("app.invalid") - 1}, invalid_string);
	elf_ErrorCode invalid_function_error = elf_pub_fun(state, (elf_StrSlice) {(char *)"app.invalid", sizeof("app.invalid") - 1}, 0);
	if (duplicate_error != ELF_ERROR_INVALID_ARGUMENT || empty_name_error != ELF_ERROR_INVALID_ARGUMENT || invalid_string_error != ELF_ERROR_INVALID_ARGUMENT || invalid_function_error != ELF_ERROR_INVALID_ARGUMENT || state->public_binding_count != count)
	{
		test_fail("public bindings reject invalid and duplicate names");
	}
}

static void run_publication_tests(elf_State *state)
{
	TEST_RUN("public_bindings.store_owned_values", test_public_bindings_store_owned_values, state);
	TEST_RUN("public_bindings.reject_invalid_and_duplicate_names", test_public_bindings_reject_invalid_and_duplicate_names, state);
}
