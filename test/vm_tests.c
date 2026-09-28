ELF_FUNCTION(vm_test_assert)
{
	elf_Value value = load_value(S, 1);
	check_numeric(S, value);
	if (!value_to_integer(value)) {
		elf_print_current_runtime_source_location(S);
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

ELF_FUNCTION(vm_test_nested_call_preserves_context)
{
	VmNestedCallContext *context = elf_get_user_data(S);
	StackFrame *native_frame_pointer = S->frame;
	StackFrame native_frame = *S->frame;
	StackFrame *caller = S->frame > S->frame_stack ? S->frame - 1 : 0;
	elf_Module *caller_module = caller ? caller->module : 0;
	BcFunction *caller_function = caller ? caller->function : 0;
	u32 caller_instruction = caller ? caller->instruction : 0;
	b32 preserved = context && !native_frame.module && !native_frame.function && caller_module && caller_function;
	if (preserved)
	{
		u32 byte_index = caller_function->offset + caller_instruction;
		preserved = BC_TYPE(caller_module->bytecode[byte_index]) == BC_CALL && elf_push_ref(S, context->callback);
	}
	if (preserved)
	{
		elf_push_nil(S);
		elf_call(S, 1, 0);
		StackFrame *restored_caller = S->frame > S->frame_stack ? S->frame - 1 : 0;
		preserved = S->frame == native_frame_pointer &&
			S->frame->framebase == native_frame.framebase && S->frame->reference == native_frame.reference &&
			S->frame->framesize == native_frame.framesize && S->frame->nargs == native_frame.nargs && S->frame->nrets == native_frame.nrets &&
			!S->frame->module && !S->frame->function && restored_caller &&
			restored_caller->module == caller_module && restored_caller->function == caller_function &&
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
	elf_open_batteries(state);
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
	vm_expect_int("test/smoke/return_int.elf", 1, "vm returns integer value");
}

static void test_vm_integer_add(void)
{
	vm_expect_int("test/smoke/return_add.elf", 3, "vm executes integer addition");
}

static void test_vm_return_nil(void)
{
	VmRunResult result = vm_test_run_file("test/smoke/return_nil.elf");
	if (!value_is_nil(result.value)) {
		test_fail("vm returns nil value");
	}
	elf_destroy_state(result.state);
}

static void test_vm_return_table(void)
{
	VmRunResult result = vm_test_run_file("test/smoke/return_table.elf");
	if (!value_is_table(result.value)) {
		test_fail("vm returns table value");
	}
	else if (elf_array_length(value_as_table(result.value)) != 2) {
		test_fail("vm table literal stores field and array values");
	}
	elf_destroy_state(result.state);
}

static void test_vm_nil_assign(void)
{
	vm_expect_int("test/smoke/nil_assign.elf", 42, "vm nil assignment stores fallback value");
}

static void test_vm_function_call(void)
{
	vm_expect_int("test/smoke/function_call.elf", 5, "vm calls user function with arguments");
}

static void test_vm_function_arguments(void)
{
	vm_expect_int("test/smoke/function_arguments.elf", 345, "vm exposes implicit this and explicit arguments");
}

static void test_vm_variadic_arguments(void)
{
	vm_expect_int("test/smoke/variadic_arguments.elf", 436, "vm exposes variadic arguments");
}

static void test_vm_closure_capture(void)
{
	vm_expect_int("test/smoke/closure_capture.elf", 17350, "vm runs closures with captured values");
}

static void test_vm_get_mem_macro(void)
{
	vm_expect_int("test/smoke/get_mem.elf", 123, "vm reports local memory slots");
}

static void test_vm_method_this(void)
{
	vm_expect_int("test/smoke/method_this.elf", 242, "vm passes field receiver as implicit this");
}

static void test_vm_script_assertions(void)
{
	vm_expect_int("test/smoke/script_assert_assignments.elf", 99, "vm runs script-side assignment assertions");
}

static void test_vm_script_logical_decl(void)
{
	vm_expect_int("test/smoke/script_assert_logical_decl.elf", 42, "vm runs logical expression declarations");
}

static void test_vm_script_table_assertions(void)
{
	vm_expect_int("test/smoke/script_assert_tables.elf", 123, "vm runs script-side table assertions");
}

static void test_vm_script_compound_assertions(void)
{
	vm_expect_int("test/smoke/script_assert_compound.elf", 46, "vm runs script-side compound assignment assertions");
}

static void test_vm_script_json_assertions(void)
{
	vm_expect_int("test/smoke/script_assert_json.elf", 321, "vm runs script-side json table assertions");
}

static void test_vm_script_sys_assertions(void)
{
	vm_expect_int("test/smoke/script_assert_sys.elf", 77, "vm runs script-side path and filesystem assertions");
}

static void test_vm_script_string_assertions(void)
{
	vm_expect_int("test/smoke/script_assert_strings.elf", 88, "vm runs counted string library assertions");
}

static void test_vm_script_table_lib_assertions(void)
{
	vm_expect_int("test/smoke/script_assert_table_lib.elf", 144, "vm runs table library assertions");
}

static void test_vm_core_library_assertions(void)
{
	vm_expect_int("test/smoke/script_assert_core_lib.elf", 4242, "vm runs core library assertions");
}

static void test_vm_for_range(void)
{
	vm_expect_int("test/smoke/for_range.elf", 276, "vm runs half-open range for loop");
}

static void test_vm_c_for(void)
{
	vm_expect_int("test/smoke/c_for.elf", 103, "vm runs C-style for loops");
}

static void test_vm_break_continue(void)
{
	vm_expect_int("test/smoke/break_continue.elf", 1813, "vm runs break and continue");
}

static void test_vm_loop_slots(void)
{
	vm_expect_int("test/smoke/loop_slots.elf", 24781, "vm restores loop slots");
}

static void test_vm_defer(void)
{
	vm_expect_int("test/smoke/defer.elf", 2114344, "vm runs deferred statements");
}

static void test_vm_string_join(void)
{
	VmRunResult result = vm_test_run_file("test/smoke/script_assert_string_join.elf");
	if (!value_is_string(result.value) || strcmp(value_as_string(result.value)->data, "score9") != 0) {
		test_fail("vm joins string lhs with formatted rhs");
	}
	elf_destroy_state(result.state);
}

static void test_vm_interpolated_strings(void)
{
	VmRunResult result = vm_test_run_file("test/smoke/interpolated_strings.elf");
	if (!value_is_string(result.value) || strcmp(value_as_string(result.value)->data, "score9") != 0) {
		test_fail("vm evaluates interpolated strings");
	}
	elf_destroy_state(result.state);
}

static void test_vm_fib(void)
{
	vm_expect_int("test/smoke/fib.elf", 55, "vm runs recursive fibonacci");
}

static void test_vm_compiled_closure_keeps_function_identity(void)
{
	elf_State *state = elf_create_state();
	char first_source_text[] = "ret fun(base) { ret fun() { ret base + 11 } }";
	char second_source_text[] = "ret fun(base) { ret fun() { ret base + 22 } }";
	elf_StrSlice first_source = {first_source_text, sizeof(first_source_text) - 1};
	elf_StrSlice second_source = {second_source_text, sizeof(second_source_text) - 1};

	elf_push_code_source(state, "first", first_source);
	elf_Closure *first_entry = value_as_closure(state->stack_ptr[-1]);
	elf_Module *first_module = first_entry->function.module;
	if (first_entry->function.index != 0) {
		test_fail("compiled module entry uses a local function index");
	}
	elf_push_nil(state);
	elf_call(state, 1, 1);
	elf_Closure *first_factory = value_as_closure(state->stack_ptr[-1]);
	if (first_factory->function.module != first_module) {
		test_fail("nested closure retains its defining module");
	}
	elf_Ref first_factory_ref = elf_create_ref(state, -1);
	elf_pop(state, 1);

	elf_push_code_source(state, "second", second_source);
	elf_Closure *second_entry = value_as_closure(state->stack_ptr[-1]);
	elf_Module *second_module = second_entry->function.module;
	if (second_module == first_module || second_entry->function.index != 0) {
		test_fail("each compilation publishes an independent module with a local entry");
	}
	elf_push_nil(state);
	elf_call(state, 1, 1);
	elf_Closure *second_factory = value_as_closure(state->stack_ptr[-1]);
	if (second_factory->function.module != second_module ||
		second_factory->function.index != first_factory->function.index)
	{
		test_fail("second nested closure retains its defining module");
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
	elf_Ref second = elf_create_ref(state, -1);
	elf_pop(state, 1);

	if (first == ELF_NO_REF || second == ELF_NO_REF ||
		first_closure->function.module != first_module ||
		second_closure->function.module != second_module ||
		first_closure->function.index != second_closure->function.index)
	{
		test_fail("capturing closures preserve overlapping module-local identities");
		elf_destroy_state(state);
		return;
	}

	force_gc_allocations(state, 256);
	if (!first_module->source_name || strcmp(string_data(first_module->source_name), "first") != 0 ||
		!second_module->source_name || strcmp(string_data(second_module->source_name), "second") != 0)
	{
		test_fail("compiled module source names survive GC");
	}

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
	if (!elf_push_code_source(state, "nested-callback", callback_source)) {
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
	if (!elf_push_code_source(state, "nested-caller", caller_source)) {
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

static void test_vm_module_builder_grows_constant_arrays(void)
{
	char source[4096];
	u32 used = (u32)snprintf(source, sizeof(source), "ret {");
	for (u32 i = 0; i < 80; ++i) used += (u32)snprintf(source + used, sizeof(source) - used, "%u,", i);
	for (u32 i = 0; i < 80; ++i) used += (u32)snprintf(source + used, sizeof(source) - used, "%u.5%s", i, i == 79 ? "}" : ",");
	if (used >= sizeof(source)) {
		test_fail("module growth test source fits its buffer");
		return;
	}

	elf_State *state = elf_create_state();
	elf_StrSlice source_slice = {source, used};
	if (!elf_push_code_source(state, "constant-growth", source_slice)) {
		test_fail("module growth source compiles");
		elf_destroy_state(state);
		return;
	}

	elf_Module *module = value_as_closure(state->stack_ptr[-1])->function.module;
	if (module->integer_constant_count <= 64 || module->number_constant_count <= 64) {
		test_fail("module builder grows both constant arrays beyond their initial capacity");
	}
	elf_push_nil(state);
	elf_call(state, 1, 1);
	if (!value_is_table(state->stack_ptr[-1]) || elf_array_length(value_as_table(state->stack_ptr[-1])) != 160) {
		test_fail("grown module constant arrays execute correctly");
	}

	elf_destroy_state(state);
}

static void test_vm_module_converts_atom_constants(void)
{
	elf_State *state = elf_create_state();
	u32 global_count = elf_array_length(state->globals);
	char source_text[] = "ret {\"same\", \"same\", \"other\"}";
	elf_StrSlice source = {source_text, sizeof(source_text) - 1};
	if (!elf_push_code_source(state, "module-strings", source)) {
		test_fail("module string source compiles");
		elf_destroy_state(state);
		return;
	}

	elf_Module *module = value_as_closure(state->stack_ptr[-1])->function.module;
	if (module->string_count != 2 || strcmp(string_data(module->strings[0]), "same") != 0 ||
		strcmp(string_data(module->strings[1]), "other") != 0)
	{
		test_fail("module converts deduplicated atom constants to strings");
	}
	if (elf_array_length(state->globals) != global_count) {
		test_fail("string constants do not consume global slots");
	}

	force_gc_allocations(state, 256);
	if (strcmp(string_data(module->strings[0]), "same") != 0 || strcmp(string_data(module->strings[1]), "other") != 0) {
		test_fail("module string constants survive GC");
	}

	elf_push_nil(state);
	elf_call(state, 1, 1);
	elf_Table *result = value_as_table(state->stack_ptr[-1]);
	if (elf_array_length(result) != 3 || !strings_equal(value_as_string(elf_array_get(state, result, 0)), module->strings[0]) ||
		!strings_equal(value_as_string(elf_array_get(state, result, 1)), module->strings[0]) ||
		!strings_equal(value_as_string(elf_array_get(state, result, 2)), module->strings[1]))
	{
		test_fail("VM loads string constants from their module");
	}

	elf_destroy_state(state);
}

static void run_vm_tests(void)
{
	test_vm_return_int();
	test_vm_integer_add();
	test_vm_return_nil();
	test_vm_return_table();
	test_vm_nil_assign();
	test_vm_function_call();
	test_vm_function_arguments();
	test_vm_variadic_arguments();
	test_vm_closure_capture();
	test_vm_get_mem_macro();
	test_vm_method_this();
	test_vm_script_assertions();
	test_vm_script_logical_decl();
	test_vm_script_table_assertions();
	test_vm_script_compound_assertions();
	test_vm_script_json_assertions();
	test_vm_script_sys_assertions();
	test_vm_script_string_assertions();
	test_vm_script_table_lib_assertions();
	test_vm_core_library_assertions();
	test_vm_for_range();
	test_vm_c_for();
	test_vm_break_continue();
	test_vm_loop_slots();
	test_vm_defer();
	test_vm_string_join();
	test_vm_interpolated_strings();
	test_vm_fib();
	test_vm_compiled_closure_keeps_function_identity();
	test_vm_nested_host_call_preserves_diagnostics_context();
	test_vm_module_builder_grows_constant_arrays();
	test_vm_module_converts_atom_constants();
}
