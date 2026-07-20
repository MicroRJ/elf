ELF_FUNCTION(vm_test_assert)
{
	elf_Value value = load_value(S, 1);
	check_value_type_rule(S, value, TRULE_NUMERIC);
	if (!value_to_integer(value)) {
		elf_print_current_runtime_source_location(S);
		test_fail("script assertion failed");
	}
	return 0;
}

static void vm_test_install_bindings(elf_State *state)
{
	elf_push_env(state);
	elf_push_cstr(state, "test_assert");
	elf_push_fun(state, vm_test_assert);
	elf_tab_set(state);
	pop_value(state);
}

static elf_Value vm_test_run_file(const char *path)
{
	elf_State *state = elf_create_state();
	vm_test_install_bindings(state);
	elf_push_code_file(state, path);
	elf_push_nil(state);
	elf_call(state, 1, 1);
	return state->stack_ptr[-1];
}

static void vm_expect_int(const char *path, i64 expected, const char *label)
{
	elf_Value value = vm_test_run_file(path);
	if (!value_is_integer(value) || value_as_integer(value) != expected) {
		fprintf(stderr, "FAIL: %s expected int %lld, got tag %d value %lld\n",
			label, expected, value_type(value), value_as_integer(value));
		test_failures += 1;
	}
}

static void test_vm_return_int(void)
{
	vm_expect_int("smoke/return_int.elf", 1, "vm returns integer value");
}

static void test_vm_integer_add(void)
{
	vm_expect_int("smoke/return_add.elf", 3, "vm executes integer addition");
}

static void test_vm_return_nil(void)
{
	elf_Value value = vm_test_run_file("smoke/return_nil.elf");
	if (!value_is_nil(value)) {
		test_fail("vm returns nil value");
	}
}

static void test_vm_return_table(void)
{
	elf_Value value = vm_test_run_file("smoke/return_table.elf");
	if (!value_is_table(value)) {
		test_fail("vm returns table value");
		return;
	}

	elf_Table *table = value_as_table(value);
	if (elf_array_length(table) != 2) {
		test_fail("vm table literal stores field and array values");
	}
}

static void test_vm_nil_assign(void)
{
	vm_expect_int("smoke/nil_assign.elf", 42, "vm nil assignment stores fallback value");
}

static void test_vm_function_call(void)
{
	vm_expect_int("smoke/function_call.elf", 5, "vm calls user function with arguments");
}

static void test_vm_function_arguments(void)
{
	vm_expect_int("smoke/function_arguments.elf", 345, "vm exposes implicit this and explicit arguments");
}

static void test_vm_variadic_arguments(void)
{
	vm_expect_int("smoke/variadic_arguments.elf", 436, "vm exposes variadic arguments");
}

static void test_vm_closure_capture(void)
{
	vm_expect_int("smoke/closure_capture.elf", 17350, "vm runs closures with captured values");
}

static void test_vm_get_mem_macro(void)
{
	vm_expect_int("smoke/get_mem.elf", 123, "vm reports local memory slots");
}

static void test_vm_method_this(void)
{
	vm_expect_int("smoke/method_this.elf", 242, "vm passes field receiver as implicit this");
}

static void test_vm_script_assertions(void)
{
	vm_expect_int("smoke/script_assert_assignments.elf", 99, "vm runs script-side assignment assertions");
}

static void test_vm_script_logical_decl(void)
{
	vm_expect_int("smoke/script_assert_logical_decl.elf", 42, "vm runs logical expression declarations");
}

static void test_vm_script_table_assertions(void)
{
	vm_expect_int("smoke/script_assert_tables.elf", 123, "vm runs script-side table assertions");
}

static void test_vm_script_compound_assertions(void)
{
	vm_expect_int("smoke/script_assert_compound.elf", 46, "vm runs script-side compound assignment assertions");
}

static void test_vm_script_json_assertions(void)
{
	vm_expect_int("smoke/script_assert_json.elf", 321, "vm runs script-side json table assertions");
}

static void test_vm_script_sys_assertions(void)
{
	vm_expect_int("smoke/script_assert_sys.elf", 77, "vm runs script-side path and filesystem assertions");
}

static void test_vm_script_string_assertions(void)
{
	vm_expect_int("smoke/script_assert_strings.elf", 88, "vm runs counted string library assertions");
}

static void test_vm_script_table_lib_assertions(void)
{
	vm_expect_int("smoke/script_assert_table_lib.elf", 144, "vm runs table library assertions");
}

static void test_vm_for_range(void)
{
	vm_expect_int("smoke/for_range.elf", 276, "vm runs half-open range for loop");
}

static void test_vm_for_steps(void)
{
	vm_expect_int("smoke/for_steps.elf", 71, "vm runs replicated for steps");
}

static void test_vm_break_continue(void)
{
	vm_expect_int("smoke/break_continue.elf", 1813, "vm runs break and continue");
}

static void test_vm_loop_slots(void)
{
	vm_expect_int("smoke/loop_slots.elf", 24781, "vm restores loop slots");
}

static void test_vm_defer(void)
{
	vm_expect_int("smoke/defer.elf", 2114344, "vm runs deferred statements");
}

static void test_vm_atom_join(void)
{
	elf_Value value = vm_test_run_file("smoke/script_assert_atom_join.elf");
	if (!value_is_atom(value) || strcmp(value_as_atom(value)->data, "score9") != 0) {
		test_fail("vm joins atom lhs with formatted rhs");
	}
}

static void test_vm_interpolated_strings(void)
{
	elf_Value value = vm_test_run_file("smoke/interpolated_strings.elf");
	if (!value_is_atom(value) || strcmp(value_as_atom(value)->data, "score9") != 0) {
		test_fail("vm evaluates interpolated strings");
	}
}

static void test_vm_fib(void)
{
	vm_expect_int("smoke/fib.elf", 55, "vm runs recursive fibonacci");
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
	test_vm_for_range();
	test_vm_for_steps();
	test_vm_break_continue();
	test_vm_loop_slots();
	test_vm_defer();
	test_vm_atom_join();
	test_vm_interpolated_strings();
	test_vm_fib();
}
