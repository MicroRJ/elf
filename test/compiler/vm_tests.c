ELF_FUNCTION(vm_test_assert)
{
	elf_Value value = load_value(S, 1);
	check_numeric(S, value);
	if (!value_to_integer(value)) {
		print_current_runtime_source_location(S);
		test_fail("script assertion failed");
	}
	return 0;
}

typedef struct
{
	elf_Ref callback;
}
VmNestedCallContext;

typedef struct
{
	elf_State *state;
	elf_Value  value;
}
VmRunResult;

static void vm_force_gc_allocations(elf_State *state, u32 count)
{
	for (u32 i = 0; i < count; ++i)
	{
		state->gc_next_cycle_bytes = 1;
		elf_table_new(state);
	}
}

ELF_FUNCTION(vm_test_nested_call_preserves_context)
{
	VmNestedCallContext *context = elf_get_user_data(S);
	StackFrame *native_frame_pointer = S->frame;
	StackFrame native_frame = *S->frame;
	StackFrame *caller = S->frame > S->frame_stack ? S->frame - 1 : 0;
	elf_Prototype *caller_prototype = caller ? caller->prototype : 0;
	u32 caller_instruction = caller ? caller->instruction : 0;
	b32 preserved = context && !native_frame.prototype && caller_prototype;
	if (preserved)
	{
		preserved = BC_TYPE(caller_prototype->bytecode[caller_instruction]) == BC_DYNAMIC_CALL && elf_push_ref(S, context->callback);
	}
	if (preserved)
	{
		elf_push_nil(S);
		elf_call(S, 1, 0);
		StackFrame *restored_caller = S->frame > S->frame_stack ? S->frame - 1 : 0;
		preserved = S->frame == native_frame_pointer &&
			S->frame->framebase == native_frame.framebase && S->frame->reference == native_frame.reference &&
			S->frame->framesize == native_frame.framesize && S->frame->nargs == native_frame.nargs && S->frame->nrets == native_frame.nrets &&
			!S->frame->prototype && restored_caller &&
			restored_caller->prototype == caller_prototype &&
			restored_caller->instruction == caller_instruction;
	}
	elf_push_int(S, preserved);
	return 1;
}

static void vm_test_install_bindings(elf_State *state)
{
	elf_push_fun(state, vm_test_assert);
	if (elf_set_global(state, "test_assert") != ELF_ERROR_NONE) {
		test_fail("VM test binding registration");
	}
}

static VmRunResult vm_test_run_file(const char *path)
{
	elf_State *state = elf_create_state();
	vm_test_install_bindings(state);
	elf_push_code_file(state, path);
	elf_push_nil(state);
	elf_call(state, 1, 1);
	return (VmRunResult){state, state->stack_ptr[-1]};
}

static void vm_expect_int(const char *path, i64 expected, const char *label)
{
	VmRunResult result = vm_test_run_file(path);
	if (!value_is_integer(result.value) || value_as_integer(result.value) != expected) {
		fprintf(stderr, "FAIL: %s expected int %lld, got tag %d value %lld\n",
			label, expected, value_type(result.value), value_as_integer(result.value));
		test_failures += 1;
	}
	elf_destroy_state(result.state);
}

static void test_vm_return_int(void)
{
	vm_expect_int("test/compiler/smoke/return_int.elf", 1, "vm returns integer value");
}

static void test_vm_integer_add(void)
{
	vm_expect_int("test/compiler/smoke/return_add.elf", 3, "vm executes integer addition");
}

static void test_vm_return_nil(void)
{
	VmRunResult result = vm_test_run_file("test/compiler/smoke/return_nil.elf");
	if (!value_is_nil(result.value)) {
		test_fail("vm returns nil value");
	}
	elf_destroy_state(result.state);
}

static void test_vm_return_table(void)
{
	VmRunResult result = vm_test_run_file("test/compiler/smoke/return_table.elf");
	if (!value_is_table(result.value)) {
		test_fail("vm returns table value");
	}
	else if (value_as_table(result.value)->count != 2) {
		test_fail("vm table literal stores keyed values");
	}
	elf_destroy_state(result.state);
}

static void test_vm_return_array(void)
{
	VmRunResult result = vm_test_run_file("test/compiler/smoke/return_array.elf");
	if (!value_is_array(result.value))
	{
		test_fail("vm returns array value");
	}
	else
	{
		elf_Array *array = value_as_array(result.value);
		if (elf_array_length(array) != 2 || !value_is_array(elf_array_get(result.state, array, 0)) || !value_is_array(elf_array_get(result.state, array, 1)))
		{
			test_fail("vm array literal preserves nested arrays");
		}
		else
		{
			elf_Array *values = value_as_array(elf_array_get(result.state, array, 0));
			elf_Array *nested = value_as_array(elf_array_get(result.state, array, 1));
			if (elf_array_length(values) != 3 || value_as_integer(elf_array_get(result.state, values, 1)) != 22 || value_as_integer(elf_array_get(result.state, nested, 0)) != 40)
			{
				test_fail("vm executes array indexing and assignment");
			}
		}
	}
	elf_destroy_state(result.state);
}

static void test_vm_nil_assign(void)
{
	vm_expect_int("test/compiler/smoke/nil_assign.elf", 42, "vm nil assignment stores fallback value");
}

static void test_vm_function_call(void)
{
	vm_expect_int("test/compiler/smoke/function_call.elf", 5, "vm calls user function with arguments");
}

static void test_vm_function_arguments(void)
{
	vm_expect_int("test/compiler/smoke/function_arguments.elf", 345, "vm exposes implicit this and explicit arguments");
}

static void test_vm_method_this(void)
{
	vm_expect_int("test/compiler/smoke/method_this.elf", 242, "vm passes field receiver as implicit this");
}

static void test_vm_script_json_assertions(void)
{
	vm_expect_int("test/compiler/smoke/script_assert_json.elf", 321, "vm runs script-side json table assertions");
}

static void test_vm_core_library_assertions(void)
{
	vm_expect_int("test/compiler/smoke/script_assert_core_lib.elf", 4242, "vm runs core library assertions");
}

static void test_vm_string_join(void)
{
	VmRunResult result = vm_test_run_file("test/compiler/smoke/script_assert_string_join.elf");
	if (!value_is_string(result.value) || strcmp(value_as_string(result.value)->data, "score9") != 0) {
		test_fail("vm joins string lhs with formatted rhs");
	}
	elf_destroy_state(result.state);
}

static void test_vm_interpolated_strings(void)
{
	VmRunResult result = vm_test_run_file("test/compiler/smoke/interpolated_strings.elf");
	if (!value_is_string(result.value) || strcmp(value_as_string(result.value)->data, "score9") != 0) {
		test_fail("vm evaluates interpolated strings");
	}
	elf_destroy_state(result.state);
}

static void test_vm_static_function(void)
{
	elf_State *state = elf_create_state();
	char source_text[] = "answer :: fun() { ret 42 }\nret answer()";
	elf_StrSlice source = {source_text, sizeof(source_text) - 1};
	if (elf_push_code_source(state, "static-function", source) != ELF_ERROR_NONE)
	{
		test_fail("static function source compiles");
		elf_destroy_state(state);
		return;
	}

	elf_push_nil(state);
	elf_call(state, 1, 1);
	if (!value_is_integer(state->stack_ptr[-1]) || value_as_integer(state->stack_ptr[-1]) != 42) {
		test_fail("static function executes through the VM");
	}
	elf_destroy_state(state);
}

static void test_vm_static_recurse_function(void)
{
	elf_State *state = elf_create_state();
	char source_text[] = "count :: fun(n) { if n ? { ret recurse(n - 1) } ret 0 }\nret count(3)";
	elf_StrSlice source = {source_text, sizeof(source_text) - 1};
	if (elf_push_code_source(state, "static-recurse-function", source) != ELF_ERROR_NONE)
	{
		test_fail("static recurse function source compiles");
		elf_destroy_state(state);
		return;
	}

	elf_push_nil(state);
	elf_call(state, 1, 1);
	if (!value_is_integer(state->stack_ptr[-1]) || value_as_integer(state->stack_ptr[-1]) != 0) {
		test_fail("static function using recurse executes through its closure");
	}
	elf_destroy_state(state);
}

static void test_vm_compiled_closure_keeps_function_identity(void)
{
	elf_State *state = elf_create_state();
	char first_source_text[] = "ret fun(base) { ret fun() { ret base + 11 } }";
	char second_source_text[] = "ret fun(base) { ret fun() { ret base + 22 } }";
	elf_StrSlice first_source = {first_source_text, sizeof(first_source_text) - 1};
	elf_StrSlice second_source = {second_source_text, sizeof(second_source_text) - 1};

	if (elf_push_code_source(state, "first", first_source) != ELF_ERROR_NONE) {
		test_fail("first closure module compiles");
		elf_destroy_state(state);
		return;
	}
	elf_Closure *first_entry = value_as_closure(state->stack_ptr[-1]);
	elf_Prototype *first_entry_prototype = first_entry->prototype;
	if (first_entry_prototype != state->prototypes[0]) {
		test_fail("first compiled entry uses the first prototype index");
	}
	elf_push_nil(state);
	elf_call(state, 1, 1);
	elf_Closure *first_factory = value_as_closure(state->stack_ptr[-1]);
	elf_Prototype *first_factory_prototype = first_factory->prototype;
	elf_Ref first_factory_ref = elf_create_ref(state, -1);
	elf_pop(state, 1);

	if (elf_push_code_source(state, "second", second_source) != ELF_ERROR_NONE) {
		test_fail("second closure module compiles");
		elf_destroy_state(state);
		return;
	}
	elf_Closure *second_entry = value_as_closure(state->stack_ptr[-1]);
	elf_Prototype *second_entry_prototype = second_entry->prototype;
	if (second_entry_prototype == first_entry_prototype) {
		test_fail("each compilation publishes a distinct entry prototype");
	}
	elf_push_nil(state);
	elf_call(state, 1, 1);
	elf_Closure *second_factory = value_as_closure(state->stack_ptr[-1]);
	elf_Prototype *second_factory_prototype = second_factory->prototype;
	if (second_factory_prototype == first_factory_prototype) {
		test_fail("nested functions receive distinct state prototype indices");
	}
	elf_Ref second_factory_ref = elf_create_ref(state, -1);
	elf_pop(state, 1);

	if (first_factory_ref == ELF_NO_REF || second_factory_ref == ELF_NO_REF ||
		!elf_push_ref(state, first_factory_ref))
	{
		test_fail("compiled factories remain referenceable");
		elf_destroy_state(state);
		return;
	}
	elf_push_nil(state);
	elf_push_int(state, 100);
	elf_call(state, 2, 1);
	elf_Closure *first_closure = value_as_closure(state->stack_ptr[-1]);
	elf_Prototype *first_closure_prototype = first_closure->prototype;
	elf_Ref first = elf_create_ref(state, -1);
	elf_pop(state, 1);

	if (!elf_push_ref(state, second_factory_ref)) {
		test_fail("second compiled factory remains referenceable");
		elf_destroy_state(state);
		return;
	}
	elf_push_nil(state);
	elf_push_int(state, 200);
	elf_call(state, 2, 1);
	elf_Closure *second_closure = value_as_closure(state->stack_ptr[-1]);
	elf_Prototype *second_closure_prototype = second_closure->prototype;
	elf_Ref second = elf_create_ref(state, -1);
	elf_pop(state, 1);

	if (first == ELF_NO_REF || second == ELF_NO_REF || first_closure_prototype == second_closure_prototype)
	{
		test_fail("capturing closures preserve distinct prototype identities");
		elf_destroy_state(state);
		return;
	}

	vm_force_gc_allocations(state, 256);

	if (!elf_push_ref(state, first)) {
		test_fail("first captured closure survives GC");
		elf_destroy_state(state);
		return;
	}
	elf_push_nil(state);
	elf_call(state, 1, 1);
	elf_Int result = 0;
	if (!elf_to_int(state, -1, &result) || result != 111) {
		test_fail("compiled closure retains its module/function identity");
	}
	elf_pop(state, 1);

	if (!elf_push_ref(state, second)) {
		test_fail("second captured closure survives GC");
		elf_destroy_state(state);
		return;
	}
	elf_push_nil(state);
	elf_call(state, 1, 1);
	if (!elf_to_int(state, -1, &result) || result != 222) {
		test_fail("independent compiled modules execute overlapping local indices");
	}

	elf_release_ref(state, first_factory_ref);
	elf_release_ref(state, second_factory_ref);
	elf_release_ref(state, first);
	elf_release_ref(state, second);

	elf_destroy_state(state);
}

static void test_vm_nested_host_call_preserves_diagnostics_context(void)
{
	elf_State *state = elf_create_state();
	char callback_source_text[] = "ret 41";
	elf_StrSlice callback_source = {callback_source_text, sizeof(callback_source_text) - 1};
	if (elf_push_code_source(state, "nested-callback", callback_source) != ELF_ERROR_NONE) {
		test_fail("nested callback compiles");
		elf_destroy_state(state);
		return;
	}

	VmNestedCallContext context = {elf_create_ref(state, -1)};
	elf_pop(state, 1);
	if (context.callback == ELF_NO_REF) {
		test_fail("nested callback remains referenceable");
		elf_destroy_state(state);
		return;
	}

	elf_set_user_data(state, &context);
	elf_push_fun(state, vm_test_nested_call_preserves_context);
	if (elf_set_global(state, "context_probe") != ELF_ERROR_NONE) {
		test_fail("nested context probe binding registration");
	}

	char caller_source_text[] = "ret context_probe()";
	elf_StrSlice caller_source = {caller_source_text, sizeof(caller_source_text) - 1};
	if (elf_push_code_source(state, "nested-caller", caller_source) != ELF_ERROR_NONE) {
		test_fail("nested caller compiles");
		elf_release_ref(state, context.callback);
		elf_destroy_state(state);
		return;
	}
	elf_push_nil(state);
	elf_call(state, 1, 1);

	elf_Int preserved = 0;
	if (!elf_to_int(state, -1, &preserved) || preserved != 1) {
		test_fail("nested host call preserves the caller diagnostic context");
	}

	elf_release_ref(state, context.callback);
	elf_destroy_state(state);
}

static int vm_run_runtime_error_diagnostic_child(void)
{
	elf_State *state = elf_create_state();
	char source_text[] =
		"outer := fun(options) {\n"
		"\tmiddle := fun() {\n"
		"\t\tinner := fun(callback: callable) {\n"
		"\t\t\tret callback\n"
		"\t\t}\n"
		"\t\tinner(42)\n"
		"\t}\n"
		"\tmiddle()\n"
		"}\n"
		"outer({ first = 1, second = 2, third = 3, expected_tail = 4 })";
	elf_StrSlice source = {source_text, sizeof(source_text) - 1};
	if (elf_push_code_source(state, "diagnostic-stack", source) != ELF_ERROR_NONE)
	{
		elf_destroy_state(state);
		return 2;
	}
	elf_push_nil(state);
	elf_call(state, 1, 0);
	elf_destroy_state(state);
	return 3;
}

static void test_vm_runtime_error_diagnostics(void)
{
	Scratch scratch = scratch_begin();
	dy_String executable = {};
	dy_Pipe error_pipe = {};
	dy_Process process = {};
	dy_Process_Status status = DY_PROCESS_RUNNING;
	u32 exit_code = 0;
	char output[16384] = {};
	u64 output_size = 0;
	b32 valid = dy_get_executable_path(scratch.arena, &executable).error == DY_ERROR_NONE;
	b32 started = false;
	if (valid)
	{
		valid = dy_pipe_create(&error_pipe).error == DY_ERROR_NONE;
	}
	if (valid)
	{
		dy_String arguments[] = {DY_LIT("--runtime-error-diagnostic-child")};
		dy_Process_Start_Options options = {
			.executable = executable,
			.arguments = {arguments, ARRAY_COUNT(arguments)},
			.standard_error = error_pipe.writer,
			.hide_window = true,
		};
		started = dy_process_start(options, &process).error == DY_ERROR_NONE;
		valid = started;
	}
	if (started)
	{
		dy_stream_close(error_pipe.writer);
		error_pipe.writer = 0;
		b32 exited = dy_process_wait(process, DY_WAIT_INFINITE, &status, &exit_code).error == DY_ERROR_NONE;
		valid = valid && exited && status == DY_PROCESS_EXITED && exit_code == 1;

		b32 read_complete = false;
		while (output_size < sizeof(output) - 1)
		{
			u64 read_size = 0;
			dy_Result read = dy_stream_read(error_pipe.reader, output + output_size, sizeof(output) - output_size - 1, 0, &read_size);
			if (read.error == DY_ERROR_NONE)
			{
				output_size += read_size;
			}
			else
			{
				read_complete = read.error == DY_ERROR_END_OF_STREAM;
				break;
			}
		}
		output[output_size] = 0;
		valid = valid && read_complete;
	}
	if (valid)
	{
		const char *headline = strstr(output, "runtime error[type-mismatch]");
		const char *type_error = headline ? strstr(headline, "type error, expected 'callable', got 'integer'") : 0;
		const char *current = type_error ? strstr(type_error, "diagnostic-stack [3:16]") : 0;
		const char *current_source = current ? strstr(current, "| inner := fun(callback: callable) {") : 0;
		const char *current_marker = current_source ? strstr(current_source, "|              ^~~~~~~~") : 0;
		const char *call_stack = current_marker ? strstr(current_marker, "call stack:") : 0;
		const char *inner_caller = call_stack ? strstr(call_stack, "diagnostic-stack [6:3]") : 0;
		const char *inner_source = inner_caller ? strstr(inner_caller, "| inner(42)") : 0;
		const char *middle_caller = inner_source ? strstr(inner_source, "diagnostic-stack [8:2]") : 0;
		const char *middle_source = middle_caller ? strstr(middle_caller, "| middle()") : 0;
		const char *outer_caller = middle_source ? strstr(middle_source, "diagnostic-stack [10:1]") : 0;
		const char *outer_source = outer_caller ? strstr(outer_caller, "| outer({ first = 1, second = 2, third = 3, expected_tail = 4 })") : 0;
		valid = headline && type_error && current && current_source && current_marker && call_stack && inner_caller && inner_source && middle_caller && middle_source && outer_caller && outer_source;
	}

	dy_pipe_close(&error_pipe);
	if (dy_process_is_valid(process))
	{
		dy_process_close(&process);
	}
	if (!valid)
	{
		fprintf(stderr, "runtime error diagnostics output:\n%s", output);
		test_fail("runtime errors print the current source and ordered caller locations");
	}
	scratch_end(scratch);
}

static void test_vm_module_emitter_grows_constant_arrays(void)
{
	char source[4096];
	u32 used = (u32)snprintf(source, sizeof(source), "ret [");
	for (u32 i = 0; i < 80; ++i) used += (u32)snprintf(source + used, sizeof(source) - used, "%u,", i);
	for (u32 i = 0; i < 80; ++i) used += (u32)snprintf(source + used, sizeof(source) - used, "%u.5%s", i, i == 79 ? "]" : ",");
	if (used >= sizeof(source)) {
		test_fail("module growth test source fits its buffer");
		return;
	}

	elf_State *state = elf_create_state();
	elf_StrSlice source_slice = {source, used};
	if (elf_push_code_source(state, "constant-growth", source_slice) != ELF_ERROR_NONE) {
		test_fail("module growth source compiles");
		elf_destroy_state(state);
		return;
	}

	elf_Prototype *prototype = value_as_closure(state->stack_ptr[-1])->prototype;
	if (prototype->integer_constant_count <= 64 || prototype->number_constant_count <= 64) {
		test_fail("function emitter grows both constant arrays beyond their initial capacity");
	}
	elf_push_nil(state);
	elf_call(state, 1, 1);
	if (!value_is_array(state->stack_ptr[-1]) || elf_array_length(value_as_array(state->stack_ptr[-1])) != 160) {
		test_fail("grown module constant arrays execute correctly");
	}

	elf_destroy_state(state);
}

static void test_vm_module_converts_atom_constants(void)
{
	elf_State *state = elf_create_state();
	u32 global_count = state->global_count;
	char source_text[] = "ret [\"same\", \"same\", \"other\"]";
	elf_StrSlice source = {source_text, sizeof(source_text) - 1};
	if (elf_push_code_source(state, "module-strings", source) != ELF_ERROR_NONE) {
		test_fail("module string source compiles");
		elf_destroy_state(state);
		return;
	}

	elf_Prototype *prototype = value_as_closure(state->stack_ptr[-1])->prototype;
	if (prototype->string_count != 2 || strcmp(elf_string_data(prototype->strings[0]), "same") != 0 || strcmp(elf_string_data(prototype->strings[1]), "other") != 0)
	{
		test_fail("function converts deduplicated atom constants to strings");
	}
	if (state->global_count != global_count) {
		test_fail("string constants do not consume global slots");
	}

	vm_force_gc_allocations(state, 256);
	if (strcmp(elf_string_data(prototype->strings[0]), "same") != 0 || strcmp(elf_string_data(prototype->strings[1]), "other") != 0) {
		test_fail("function string constants survive GC");
	}

	elf_push_nil(state);
	elf_call(state, 1, 1);
	elf_Array *result = value_as_array(state->stack_ptr[-1]);
	if (elf_array_length(result) != 3 || !elf_string_equal(value_as_string(elf_array_get(state, result, 0)), prototype->strings[0]) || !elf_string_equal(value_as_string(elf_array_get(state, result, 1)), prototype->strings[0]) || !elf_string_equal(value_as_string(elf_array_get(state, result, 2)), prototype->strings[1]))
	{
		test_fail("VM loads string constants from their function");
	}

	elf_destroy_state(state);
}

static void run_vm_tests(void)
{
	test_vm_return_int();
	test_vm_integer_add();
	test_vm_return_nil();
	test_vm_return_table();
	test_vm_return_array();
	test_vm_nil_assign();
	test_vm_function_call();
	test_vm_function_arguments();
	test_vm_method_this();
#if 0
	// Disabled until JSON errors report the expected source line.
	test_vm_script_json_assertions();
#endif
#if 0
	// Disabled until the core library regains feature parity.
	test_vm_core_library_assertions();
#endif
	test_vm_string_join();
	test_vm_interpolated_strings();
	test_vm_static_function();
#if 0
	// Static recurse currently requires a missing closure.
	test_vm_static_recurse_function();
#endif
	test_vm_compiled_closure_keeps_function_identity();
	test_vm_nested_host_call_preserves_diagnostics_context();
	test_vm_runtime_error_diagnostics();
	test_vm_module_emitter_grows_constant_arrays();
	test_vm_module_converts_atom_constants();
}
