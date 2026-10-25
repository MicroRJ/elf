static ELF_FUNCTION(test_api_callback)
{
	elf_StrSlice receiver = {};
	elf_Int argument = 0;

	if (elf_arg_count(S) != 2) {
		test_fail("API callback argument count includes this");
	}
	if (!elf_to_str(S, 0, &receiver)
	|| receiver.size != 4 || memcmp(receiver.data, "this", 4) != 0) {
		test_fail("API argument zero is this");
	}
	if (!elf_to_int(S, 1, &argument) || argument != 41) {
		test_fail("API argument one is first explicit argument");
	}

	elf_push_int(S, argument + 1);
	return 1;
}

static elf_b32 api_slice_matches(elf_StrSlice slice, const char *text)
{
	elf_u64 size = (elf_u64)strlen(text);
	return slice.size == size && memcmp(slice.data, text, size) == 0;
}

static elf_b32 api_slice_contains(elf_StrSlice slice, const char *text)
{
	elf_u64 size = (elf_u64)strlen(text);
	if (size > slice.size) return false;
	for (elf_u64 i = 0; i <= slice.size - size; ++ i) {
		if (memcmp(slice.data + i, text, size) == 0) return true;
	}
	return false;
}

static void test_api_call_addressing(elf_State *state)
{
	elf_i32 checkpoint = elf_get_top(state);
	elf_push_fun(state, test_api_callback);
	elf_push_cstr(state, "this");
	elf_push_int(state, 41);
	elf_call(state, 2, 1);

	elf_Int result = 0;
	if (!elf_to_int(state, -1, &result) || result != 42) {
		test_fail("API native callback result");
	}
	if (elf_set_top(state, checkpoint) != ELF_ERROR_NONE) {
		test_fail("API restores host checkpoint");
	}
}

static void test_api_tables_and_refs(elf_State *state)
{
	elf_i32 checkpoint = elf_get_top(state);
	elf_new_table(state);
	elf_i32 table = elf_abs_index(state, -1);

	elf_push_int(state, 42);
	if (elf_set_field(state, table, "answer") != ELF_ERROR_NONE) {
		test_fail("API sets a generic field value");
	}
	if (!elf_get_field(state, table, "answer")) {
		test_fail("API gets a field");
	}
	elf_Int answer = 0;
	if (!elf_to_int(state, -1, &answer) || answer != 42) {
		test_fail("API field preserves its type");
	}
	elf_pop(state, 1);

	if (elf_get_index(state, table, 0)) {
		test_fail("API does not expose table storage as array indexes");
		elf_pop(state, 1);
	}

	elf_Ref reference = elf_create_ref(state, table);
	if (reference == ELF_NO_REF) {
		test_fail("API creates a state-owned reference");
	}
	elf_set_top(state, checkpoint);
	state->gc_next_cycle_bytes = 1;
	elf_table_new(state);
	if (!elf_push_ref(state, reference)) {
		test_fail("API reference survives leaving the stack");
	}
	if (!elf_get_field(state, -1, "answer")
	|| !elf_to_int(state, -1, &answer) || answer != 42) {
		test_fail("API reference restores the original table");
	}
	elf_pop(state, 1);

	elf_u32 cursor = 0;
	elf_u32 entries = 0;
	while (elf_next(state, -1, &cursor)) {
		entries += 1;
		elf_pop(state, 2);
	}
	if (entries != 1) {
		test_fail("API iterates keyed table entries");
	}

	if (elf_release_ref(state, reference) != ELF_ERROR_NONE || elf_push_ref(state, reference)) {
		test_fail("API releases state-owned references");
	}
	elf_set_top(state, checkpoint);
}

static void test_api_arrays(elf_State *state)
{
	elf_i32 checkpoint = elf_get_top(state);
	elf_new_array(state);
	elf_i32 array_index = elf_abs_index(state, -1);
	if (elf_type(state, array_index) != ELF_VALUE_TYPE_ARRAY)
	{
		test_fail("API creates an array");
	}

	elf_push_int(state, 10);
	if (elf_append(state, array_index) != ELF_ERROR_NONE)
	{
		test_fail("API appends to an array");
	}
	elf_push_int(state, 20);
	if (elf_set_index(state, array_index, 1) != ELF_ERROR_NONE)
	{
		test_fail("API set index appends at the array end");
	}
	elf_push_int(state, 15);
	if (elf_set_index(state, array_index, 0) != ELF_ERROR_NONE)
	{
		test_fail("API set index replaces an array value");
	}

	elf_u32 length = 0;
	elf_Int first = 0;
	elf_Int second = 0;
	if (!elf_length(state, array_index, &length) || length != 2)
	{
		test_fail("API reads array length");
	}
	if (!elf_get_index(state, array_index, 0) || !elf_to_int(state, -1, &first) || first != 15)
	{
		test_fail("API reads the replaced array value");
	}
	else
	{
		elf_pop(state, 1);
	}
	if (!elf_get_index(state, array_index, 1) || !elf_to_int(state, -1, &second) || second != 20)
	{
		test_fail("API reads the appended array value");
	}
	else
	{
		elf_pop(state, 1);
	}
	elf_i32 top = elf_get_top(state);
	if (elf_get_index(state, array_index, 2) || elf_get_top(state) != top)
	{
		test_fail("API rejects an out-of-range array read transactionally");
	}

	elf_push_int(state, 30);
	top = elf_get_top(state);
	if (elf_set_index(state, array_index, 3) != ELF_ERROR_OUT_OF_RANGE || elf_get_top(state) != top)
	{
		test_fail("API rejects an out-of-range array write transactionally");
	}
	elf_pop(state, 1);

	elf_Array *array = value_as_array(state->frame->arguments[array_index]);
	array->obj.status |= ELF_OBJECT_READONLY;
	elf_push_int(state, 30);
	top = elf_get_top(state);
	if (elf_append(state, array_index) != ELF_ERROR_READONLY || elf_get_top(state) != top)
	{
		test_fail("API rejects readonly array writes transactionally");
	}
	elf_set_top(state, checkpoint);
}

static void test_api_string_length(elf_State *state)
{
	elf_Index checkpoint = elf_get_top(state);
	elf_push_nil(state);
	elf_u32 length = UINT32_MAX;
	if (!elf_length(state, -1, &length) || length != 0)
	{
		test_fail("API measures nil as zero");
	}
	elf_pop(state, 1);

	elf_push_str(state, "elf\0lang", 8);
	length = 0;
	if (!elf_length(state, -1, &length) || length != 8)
	{
		test_fail("API reads string byte length");
	}
	elf_set_top(state, checkpoint);
}

static void test_api_errors(elf_State *state)
{
	elf_Index checkpoint = elf_get_top(state);
	elf_push_error(state, "host failure");
	elf_Index error = elf_abs_index(state, -1);

	if (elf_type(state, error) != ELF_VALUE_TYPE_ERROR || !elf_is_error(state, error)) {
		test_fail("API creates a tagged error value");
	}
	elf_GCError *error_object = value_as_error(state->stack_ptr[-1]);
	if (elf_string_size(error_object->message) != 12 || memcmp(elf_string_data(error_object->message), "host failure", 12) != 0)
	{
		test_fail("API error retains its message");
	}
	if (error_object->data)
	{
		test_fail("API error has no data by default");
	}
	if (elf_get_field(state, error, "message"))
	{
		test_fail("API does not treat errors as field tables");
	}
	elf_push_cstr(state, "host");
	if (elf_set_field(state, error, "kind") != ELF_ERROR_TYPE_MISMATCH)
	{
		test_fail("API does not treat errors as mutable field tables");
	}
	elf_pop(state, 1);

	if (!elf_push_value_text(state, error)) {
		test_fail("API formats an error value");
	}
	else
	{
		elf_StrSlice text = {};
		if (!elf_to_str(state, -1, &text) || !api_slice_matches(text, "host failure")) {
			test_fail("API formats an error using its message");
		}
		elf_pop(state, 1);
	}
	elf_Index top = elf_get_top(state);
	if (elf_push_value_source(state, error) || elf_get_top(state) != top) {
		test_fail("API rejects errors as constant-expression source");
	}

	elf_u32 length = 0;
	if (elf_length(state, error, &length)) {
		test_fail("API does not expose an error as an array table");
	}
	elf_new_table(state);
	if (elf_is_error(state, -1)) {
		test_fail("API distinguishes ordinary tables from errors");
	}
	elf_set_top(state, checkpoint);
}

static void test_api_buffers(elf_State *state)
{
	elf_Index checkpoint = elf_get_top(state);
	char source_data[] = "ret elf.buffer(4):append(\"ok\")";
	elf_StrSlice source = {source_data, sizeof(source_data) - 1};
	if (elf_push_code_source(state, 0, ELF_LIT("api-buffer"), source) != ELF_ERROR_NONE)
	{
		test_fail("API buffer source compiles");
		elf_set_top(state, checkpoint);
		return;
	}
	elf_push_nil(state);
	elf_call(state, 1, 1);
	elf_Index buffer = elf_abs_index(state, -1);
	if (elf_type(state, buffer) != ELF_VALUE_TYPE_BUFFER) {
		test_fail("API identifies a buffer value");
	}
	elf_u32 length = 0;
	if (!elf_length(state, buffer, &length) || length != 2) {
		test_fail("API reads buffer length");
	}
	elf_Int byte = 0;
	if (!elf_get_index(state, buffer, 1) || !elf_to_int(state, -1, &byte) || byte != 'k') {
		test_fail("API reads a buffer byte");
	}
	else
	{
		elf_pop(state, 1);
	}

	elf_push_int(state, 'n');
	if (elf_set_index(state, buffer, 1) != ELF_ERROR_NONE ||
		!elf_get_index(state, buffer, 1) || !elf_to_int(state, -1, &byte) || byte != 'n') {
		test_fail("API writes a buffer byte");
	}
	else
	{
		elf_pop(state, 1);
	}

	if (!elf_push_value_text(state, buffer)) {
		test_fail("API formats a buffer value");
	}
	else
	{
		elf_StrSlice text = {};
		if (!elf_to_str(state, -1, &text) || !api_slice_matches(text, "buffer(2)")) {
			test_fail("API buffer display includes its length");
		}
		elf_pop(state, 1);
	}
	elf_Index top = elf_get_top(state);
	if (elf_push_value_source(state, buffer) || elf_get_top(state) != top) {
		test_fail("API rejects buffers as constant-expression source");
	}
	elf_set_top(state, checkpoint);
}

static u32 api_user_object_destroyed;

static void test_api_user_object_destroy(elf_State *state, void *data)
{
	(void)state;
	if (*(elf_u64 *)data != 0x123456789abcdef0ull) {
		test_fail("API user object destructor receives its payload");
	}
	api_user_object_destroyed += 1;
}

static const elf_UserType test_api_user_type = {
	.name = "api_test_object",
	.destroy = test_api_user_object_destroy,
};

static const elf_UserType test_api_other_user_type = {
	.name = "api_test_other",
};

static void test_api_user_objects(elf_State *state)
{
	elf_Index checkpoint = elf_get_top(state);
	elf_new_table(state);
	elf_Index metatable = elf_abs_index(state, -1);
	elf_push_int(state, 42);
	elf_set_field(state, metatable, "answer");

	void *data = 0;
	if (elf_push_user_object(state, &test_api_user_type, metatable,
		sizeof(elf_u64), &data) != ELF_ERROR_NONE || !data) {
		test_fail("API creates a user object");
		elf_set_top(state, checkpoint);
		return;
	}
	*(elf_u64 *)data = 0x123456789abcdef0ull;
	elf_Index object = elf_abs_index(state, -1);
	if (elf_type(state, object) != ELF_VALUE_TYPE_USER_OBJECT ||
		!elf_is_user_object(state, object, &test_api_user_type) ||
		elf_is_user_object(state, object, &test_api_other_user_type) ||
		elf_to_user_object(state, object, &test_api_user_type) != data ||
		elf_to_user_object(state, object, &test_api_other_user_type)) {
		test_fail("API identifies nominal user object types");
	}

	elf_Ref reference = elf_create_ref(state, object);
	elf_set_top(state, checkpoint);
	force_gc_allocations(state, 32);
	if (api_user_object_destroyed != 0 || !elf_push_ref(state, reference)) {
		test_fail("referenced user object and metatable survive GC");
	}
	else
	{
		elf_UserObject *user = value_as_user(state->stack_ptr[-1]);
		elf_Value answer = elf_table_get_or_nil(state, user->metatable,
			test_key_string(state, "answer"));
		if (!value_is_integer(answer) || value_as_integer(answer) != 42)
			test_fail("user object retains its instance metatable");
		elf_pop(state, 1);
	}
	elf_release_ref(state, reference);
	force_gc_allocations(state, 32);
	if (api_user_object_destroyed != 1) {
		test_fail("unreachable user object runs its destructor exactly once");
	}
	elf_set_top(state, checkpoint);
}

static void test_api_mutation_errors(elf_State *state)
{
	elf_Index checkpoint = elf_get_top(state);
	if (elf_pop(state, 1) != ELF_ERROR_STACK_UNDERFLOW) {
		test_fail("API reports stack underflow");
	}
	if (elf_push_value(state, elf_get_top(state)) != ELF_ERROR_INVALID_INDEX) {
		test_fail("API reports invalid stack indices");
	}
	if (elf_set_top(state, -1) != ELF_ERROR_OUT_OF_RANGE) {
		test_fail("API reports invalid stack tops");
	}
	if (elf_release_ref(state, ELF_NO_REF) != ELF_ERROR_INVALID_REFERENCE) {
		test_fail("API reports invalid references");
	}

	elf_push_int(state, 7);
	elf_Index value_top = elf_get_top(state);
	if (elf_set_field(state, checkpoint, "field") != ELF_ERROR_TYPE_MISMATCH
	|| elf_get_top(state) != value_top) {
		test_fail("API type errors preserve the value being written");
	}
	elf_set_top(state, checkpoint);

	elf_new_table(state);
	elf_Index table_index = elf_abs_index(state, -1);
	elf_push_int(state, 9);
	value_top = elf_get_top(state);
	if (elf_set_index(state, table_index, 1) != ELF_ERROR_TYPE_MISMATCH || elf_get_top(state) != value_top) {
		test_fail("API rejects indexed table mutation and preserves the value being written");
	}
	if (elf_append(state, table_index) != ELF_ERROR_TYPE_MISMATCH || elf_get_top(state) != value_top) {
		test_fail("API rejects appending to tables and preserves the value being written");
	}

	elf_Table *table = value_as_table(state->frame->arguments[table_index]);
	table->obj.status |= ELF_OBJECT_READONLY;
	if (elf_set_field(state, table_index, "field") != ELF_ERROR_READONLY
	|| elf_get_top(state) != value_top) {
		test_fail("API readonly errors preserve the value being written");
	}
	elf_set_top(state, checkpoint);
}

static void run_api_tests(void)
{
	elf_State *state = elf_create_state();
	TEST_RUN("api.call_addressing", test_api_call_addressing, state);
	TEST_RUN("api.tables_and_refs", test_api_tables_and_refs, state);
	TEST_RUN("api.arrays", test_api_arrays, state);
	TEST_RUN("api.string_length", test_api_string_length, state);
	TEST_RUN("api.errors", test_api_errors, state);
	TEST_RUN("api.user_objects", test_api_user_objects, state);
	TEST_RUN("api.mutation_errors", test_api_mutation_errors, state);
	elf_destroy_state(state);
}
