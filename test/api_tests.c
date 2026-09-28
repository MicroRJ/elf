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

static void test_api_source_diagnostics(elf_State *state)
{
	elf_i32 checkpoint = elf_get_top(state);
	elf_push_int(state, 77);
	elf_i32 sentinel_top = elf_get_top(state);
	elf_StrSlice missing_source = {};
	elf_CompileReport report = {};
	if (elf_push_constant_expr(state, 0, missing_source, &report) != ELF_ERROR_INVALID_ARGUMENT
	|| elf_push_json(state, 0, missing_source, &report) != ELF_ERROR_INVALID_ARGUMENT
	|| elf_push_code_source(state, 0, missing_source, &report) != ELF_ERROR_INVALID_ARGUMENT
	|| elf_get_top(state) != sentinel_top) {
		test_fail("invalid source arguments preserve the stack");
	}
	if (report.diagnostic_count != 0) {
		test_fail("invalid source arguments produce no compiler diagnostics");
	}

	char invalid_constant_text[] = "{\n  answer = runtime_value,\n}";
	elf_StrSlice invalid_constant = {invalid_constant_text, sizeof(invalid_constant_text) - 1};
	if (elf_push_constant_expr(state, "invalid-constant.elf", invalid_constant, &report)
	!= ELF_ERROR_COMPILATION_FAILED) {
		test_fail("API rejects a non-constant expression");
	}
	if (elf_get_top(state) != sentinel_top) {
		test_fail("failed constant expression leaves the stack unchanged");
	}
	elf_Int sentinel = 0;
	if (!elf_to_int(state, -1, &sentinel) || sentinel != 77) {
		test_fail("failed constant expression preserves existing stack values");
	}

	if (report.error_count == 0 || report.diagnostic_count == 0
	|| report.diagnostics[0].severity != ELF_DIAGNOSTIC_ERROR
	|| report.diagnostics[0].phase != ELF_DIAGNOSTIC_PHASE_EVALUATION
	|| !api_slice_matches(report.diagnostics[0].source_name, "invalid-constant.elf")
	|| report.diagnostics[0].line != 2
	|| report.diagnostics[0].column != 12
	|| !api_slice_contains(report.diagnostics[0].message, "not a constant expression")) {
		test_fail("API reports constant-expression diagnostics");
	}
	elf_destroy_compile_report(&report);

	char invalid_string_text[] = "\"\\q\"";
	elf_StrSlice invalid_string = {invalid_string_text, sizeof(invalid_string_text) - 1};
	if (elf_push_constant_expr(state, "invalid-string.elf", invalid_string, &report)
	!= ELF_ERROR_COMPILATION_FAILED) {
		test_fail("API rejects lexer errors");
	}
	if (elf_get_top(state) != sentinel_top) {
		test_fail("lexer failure leaves the stack unchanged");
	}
	if (report.error_count == 0 || report.diagnostic_count == 0
	|| report.diagnostics[0].phase != ELF_DIAGNOSTIC_PHASE_LEXER
	|| !api_slice_matches(report.diagnostics[0].source_name, "invalid-string.elf")) {
		test_fail("API retains lexer diagnostics");
	}
	elf_destroy_compile_report(&report);

	char valid_constant_text[] = "42";
	elf_StrSlice valid_constant = {valid_constant_text, sizeof(valid_constant_text) - 1};
	if (elf_push_constant_expr(state, "valid-constant.elf", valid_constant, &report) != ELF_ERROR_NONE) {
		test_fail("API parses valid source after a failed constant expression");
	}
	elf_Int constant = 0;
	if (!elf_to_int(state, -1, &constant) || constant != 42) {
		test_fail("API returns a valid constant after a failed parse");
	}
	if (report.error_count != 0) {
		test_fail("successful source parsing has no errors");
	}
	elf_destroy_compile_report(&report);
	elf_pop(state, 1);

	elf_Module *module_checkpoint = state->modules;
	elf_u64 arena_checkpoint = state->arena.in_use;
	char invalid_code_text[] = "answer := 1 +";
	elf_StrSlice invalid_code = {invalid_code_text, sizeof(invalid_code_text) - 1};
	if (elf_push_code_source(state, "invalid-code.elf", invalid_code, &report)
	!= ELF_ERROR_COMPILATION_FAILED) {
		test_fail("API rejects malformed code source");
	}
	if (elf_get_top(state) != sentinel_top) {
		test_fail("failed code compilation leaves the stack unchanged");
	}
	if (state->modules != module_checkpoint || state->arena.in_use != arena_checkpoint) {
		test_fail("failed code compilation publishes no partial module");
	}
	if (report.error_count < 2 || report.diagnostic_count < 2
	|| report.diagnostics[0].phase != ELF_DIAGNOSTIC_PHASE_PARSER
	|| !api_slice_matches(report.diagnostics[0].source_name, "invalid-code.elf")) {
		test_fail("API retains multiple code parser diagnostics");
	}
	elf_destroy_compile_report(&report);

	char valid_code_text[] = "unused := 1\nret 42";
	elf_StrSlice valid_code = {valid_code_text, sizeof(valid_code_text) - 1};
	if (elf_push_code_source(state, "valid-code.elf", valid_code, &report) != ELF_ERROR_NONE) {
		test_fail("API compiles valid code after a failed parse");
	}
	else
	{
		elf_push_nil(state);
		elf_call(state, 1, 1);
		elf_Int result = 0;
		if (!elf_to_int(state, -1, &result) || result != 42) {
			test_fail("code compiled after a failed parse executes correctly");
		}
		elf_pop(state, 1);
	}
	if (report.error_count != 0 || report.warning_count != 1
	|| report.diagnostic_count != 1
	|| report.diagnostics[0].severity != ELF_DIAGNOSTIC_WARNING
	|| report.diagnostics[0].phase != ELF_DIAGNOSTIC_PHASE_LOWERING
	|| !api_slice_contains(report.diagnostics[0].message, "unreferenced entity")) {
		test_fail("successful code compilation retains lowering warnings");
	}
	elf_destroy_compile_report(&report);

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
	|| elf_push_constant_expr(state, "roundtrip.elf", source, 0) != ELF_ERROR_NONE) {
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
	test_api_mutation_errors(state);
	test_api_source_diagnostics(state);
	test_api_value_source(state);
	elf_destroy_state(state);
}
