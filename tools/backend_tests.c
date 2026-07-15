typedef struct
{
	elf_State       *state;
	BytecodeFunction function;
}
BackendCompileResult;

static BackendCompileResult backend_test_compile_file(const char *path)
{
	BackendCompileResult result = {};
	result.state = elf_create_state();

	elf_StrSlice source = {};
	elf_Handle file = elf_platform_access_file(path, SYS_OPEN_READ, SYS_OPEN_EXISTING);
	if (file)
	{
		u64 size = elf_platform_get_file_size(file);
		char *data = arena_push(&result.state->arena, size + 16);
		zero_memory(data + size, 16);
		elf_platform_read_file(file, data, (u32)size);
		elf_platform_close_file(file);

		source.data = data;
		source.size = size;
	}

	result.function = elf_compile_source(result.state, path, source);
	return result;
}

static BackendCompileResult backend_test_compile_source(const char *source_text)
{
	BackendCompileResult result = {};
	result.state = elf_create_state();

	elf_StrSlice source = {};
	source.data = (char *)source_text;
	source.size = (u64)strlen(source_text);

	result.function = elf_compile_source(result.state, "backend_tests", source);
	return result;
}

static Bytecode *backend_test_bytes(BackendCompileResult result)
{
	return result.state->bytecode + result.function.offset;
}

static u32 backend_count_bytecode(BackendCompileResult result, BytecodeType type)
{
	u32 count = 0;
	Bytecode *bytes = backend_test_bytes(result);
	for (u32 i = 0; i < result.function.length; ++ i)
	{
		if (bytes[i].b_type == type) {
			count += 1;
		}
	}
	return count;
}

static SourceMapEntry *backend_find_source_map_entry(BytecodeFunction *function, u32 byte)
{
	for (u32 i = 0; i < function->source_map_count; ++ i) {
		SourceMapEntry *entry = &function->source_map[i];
		if (byte >= entry->byte_start && byte < entry->byte_end) {
			return entry;
		}
	}
	return 0;
}

static void test_backend_source_map(void)
{
	BackendCompileResult result = backend_test_compile_file("smoke/return_add.elf");
	BytecodeFunction *function = &result.function;
	if (!function->source_name || !function->source_data || function->source_size == 0) {
		test_fail("backend stores source data on bytecode function");
		return;
	}
	if (function->source_map_count == 0) {
		test_fail("backend emits source map entries");
		return;
	}

	for (u32 i = 0; i < result.function.length; ++ i) {
		u32 byte = result.function.offset + i;
		SourceMapEntry *entry = backend_find_source_map_entry(function, byte);
		if (!entry || !entry->site.data || !entry->site.line_start || entry->site.size == 0) {
			test_fail("backend maps each bytecode instruction to source");
			return;
		}
	}
}

static void backend_expect_no_eager_logical_bytecode(BackendCompileResult result, const char *label)
{
	if (backend_count_bytecode(result, BC_BIT_AND) != 0) {
		test_fail(label);
	}
	if (backend_count_bytecode(result, BC_BIT_OR) != 0) {
		test_fail(label);
	}
}

static void backend_expect_patched_forward_jumps(BackendCompileResult result, const char *label)
{
	Bytecode *bytes = backend_test_bytes(result);
	for (u32 i = 0; i < result.function.length; ++ i)
	{
		Bytecode byte = bytes[i];
		if (byte.b_type == BC_JUMP ||
			byte.b_type == BC_JZ ||
			byte.b_type == BC_JNZ)
		{
			i32 target = (i32)i + byte.b_x;
			if (byte.b_x <= 0 || target <= (i32)i || target > (i32)result.function.length) {
				test_fail(label);
				return;
			}
		}
	}
}

static void backend_expect_loop_jumps(BackendCompileResult result, u32 expected_backward_jumps, const char *label)
{
	u32 backward_jumps = 0;
	Bytecode *bytes = backend_test_bytes(result);
	for (u32 i = 0; i < result.function.length; ++ i)
	{
		Bytecode byte = bytes[i];
		if (byte.b_type == BC_JUMP)
		{
			i32 target = (i32)i + byte.b_x;
			if (byte.b_x >= 0 || target < 0 || target >= (i32)result.function.length) {
				test_fail(label);
				return;
			}
			backward_jumps += 1;
		}
		else if (byte.b_type == BC_JZ ||
			byte.b_type == BC_JNZ)
		{
			i32 target = (i32)i + byte.b_x;
			if (byte.b_x <= 0 || target <= (i32)i || target > (i32)result.function.length) {
				test_fail(label);
				return;
			}
		}
	}

	if (backward_jumps != expected_backward_jumps) {
		test_fail(label);
	}
}

static i32 backend_find_first_bytecode(BackendCompileResult result, BytecodeType type)
{
	Bytecode *bytes = backend_test_bytes(result);
	for (u32 i = 0; i < result.function.length; ++ i)
	{
		if (bytes[i].b_type == type) {
			return i;
		}
	}
	return -1;
}

static void test_backend_short_circuit_and(void)
{
	BackendCompileResult result = backend_test_compile_file("smoke/short_circuit_and.elf");

	if (backend_count_bytecode(result, BC_JZ) != 2) {
		test_fail("&& emits one false jump per operand");
	}
	if (backend_count_bytecode(result, BC_JNZ) != 0) {
		test_fail("&& does not emit true short-circuit jumps");
	}
	backend_expect_no_eager_logical_bytecode(result, "&& does not lower to eager logical bytecode");
	backend_expect_patched_forward_jumps(result, "&& patches condition jumps forward");
}

static void test_backend_short_circuit_or(void)
{
	BackendCompileResult result = backend_test_compile_file("smoke/short_circuit_or.elf");

	if (backend_count_bytecode(result, BC_JNZ) != 1) {
		test_fail("|| emits true short-circuit jump for first operand");
	}
	if (backend_count_bytecode(result, BC_JZ) != 1) {
		test_fail("|| emits false jump for final operand");
	}
	backend_expect_no_eager_logical_bytecode(result, "|| does not lower to eager logical bytecode");
	backend_expect_patched_forward_jumps(result, "|| patches condition jumps forward");
}

static void test_backend_short_circuit_nested_and_or(void)
{
	BackendCompileResult result = backend_test_compile_file("smoke/short_circuit_nested_and_or.elf");

	if (backend_count_bytecode(result, BC_JZ) != 2) {
		test_fail("nested a && (b || c) emits two false jumps");
	}
	if (backend_count_bytecode(result, BC_JNZ) != 1) {
		test_fail("nested a && (b || c) emits one true short-circuit jump");
	}
	backend_expect_no_eager_logical_bytecode(result, "nested a && (b || c) does not lower to eager logical bytecode");
	backend_expect_patched_forward_jumps(result, "nested a && (b || c) patches jumps forward");
}

static void test_backend_short_circuit_nested_or_and(void)
{
	BackendCompileResult result = backend_test_compile_file("smoke/short_circuit_nested_or_and.elf");

	if (backend_count_bytecode(result, BC_JNZ) != 1) {
		test_fail("nested a || (b && c) emits one true short-circuit jump");
	}
	if (backend_count_bytecode(result, BC_JZ) != 2) {
		test_fail("nested a || (b && c) emits two false jumps");
	}
	backend_expect_no_eager_logical_bytecode(result, "nested a || (b && c) does not lower to eager logical bytecode");
	backend_expect_patched_forward_jumps(result, "nested a || (b && c) patches jumps forward");
}

static void test_backend_short_circuit_mixed_groups(void)
{
	BackendCompileResult result = backend_test_compile_file("smoke/short_circuit_mixed_groups.elf");

	if (backend_count_bytecode(result, BC_JNZ) != 2) {
		test_fail("mixed (a || b) && (c || d) emits two true short-circuit jumps");
	}
	if (backend_count_bytecode(result, BC_JZ) != 2) {
		test_fail("mixed (a || b) && (c || d) emits two false jumps");
	}
	backend_expect_no_eager_logical_bytecode(result, "mixed (a || b) && (c || d) does not lower to eager logical bytecode");
	backend_expect_patched_forward_jumps(result, "mixed (a || b) && (c || d) patches jumps forward");
}

static void test_backend_while_loop(void)
{
	BackendCompileResult result = backend_test_compile_file("smoke/while_loop.elf");

	if (backend_count_bytecode(result, BC_JZ) != 1) {
		test_fail("while emits one loop-exit conditional jump");
	}
	if (backend_count_bytecode(result, BC_JUMP) != 1) {
		test_fail("while emits one loop-back jump");
	}
	backend_expect_loop_jumps(result, 1, "while patches exit and loop-back jumps");
}

static void test_backend_while_short_circuit_loop(void)
{
	BackendCompileResult result = backend_test_compile_file("smoke/while_short_circuit.elf");

	if (backend_count_bytecode(result, BC_JZ) != 2) {
		test_fail("while short-circuit predicate emits two false jumps");
	}
	if (backend_count_bytecode(result, BC_JUMP) != 1) {
		test_fail("while short-circuit predicate emits one loop-back jump");
	}
	backend_expect_loop_jumps(result, 1, "while short-circuit patches exit and loop-back jumps");
}

static void test_backend_range_for_loop_shape(void)
{
	BackendCompileResult result = backend_test_compile_source(
		"sum := 0\n"
		"for i := 0 ... 10 ? {\n"
		"	sum += i\n"
		"}\n"
		"ret sum\n");

	if (backend_count_bytecode(result, BC_JZ) != 1) {
		test_fail("range for emits one loop-exit conditional jump");
	}
	if (backend_count_bytecode(result, BC_JUMP) != 1) {
		test_fail("range for emits one loop-back jump");
	}
	if (backend_count_bytecode(result, BC_CALL) != 0) {
		test_fail("plain range for does not emit calls");
	}
	if (backend_count_bytecode(result, BC_GETINDEX) != 0) {
		test_fail("plain range for does not index a collection");
	}
	if (backend_count_bytecode(result, BC_LOADNIL) != 0) {
		test_fail("range for does not nil-initialize loop variables");
	}
	backend_expect_loop_jumps(result, 1, "range for patches exit and loop-back jumps");
}

static void test_backend_range_for_collection_call_is_hoisted(void)
{
	BackendCompileResult result = backend_test_compile_source(
		"values := fun() { ret { 1, 2, 3 } }\n"
		"sum := 0\n"
		"for item := values()[0 ... 3] ? {\n"
		"	sum += item\n"
		"}\n"
		"ret sum\n");

	if (backend_count_bytecode(result, BC_CALL) != 1) {
		test_fail("range collection expression is evaluated once before the loop");
	}
	if (backend_count_bytecode(result, BC_GETINDEX) != 1) {
		test_fail("single-variable collection range for emits one indexed load in loop body");
	}

	i32 call = backend_find_first_bytecode(result, BC_CALL);
	i32 loop_exit = backend_find_first_bytecode(result, BC_JZ);
	if (call < 0 || loop_exit < 0 || call >= loop_exit) {
		test_fail("range collection call happens before loop condition");
	}
	backend_expect_loop_jumps(result, 1, "collection range for patches exit and loop-back jumps");
}

static void test_backend_local_initializer_reuses_result_slot(void)
{
	BackendCompileResult result = backend_test_compile_source(
		"b := 1\n"
		"c := 2\n"
		"a := (b + c) + (b + c)\n"
		"ret a\n");

	if (result.function.stack_size != 5) {
		test_fail("local initializer adopts expression result slot");
	}
}

static void test_backend_formats_bytecode_function(void)
{
	BackendCompileResult result = backend_test_compile_source(
		"x := 41\n"
		"ret x + 1\n");

	Scratch scratch = get_scratch();
	char *text = format_bytecode_function(result.state, scratch.arena, result.function);
	if (!strstr(text, "bytecode function")) {
		test_fail("bytecode formatter prints function header");
	}
	if (!strstr(text, "stack_size")) {
		test_fail("bytecode formatter prints function metadata");
	}
	if (!strstr(text, "integer[")) {
		test_fail("bytecode formatter prints integer constants");
	}
	if (!strstr(text, "return")) {
		test_fail("bytecode formatter prints return bytecode");
	}
	end_scratch(scratch);
}

static void run_backend_tests(void)
{
	test_backend_source_map();
	test_backend_short_circuit_and();
	test_backend_short_circuit_or();
	test_backend_short_circuit_nested_and_or();
	test_backend_short_circuit_nested_or_and();
	test_backend_short_circuit_mixed_groups();
	test_backend_while_loop();
	test_backend_while_short_circuit_loop();
	test_backend_range_for_loop_shape();
	test_backend_range_for_collection_call_is_hoisted();
	test_backend_local_initializer_reuses_result_slot();
	test_backend_formats_bytecode_function();
}
