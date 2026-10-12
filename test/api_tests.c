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

	elf_push_cstr(state, "first");
	if (elf_append(state, table) != ELF_ERROR_NONE) {
		test_fail("API appends a generic array value");
	}
	if (!elf_get_index(state, table, 2) || !elf_is_nil(state, -1)) {
		test_fail("API missing array index produces nil");
	}
	elf_pop(state, 1);

	elf_Ref reference = elf_create_ref(state, table);
	if (reference == ELF_NO_REF) {
		test_fail("API creates a state-owned reference");
	}
	elf_set_top(state, checkpoint);
	state->gc_next_cycle_bytes = 1;
	elf_new_table_rogue(state);
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
	if (entries != 2) {
		test_fail("API iterates array and keyed table entries");
	}

	if (elf_push_value(state, -1) != ELF_ERROR_NONE
	|| elf_set_global(state, "api_test") != ELF_ERROR_NONE) {
		test_fail("API writes a global from the stack");
	}
	elf_get_global(state, "api_test");
	if (!elf_equal(state, -1, -2)) {
		test_fail("API global preserves table identity");
	}
	elf_pop(state, 1);

	if (elf_release_ref(state, reference) != ELF_ERROR_NONE || elf_push_ref(state, reference)) {
		test_fail("API releases state-owned references");
	}
	elf_push_nil(state);
	elf_set_global(state, "api_test");
	elf_set_top(state, checkpoint);
}

static void test_api_string_length(elf_State *state)
{
	elf_Index checkpoint = elf_get_top(state);
	elf_push_str(state, "elf\0lang", 8);
	elf_u32 length = 0;
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
	if (!elf_get_field(state, error, "message")) {
		test_fail("API gets an error field");
	}
	else
	{
		elf_StrSlice message = {};
		if (!elf_to_str(state, -1, &message) || !api_slice_matches(message, "host failure")) {
			test_fail("API error retains its message");
		}
		elf_pop(state, 1);
	}

	elf_push_cstr(state, "host");
	if (elf_set_field(state, error, "kind") != ELF_ERROR_NONE
	|| !elf_get_field(state, error, "kind")) {
		test_fail("API populates structured error fields");
	}
	else
	{
		elf_StrSlice kind = {};
		if (!elf_to_str(state, -1, &kind) || !api_slice_matches(kind, "host")) {
			test_fail("API structured error field retains its value");
		}
		elf_pop(state, 1);
	}

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
	if (elf_push_buffer(state, 3) != ELF_ERROR_NONE || !elf_is_buffer(state, -1)) {
		test_fail("API pushes a buffer value");
	}
	else
	{
		elf_Index host_buffer = elf_abs_index(state, -1);
		if (elf_buffer_append(state, host_buffer, "abc", 3) != ELF_ERROR_NONE ||
			elf_buffer_reserve(state, host_buffer, 16) != ELF_ERROR_NONE) {
			test_fail("API grows and appends to a buffer");
		}
		elf_BufferView view = {};
		if (!elf_to_buffer(state, host_buffer, &view) || view.size != 3 ||
			view.capacity < 16 || memcmp(view.data, "abc", 3) != 0) {
			test_fail("API exposes buffer storage");
		}
		if (elf_buffer_resize(state, host_buffer, 5) != ELF_ERROR_NONE ||
			!elf_to_buffer(state, host_buffer, &view) || view.size != 5 ||
			view.data[3] != 0 || view.data[4] != 0) {
			test_fail("API resizes and zero-fills a buffer");
		}
		if (elf_buffer_resize(state, host_buffer, 3) != ELF_ERROR_NONE ||
			elf_buffer_patch(state, host_buffer, 1, 1, "XYZ", 3) != ELF_ERROR_NONE ||
			!elf_to_buffer(state, host_buffer, &view) || view.size != 5 ||
			memcmp(view.data, "aXYZc", 5) != 0) {
			test_fail("API patches a buffer range");
		}
	}
	elf_set_top(state, checkpoint);

	char source_data[] = "ret elf.buffer(4):append(\"ok\")";
	elf_StrSlice source = {source_data, sizeof(source_data) - 1};
	if (elf_push_code_source(state, "api-buffer", source) != ELF_ERROR_NONE)
	{
		test_fail("API buffer source compiles");
		elf_set_top(state, checkpoint);
		return;
	}
	elf_push_nil(state);
	elf_call(state, 1, 1);
	elf_Index buffer = elf_abs_index(state, -1);
	if (elf_type(state, buffer) != ELF_VALUE_TYPE_BUFFER || !elf_is_buffer(state, buffer)) {
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
	else elf_pop(state, 1);

	elf_push_int(state, 'n');
	if (elf_set_index(state, buffer, 1) != ELF_ERROR_NONE ||
		!elf_get_index(state, buffer, 1) || !elf_to_int(state, -1, &byte) || byte != 'n') {
		test_fail("API writes a buffer byte");
	}
	else elf_pop(state, 1);

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
	if (elf_set_index(state, table_index, 1) != ELF_ERROR_OUT_OF_RANGE
	|| elf_get_top(state) != value_top) {
		test_fail("API bounds errors preserve the value being written");
	}

	elf_Table *table = value_as_table(state->frame->framebase[table_index]);
	table->obj.status |= ELF_OBJECT_READONLY;
	if (elf_set_field(state, table_index, "field") != ELF_ERROR_READONLY
	|| elf_get_top(state) != value_top) {
		test_fail("API readonly errors preserve the value being written");
	}
	elf_set_top(state, checkpoint);
}

static Compiler *api_test_create_compiler(elf_State *state, elf_Scratch *scratch, const char *name, elf_StrSlice source)
{
	*scratch = elf_begin_scratch();
	Compiler *compiler = compiler_create(state, scratch->arena);
	compiler_set_log_messages(compiler, false);
	compiler_add_root_module(compiler, name, source);
	return compiler;
}

static void test_api_source_compilation(elf_State *state)
{
	elf_i32 checkpoint = elf_get_top(state);
	elf_push_int(state, 77);
	elf_i32 sentinel_top = elf_get_top(state);
	elf_StrSlice missing_source = {};
	if (elf_push_constant_expr(state, 0, missing_source) != ELF_ERROR_INVALID_ARGUMENT
	|| elf_push_json(state, 0, missing_source) != ELF_ERROR_INVALID_ARGUMENT
	|| elf_push_code_source(state, 0, missing_source) != ELF_ERROR_INVALID_ARGUMENT
	|| elf_get_top(state) != sentinel_top) {
		test_fail("invalid source arguments preserve the stack");
	}
	char rejected_source_text[] = "1 +";
	elf_StrSlice rejected_source = {rejected_source_text, sizeof(rejected_source_text) - 1};
	if (elf_push_constant_expr(state, "rejected-source.elf", rejected_source) != ELF_ERROR_COMPILATION_FAILED || elf_get_top(state) != sentinel_top) {
		test_fail("public compilation reports failure without compiler messages");
	}

	elf_Scratch compiler_scratch;
	Compiler *compiler;
	Com_Report *report;
	char bounded_constant_text[] = {'4', '2'};
	elf_StrSlice bounded_constant = {bounded_constant_text, sizeof(bounded_constant_text)};
	compiler = api_test_create_compiler(state, &compiler_scratch, "bounded-constant.elf", bounded_constant);
	if (!compiler_push_constant_expr(compiler)) {
		test_fail("API accepts a source slice without trailing storage");
	}
	elf_Int bounded_value = 0;
	if (!elf_to_int(state, -1, &bounded_value) || bounded_value != 42) {
		test_fail("bounded source slice compiles completely");
	}
	elf_end_scratch(compiler_scratch);
	elf_pop(state, 1);

	char nul_source_text[] = {'4', 0, '2'};
	elf_StrSlice nul_source = {nul_source_text, sizeof(nul_source_text)};
	compiler = api_test_create_compiler(state, &compiler_scratch, "nul-source.elf", nul_source);
	if (compiler_push_constant_expr(compiler)) {
		test_fail("API rejects embedded NUL source bytes");
	}
	report = &compiler->report;
	if (elf_get_top(state) != sentinel_top || report->error_count != 1 || report->first->phase != COMPILER_PHASE_LEXER || !api_slice_contains(report->first->text, "NUL byte")) {
		test_fail("embedded NUL failure returns a lexer message");
	}
	elf_end_scratch(compiler_scratch);

#if SIZE_MAX > UINT32_MAX
	elf_StrSlice oversized_source = {bounded_constant_text, (elf_Size)UINT32_MAX + 1};
	compiler = api_test_create_compiler(state, &compiler_scratch, "oversized-source.elf", oversized_source);
	if (compiler_push_constant_expr(compiler)) {
		test_fail("API rejects source larger than its internal representation");
	}
	report = &compiler->report;
	if (elf_get_top(state) != sentinel_top || report->error_count != 1 || !api_slice_contains(report->first->text, "32-bit size limit")) {
		test_fail("oversized source failure returns a compiler message");
	}
	elf_end_scratch(compiler_scratch);
#endif

	char invalid_constant_text[] = "{\n  answer = runtime_value,\n}";
	elf_StrSlice invalid_constant = {invalid_constant_text, sizeof(invalid_constant_text) - 1};
	compiler = api_test_create_compiler(state, &compiler_scratch, "invalid-constant.elf", invalid_constant);
	if (compiler_push_constant_expr(compiler)) {
		test_fail("API rejects a non-constant expression");
	}
	if (elf_get_top(state) != sentinel_top) {
		test_fail("failed constant expression leaves the stack unchanged");
	}
	elf_Int sentinel = 0;
	if (!elf_to_int(state, -1, &sentinel) || sentinel != 77) {
		test_fail("failed constant expression preserves existing stack values");
	}

	report = &compiler->report;
	if (report->error_count == 0 || report->message_count == 0 || report->first->severity != COMPILER_ERROR || report->first->phase != COMPILER_PHASE_EVALUATION || report->first->site.module != 0 || report->first->site.line_index != 2 || source_slice_column(report->first->site) != 12 || !api_slice_contains(report->first->text, "not a constant expression")) {
		test_fail("compiler reports constant-expression messages");
	}
	elf_end_scratch(compiler_scratch);

	char invalid_string_text[] = "\"\\q\"";
	elf_StrSlice invalid_string = {invalid_string_text, sizeof(invalid_string_text) - 1};
	compiler = api_test_create_compiler(state, &compiler_scratch, "invalid-string.elf", invalid_string);
	if (compiler_push_constant_expr(compiler)) {
		test_fail("API rejects lexer errors");
	}
	if (elf_get_top(state) != sentinel_top) {
		test_fail("lexer failure leaves the stack unchanged");
	}
	report = &compiler->report;
	if (report->error_count == 0 || report->message_count == 0 || report->first->phase != COMPILER_PHASE_LEXER || report->first->site.module != 0) {
		test_fail("compiler retains lexer messages");
	}
	elf_end_scratch(compiler_scratch);

	char valid_constant_text[] = "42";
	elf_StrSlice valid_constant = {valid_constant_text, sizeof(valid_constant_text) - 1};
	compiler = api_test_create_compiler(state, &compiler_scratch, "valid-constant.elf", valid_constant);
	if (!compiler_push_constant_expr(compiler)) {
		test_fail("API parses valid source after a failed constant expression");
	}
	elf_Int constant = 0;
	if (!elf_to_int(state, -1, &constant) || constant != 42) {
		test_fail("API returns a valid constant after a failed parse");
	}
	if (compiler->report.error_count != 0) {
		test_fail("successful source parsing has no errors");
	}
	elf_end_scratch(compiler_scratch);
	elf_pop(state, 1);

	elf_Module *module_checkpoint = state->modules;
	elf_u64 arena_checkpoint = state->arena.used;
	char invalid_code_text[] = "answer := 1 +";
	elf_StrSlice invalid_code = {invalid_code_text, sizeof(invalid_code_text) - 1};
	compiler = api_test_create_compiler(state, &compiler_scratch, "invalid-code.elf", invalid_code);
	if (bc_function_ref_is_valid(compiler_compile(compiler))) {
		test_fail("API rejects malformed code source");
	}
	if (elf_get_top(state) != sentinel_top) {
		test_fail("failed code compilation leaves the stack unchanged");
	}
	if (state->modules != module_checkpoint || state->arena.used != arena_checkpoint) {
		test_fail("failed code compilation publishes no partial module");
	}
	report = &compiler->report;
	if (report->error_count < 2 || report->message_count < 2 || report->first->phase != COMPILER_PHASE_PARSER || report->first->site.module != 0) {
		test_fail("compiler retains multiple code parser messages");
	}
	u32 linked_message_count = 0;
	Com_Message *last_message = 0;
	for (Com_Message *message = report->first; message; message = message->next)
	{
		last_message = message;
		linked_message_count += 1;
	}
	if (linked_message_count != report->message_count || last_message != report->last) {
		test_fail("compiler report links every message in order");
	}
	elf_end_scratch(compiler_scratch);

	char valid_code_text[] = "unused := 1\nret 42";
	elf_StrSlice valid_code = {valid_code_text, sizeof(valid_code_text) - 1};
	compiler = api_test_create_compiler(state, &compiler_scratch, "valid-code.elf", valid_code);
	BcFunctionRef function = compiler_compile(compiler);
	if (!bc_function_ref_is_valid(function)) {
		test_fail("API compiles valid code after a failed parse");
	}
	else
	{
		ASSERT(bc_function_from_ref(function)->captures == 0);
		elf_Closure *closure = elf_gc_alloc(state, ELF_OBJECT_CLOSURE, sizeof(*closure));
		closure->function = function;
		push_value(state, value_from_closure(closure));
		elf_push_nil(state);
		elf_call(state, 1, 1);
		elf_Int result = 0;
		if (!elf_to_int(state, -1, &result) || result != 42) {
			test_fail("code compiled after a failed parse executes correctly");
		}
		elf_pop(state, 1);
	}
	report = &compiler->report;
	if (report->error_count != 0 || report->warning_count != 1 || report->message_count != 1 || report->first->severity != COMPILER_WARNING || report->first->phase != COMPILER_PHASE_LOWERING || !api_slice_contains(report->first->text, "unreferenced entity")) {
		test_fail("successful compilation retains lowering warnings");
	}
	elf_end_scratch(compiler_scratch);
	elf_set_top(state, checkpoint);
}

static void test_api_value_source(elf_State *state)
{
	elf_i32 checkpoint = elf_get_top(state);
	elf_new_table(state);
	elf_i32 root = elf_abs_index(state, -1);

	elf_push_cstr(state, "Orbiter");
	elf_set_field(state, root, "name");
	elf_Num expected_scale = -0.12345678901234567;
	elf_push_num(state, expected_scale);
	elf_set_field(state, root, "scale");
	char escaped_text[] = {'a', '\0', '\a', '\b', '\f', '\v', 'z'};
	elf_push_str(state, escaped_text, sizeof(escaped_text));
	elf_set_field(state, root, "escaped");
	elf_new_table(state);
	elf_i32 panels = elf_abs_index(state, -1);
	elf_push_int(state, 3);
	elf_append(state, panels);
	elf_push_value(state, panels);
	elf_set_field(state, root, "panels");
	elf_pop(state, 1);

	if (!elf_push_value_source(state, root)) {
		test_fail("API serializes a config table to source");
		elf_set_top(state, checkpoint);
		return;
	}
	elf_StrSlice source = {};
	if (!elf_to_str(state, -1, &source)
	|| elf_push_constant_expr(state, "roundtrip.elf", source) != ELF_ERROR_NONE) {
		test_fail("API value source parses as a constant expression");
		elf_set_top(state, checkpoint);
		return;
	}

	if (!elf_get_field(state, -1, "name")) {
		test_fail("round-tripped table contains string field");
	}
	else
	{
		elf_StrSlice name = {};
		if (!elf_to_str(state, -1, &name) || !api_slice_matches(name, "Orbiter")) {
			test_fail("round-tripped string field keeps its value");
		}
		elf_pop(state, 1);
	}
	if (!elf_get_field(state, -1, "scale")) {
		test_fail("round-tripped table contains number field");
	}
	else
	{
		elf_Num scale = 0;
		if (!elf_to_num(state, -1, &scale) || scale != expected_scale) {
			test_fail("round-tripped number field keeps its value");
		}
		elf_pop(state, 1);
	}
	if (!elf_get_field(state, -1, "escaped")) {
		test_fail("round-tripped table contains escaped string field");
	}
	else
	{
		elf_StrSlice escaped = {};
		if (!elf_to_str(state, -1, &escaped)
		|| escaped.size != sizeof(escaped_text)
		|| memcmp(escaped.data, escaped_text, sizeof(escaped_text)) != 0) {
			test_fail("round-tripped escaped string keeps every byte");
		}
		elf_pop(state, 1);
	}
	if (!elf_get_field(state, -1, "panels")) {
		test_fail("round-tripped table contains nested table");
	}
	else
	{
		elf_u32 length = 0;
		elf_Int panel = 0;
		elf_b32 got_length = elf_length(state, -1, &length);
		elf_b32 got_panel = elf_get_index(state, -1, 0);
		if (!got_length || length != 1
		|| !got_panel
		|| !elf_to_int(state, -1, &panel) || panel != 3) {
			test_fail("round-tripped nested table keeps its values");
		}
		if (got_panel) elf_pop(state, 1);
		elf_pop(state, 1);
	}
	elf_set_top(state, checkpoint);

	elf_new_table(state);
	root = elf_abs_index(state, -1);
	elf_push_fun(state, test_api_callback);
	elf_set_field(state, root, "callback");
	elf_i32 top = elf_get_top(state);
	if (elf_push_value_source(state, root) || elf_get_top(state) != top) {
		test_fail("unsupported nested values fail serialization transactionally");
	}
	elf_set_top(state, checkpoint);

	elf_new_table(state);
	root = elf_abs_index(state, -1);
	elf_push_value(state, root);
	elf_set_field(state, root, "self");
	top = elf_get_top(state);
	if (elf_push_value_source(state, root) || elf_get_top(state) != top) {
		test_fail("cyclic tables fail serialization transactionally");
	}
	elf_set_top(state, checkpoint);
}

static void run_api_tests(void)
{
	elf_State *state = elf_create_state();
	test_api_call_addressing(state);
	test_api_tables_and_refs(state);
	test_api_string_length(state);
	test_api_errors(state);
	test_api_buffers(state);
	test_api_user_objects(state);
	test_api_mutation_errors(state);
	test_api_source_compilation(state);
	test_api_value_source(state);
	elf_destroy_state(state);
}
