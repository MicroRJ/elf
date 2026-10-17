//
// See Copyright Notice In elf.h
//

#include <dy.h>

typedef struct
{
	elf_State       *state;
	BcFunctionRef function;
}
BackendCompileResult;

static BcFunction *backend_test_function(BackendCompileResult result)
{
	return bc_function_from_ref(result.function);
}

static BcFunctionRef backend_test_compile(elf_State *state, const char *name, elf_StrSlice source)
{
	elf_Scratch scratch = elf_begin_scratch();
	elf_StrSlice source_name = {(char *)name, strlen(name)};
	BcModule *compiled_module = compiler_compile(scratch.arena, source_name, source);
	elf_Module *module = elf_link_module(state, compiled_module);
	BcFunctionRef entry = {};
	if (module)
	{
		entry.module = module;
		entry.index = 0;
	}
	elf_end_scratch(scratch);
	return entry;
}

static BackendCompileResult backend_test_compile_file(const char *path)
{
	BackendCompileResult result = {};
	result.state = elf_create_state();

	elf_StrSlice source = {};
	dy_File_Info info;
	dy_File file = {0};
	if (!dy_get_file_info(dy_string_from_cstring(path), &info).error && !info.is_directory)
		dy_access_file(dy_string_from_cstring(path), DY_FILE_OPEN_EXISTING,
			DY_FILE_READ | DY_FILE_SHARE_READ, &file);
	if (dy_file_is_valid(file))
	{
		u64 size = info.size;
		char *data = elf_arena_push(&result.state->arena, size + 16);
		zero_memory(data + size, 16);
		dy_u64 read = 0;
		dy_Result read_result = dy_read_file(file, data, size, &read);
		dy_close_file(file);
		if (read_result.error || read != size) size = 0;

		source.data = data;
		source.size = size;
	}

	result.function = backend_test_compile(result.state, path, source);
	return result;
}

static BackendCompileResult backend_test_compile_source(const char *source_text)
{
	BackendCompileResult result = {};
	result.state = elf_create_state();

	elf_StrSlice source = {};
	source.data = (char *)source_text;
	source.size = (u64)strlen(source_text);

	result.function = backend_test_compile(result.state, "backend_tests", source);
	return result;
}

static Bytecode *backend_test_bytes(BackendCompileResult result)
{
	BcFunction *function = backend_test_function(result);
	return result.function.module->bytecode + function->offset;
}

static void backend_test_destroy(BackendCompileResult result)
{
	elf_destroy_state(result.state);
}

static u32 backend_count_bytecode(BackendCompileResult result, BcType type)
{
	u32 count = 0;
	BcFunction *function = backend_test_function(result);
	Bytecode *bytes = backend_test_bytes(result);
	for (u32 i = 0; i < function->length; ++ i)
	{
		if (bytes[i].b_type == type) {
			count += 1;
		}
	}
	return count;
}

static BcSourceMapEntry *backend_find_source_map_entry(elf_Module *module, u32 byte)
{
	for (u32 i = 0; i < module->source_map_count; ++ i) {
		BcSourceMapEntry *entry = &module->source_map[i];
		if (byte >= entry->byte_start && byte < entry->byte_end) {
			return entry;
		}
	}
	return 0;
}

static void test_backend_source_map(void)
{
	BackendCompileResult result = backend_test_compile_file("test/smoke/return_add.elf");
	BcFunction *function = backend_test_function(result);
	elf_Module *module = result.function.module;
	if (!module->source_name || !module->source_data || module->source_size == 0) {
		test_fail("backend stores source data on compiled module");
		goto done;
	}
	if (module->source_map_count == 0) {
		test_fail("backend emits source map entries");
		goto done;
	}

	for (u32 i = 0; i < function->length; ++ i) {
		u32 byte = function->offset + i;
		BcSourceMapEntry *entry = backend_find_source_map_entry(module, byte);
		if (!entry || !source_slice_is_valid(entry->site) || entry->site.size == 0) {
			test_fail("backend maps each bytecode instruction to source");
			goto done;
		}
	}

done:
	backend_test_destroy(result);
}

static void test_backend_module_owns_source(void)
{
	static const char expected[] = "value := 41\nret value + 1\n";
	u32 source_size = sizeof(expected) - 1;
	char *source_data = malloc(source_size);
	copy_memory(source_data, expected, source_size);

	elf_State *state = elf_create_state();
	elf_StrSlice source = {source_data, source_size};
	BcFunctionRef entry = backend_test_compile(state, "owned_source", source);
	elf_Module *module = entry.module;
	if (module->source_data == source_data) {
		test_fail("compiled module does not borrow caller source memory");
		free(source_data);
		elf_destroy_state(state);
		return;
	}

	zero_memory(source_data, source_size);
	free(source_data);
	if (module->source_size != source_size || memcmp(module->source_data, expected, source_size) != 0) {
		test_fail("compiled module retains an owned source copy");
	}

	for (u32 map_index = 0; map_index < module->source_map_count; ++map_index)
	{
		SourceSite site = module->source_map[map_index].site;
		if (!source_slice_is_valid(site) || site.offset > module->source_size || site.line_offset > module->source_size || site.size > module->source_size - site.offset)
		{
			test_fail("source map offsets fit module-owned source memory");
			elf_destroy_state(state);
			return;
		}
	}

	elf_destroy_state(state);
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
	BcFunction *function = backend_test_function(result);
	Bytecode *bytes = backend_test_bytes(result);
	for (u32 i = 0; i < function->length; ++ i)
	{
		Bytecode byte = bytes[i];
		if (byte.b_type == BC_JUMP ||
			byte.b_type == BC_JZ ||
			byte.b_type == BC_JNZ)
		{
			i32 target = (i32)i + byte.b_x;
			if (byte.b_x <= 0 || target <= (i32)i || target > (i32)function->length) {
				test_fail(label);
				return;
			}
		}
	}
}

static void backend_expect_min_conditional_jumps(BackendCompileResult result, u32 min_count, const char *label)
{
	u32 count = backend_count_bytecode(result, BC_JZ) + backend_count_bytecode(result, BC_JNZ);
	if (count < min_count) {
		test_fail(label);
	}
}

static void backend_expect_jump_counts(
	BackendCompileResult result,
	u32 expected_jz,
	u32 expected_jnz,
	u32 expected_jump,
	const char *label)
{
	if (backend_count_bytecode(result, BC_JZ) != expected_jz ||
		backend_count_bytecode(result, BC_JNZ) != expected_jnz ||
		backend_count_bytecode(result, BC_JUMP) != expected_jump)
	{
		test_fail(label);
	}
}

static void backend_expect_loop_jumps(BackendCompileResult result, u32 expected_backward_jumps, const char *label)
{
	u32 backward_jumps = 0;
	BcFunction *function = backend_test_function(result);
	Bytecode *bytes = backend_test_bytes(result);
	for (u32 i = 0; i < function->length; ++ i)
	{
		Bytecode byte = bytes[i];
		if (byte.b_type == BC_JUMP)
		{
			i32 target = (i32)i + byte.b_x;
			if (byte.b_x >= 0 || target < 0 || target >= (i32)function->length) {
				test_fail(label);
				return;
			}
			backward_jumps += 1;
		}
		else if (byte.b_type == BC_JZ ||
			byte.b_type == BC_JNZ)
		{
			i32 target = (i32)i + byte.b_x;
			if (byte.b_x <= 0 || target <= (i32)i || target > (i32)function->length) {
				test_fail(label);
				return;
			}
		}
	}

	if (backward_jumps != expected_backward_jumps) {
		test_fail(label);
	}
}

static i32 backend_find_first_bytecode(BackendCompileResult result, BcType type)
{
	BcFunction *function = backend_test_function(result);
	Bytecode *bytes = backend_test_bytes(result);
	for (u32 i = 0; i < function->length; ++ i)
	{
		if (bytes[i].b_type == type) {
			return i;
		}
	}
	return -1;
}

static void test_backend_short_circuit_and(void)
{
	BackendCompileResult result = backend_test_compile_file("test/smoke/short_circuit_and.elf");

	backend_expect_min_conditional_jumps(result, 2, "&& emits conditional jumps for short-circuiting");
	backend_expect_jump_counts(result, 2, 0, 0, "&& emits optimal short-circuit jump shape");
	backend_expect_no_eager_logical_bytecode(result, "&& does not lower to eager logical bytecode");
	backend_expect_patched_forward_jumps(result, "&& patches condition jumps forward");
	backend_test_destroy(result);
}

static void test_backend_short_circuit_or(void)
{
	BackendCompileResult result = backend_test_compile_file("test/smoke/short_circuit_or.elf");

	backend_expect_min_conditional_jumps(result, 2, "|| emits conditional jumps for short-circuiting");
	backend_expect_jump_counts(result, 1, 1, 0, "|| emits optimal short-circuit jump shape");
	backend_expect_no_eager_logical_bytecode(result, "|| does not lower to eager logical bytecode");
	backend_expect_patched_forward_jumps(result, "|| patches condition jumps forward");
	backend_test_destroy(result);
}

static void test_backend_short_circuit_nested_and_or(void)
{
	BackendCompileResult result = backend_test_compile_file("test/smoke/short_circuit_nested_and_or.elf");

	backend_expect_min_conditional_jumps(result, 3, "nested a && (b || c) emits conditional jumps for short-circuiting");
	backend_expect_jump_counts(result, 2, 1, 0, "nested a && (b || c) emits optimal short-circuit jump shape");
	backend_expect_no_eager_logical_bytecode(result, "nested a && (b || c) does not lower to eager logical bytecode");
	backend_expect_patched_forward_jumps(result, "nested a && (b || c) patches jumps forward");
	backend_test_destroy(result);
}

static void test_backend_short_circuit_nested_or_and(void)
{
	BackendCompileResult result = backend_test_compile_file("test/smoke/short_circuit_nested_or_and.elf");

	backend_expect_min_conditional_jumps(result, 3, "nested a || (b && c) emits conditional jumps for short-circuiting");
	backend_expect_jump_counts(result, 2, 1, 0, "nested a || (b && c) emits optimal short-circuit jump shape");
	backend_expect_no_eager_logical_bytecode(result, "nested a || (b && c) does not lower to eager logical bytecode");
	backend_expect_patched_forward_jumps(result, "nested a || (b && c) patches jumps forward");
	backend_test_destroy(result);
}

static void test_backend_short_circuit_mixed_groups(void)
{
	BackendCompileResult result = backend_test_compile_file("test/smoke/short_circuit_mixed_groups.elf");

	backend_expect_min_conditional_jumps(result, 4, "mixed (a || b) && (c || d) emits conditional jumps for short-circuiting");
	backend_expect_jump_counts(result, 2, 2, 0, "mixed (a || b) && (c || d) emits optimal short-circuit jump shape");
	backend_expect_no_eager_logical_bytecode(result, "mixed (a || b) && (c || d) does not lower to eager logical bytecode");
	backend_expect_patched_forward_jumps(result, "mixed (a || b) && (c || d) patches jumps forward");
	backend_test_destroy(result);
}

static void test_backend_while_loop(void)
{
	BackendCompileResult result = backend_test_compile_file("test/smoke/while_loop.elf");

	if (backend_count_bytecode(result, BC_JZ) != 1) {
		test_fail("while emits one loop-exit conditional jump");
	}
	if (backend_count_bytecode(result, BC_JUMP) != 1) {
		test_fail("while emits one loop-back jump");
	}
	backend_expect_loop_jumps(result, 1, "while patches exit and loop-back jumps");
	backend_test_destroy(result);
}

static void test_backend_while_short_circuit_loop(void)
{
	BackendCompileResult result = backend_test_compile_file("test/smoke/while_short_circuit.elf");

	if (backend_count_bytecode(result, BC_JZ) != 2) {
		test_fail("while short-circuit predicate emits two false jumps");
	}
	if (backend_count_bytecode(result, BC_JUMP) != 1) {
		test_fail("while short-circuit predicate emits one loop-back jump");
	}
	backend_expect_jump_counts(result, 2, 0, 1, "while short-circuit predicate emits optimal jump shape");
	backend_expect_loop_jumps(result, 1, "while short-circuit patches exit and loop-back jumps");
	backend_test_destroy(result);
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
	if (backend_count_bytecode(result, BC_CALL) != 0 || backend_count_bytecode(result, BC_DYNAMIC_CALL) != 0) {
		test_fail("plain range for does not emit calls");
	}
	if (backend_count_bytecode(result, BC_GETINDEX) != 0) {
		test_fail("plain range for does not index a collection");
	}
	if (backend_count_bytecode(result, BC_LOADNIL) != 0) {
		test_fail("range for does not nil-initialize loop variables");
	}
	backend_expect_loop_jumps(result, 1, "range for patches exit and loop-back jumps");
	backend_test_destroy(result);
}

static void test_backend_range_for_collection_call_is_hoisted(void)
{
	BackendCompileResult result = backend_test_compile_source(
		"values := fun() { ret [1, 2, 3] }\n"
		"sum := 0\n"
		"for item := values()[0 ... 3] ? {\n"
		"	sum += item\n"
		"}\n"
		"ret sum\n");

	if (backend_count_bytecode(result, BC_DYNAMIC_CALL) != 1) {
		test_fail("range collection expression is evaluated once before the loop");
	}
	if (backend_count_bytecode(result, BC_GETINDEX) != 1) {
		test_fail("single-variable collection range for emits one indexed load in loop body");
	}

	i32 call = backend_find_first_bytecode(result, BC_DYNAMIC_CALL);
	i32 loop_exit = backend_find_first_bytecode(result, BC_JZ);
	if (call < 0 || loop_exit < 0 || call >= loop_exit) {
		test_fail("range collection call happens before loop condition");
	}
	backend_expect_loop_jumps(result, 1, "collection range for patches exit and loop-back jumps");
	backend_test_destroy(result);
}

static void test_backend_array_bytecode(void)
{
	BackendCompileResult result = backend_test_compile_source("values := [1, 2, 3]\nvalues[1] = 9\nret values[1]");
	if (backend_count_bytecode(result, BC_ARRAY) != 1 || backend_count_bytecode(result, BC_ARRAYADD) != 3)
	{
		test_fail("array literal emits array allocation and append bytecodes");
	}
	if (backend_count_bytecode(result, BC_SETINDEX) != 1 || backend_count_bytecode(result, BC_GETINDEX) != 1)
	{
		test_fail("array indexed access emits generic index bytecodes");
	}
	if (backend_count_bytecode(result, BC_TABLE) != 0)
	{
		test_fail("array literal does not emit table compatibility bytecodes");
	}
	backend_test_destroy(result);

	result = backend_test_compile_source("ret {answer = 1}");
	if (backend_count_bytecode(result, BC_TABLE) != 1 || backend_count_bytecode(result, BC_SETFIELD) != 1 || backend_count_bytecode(result, BC_ARRAYADD) != 0)
	{
		test_fail("keyed table literal emits only table field bytecodes");
	}
	backend_test_destroy(result);
}

static void test_backend_local_initializer_reuses_result_slot(void)
{
	BackendCompileResult result = backend_test_compile_source(
		"b := 1\n"
		"c := 2\n"
		"a := (b + c) + (b + c)\n"
		"ret a\n");

	if (backend_test_function(result)->stack_size != 5) {
		test_fail("local initializer adopts expression result slot");
	}
	backend_test_destroy(result);
}

static void test_backend_truthy_or_reuses_left_result_slot(void)
{
	BackendCompileResult result = backend_test_compile_source(
		"b := 1\n"
		"c := 2\n"
		"a := ((b + c) + (b + c)) ?? 99\n"
		"ret a\n");

	if (backend_test_function(result)->stack_size != 5) {
		test_fail("truthy-or initializer adopts left expression result slot");
	}
	backend_test_destroy(result);
}

static void test_backend_logical_expr_delays_result_slot(void)
{
	BackendCompileResult and_result = backend_test_compile_source(
		"b := 1\n"
		"c := 2\n"
		"a := ((b + c) + (b + c)) && 1\n"
		"ret a\n");

	if (backend_test_function(and_result)->stack_size != 5) {
		test_fail("logical-and initializer delays boolean result slot allocation");
	}

	BackendCompileResult or_result = backend_test_compile_source(
		"b := 1\n"
		"c := 2\n"
		"a := ((b + c) + (b + c)) || 0\n"
		"ret a\n");

	if (backend_test_function(or_result)->stack_size != 5) {
		test_fail("logical-or initializer delays boolean result slot allocation");
	}
	backend_test_destroy(and_result);
	backend_test_destroy(or_result);
}

#if 0
static void test_backend_if_else_restores_stack_top(void)
{
	elf_State *state = elf_create_state();
	elf_Scratch scratch = elf_begin_scratch();
	Compiler *compiler = compiler_create(scratch.arena);
	compiler_add_root_source(compiler, "backend_tests", (elf_StrSlice) {});
	IrModule *ir_module = ir_new_module(compiler);
	IrEmitter *emitter = ir_new_function_emitter(ir_module);
	SourceSite site = {};

	Ir pred = ir_new_int(emitter, site, 0);
	Ir true_clause = ir_new_local(emitter, site, ir_new_int(emitter, site, 11));
	Ir else_clause = ir_new_local(emitter, site, ir_new_int(emitter, site, 22));
	Ir if_ir = ir_new_if(emitter, site, pred, true_clause, else_clause);
	Ir after_if = ir_new_local(emitter, site, ir_new_int(emitter, site, 33));

	IrCollector stats = begin_ir_frame(emitter);
	ir_push(&stats, if_ir);
	ir_push(&stats, after_if);
	Ir body = ir_new_block(emitter, site, close_ir_frame(&stats));

	IrFunction function = {};
	function.site = site;
	function.variadic = true;
	Ir params[] = { ir_new_local(emitter, site, 0) };
	function.params = params;
	function.param_count = ARRAY_COUNT(params);
	function.body = body;

	BcEmitter *module = bc_create_module_emitter(compiler, 1);
	BcFunctionEmitter *emitter = bc_create_function_emitter(module);
	bc_emit_function(emitter, &function);
	if (module->generated_function_count != 1) {
		test_fail("function generation appends one bytecode function to its module");
	}

	if (after_if->ir_local.slot.slot != IMPLICIT_PARAM_COUNT) {
		test_fail("if/else restores stack top before following statement");
	}

	elf_end_scratch(scratch);
	elf_destroy_state(state);
}
#endif

static void test_backend_formats_bytecode_function(void)
{
	BackendCompileResult result = backend_test_compile_source(
		"x := 41\n"
		"ret x + 1\n");

	elf_Scratch scratch = elf_begin_scratch();
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
	elf_end_scratch(scratch);
	backend_test_destroy(result);
}

static void test_backend_outputs_strings(void)
{
	elf_Arena arena = elf_arena_create(0);
	const char text[] = "backend string";
	Atom *atom = elf_arena_push_zero(&arena, sizeof(*atom) + sizeof(text));
	atom->size = sizeof(text) - 1;
	copy_memory(atom->data, text, sizeof(text));

	IrNode string = {.kind = IR_ATOM, .atom = atom};
	IrNode return_string = {.kind = IR_RETURN, .ir_return.expr = &string};
	IrFunction function = {.body = &return_string};
	IrModule ir_module = {.arena = &arena, .functions = &function, .function_count = 1, .function_capacity = 1};

	BcModule *module = bc_emit_module(&ir_module);
	if (module->string_count != 1 || module->strings[0].size != sizeof(text) - 1 || memcmp(module->strings[0].data, text, sizeof(text))) {
		test_fail("backend outputs owned bytecode strings");
	}
	if (module->strings[0].data == atom->data) {
		test_fail("backend bytecode strings do not borrow compiler atoms");
	}

	elf_arena_destroy(&arena);
}

static void test_backend_compiles_static_functions(void)
{
	BackendCompileResult result = backend_test_compile_source(
		"foo :: fun() { ret bar() }\n"
		"bar :: fun() { ret foo() }\n"
		"ret foo\n");

	if (!bc_function_ref_is_valid(result.function)) {
		test_fail("backend compiles mutually recursive static functions");
	}
	else if (result.function.module->bytecode_function_count != 3) {
		test_fail("backend emits both mutually recursive static functions");
	}
	backend_test_destroy(result);
}

static void test_backend_emits_direct_static_call(void)
{
	BackendCompileResult result = backend_test_compile_source(
		"add :: fun(a, b) { ret a + b }\n"
		"ret add(2, 3)\n");

	if (backend_count_bytecode(result, BC_CALL) != 1 || backend_count_bytecode(result, BC_DYNAMIC_CALL) != 0) {
		test_fail("static function call emits one direct call");
	}
	if (backend_count_bytecode(result, BC_CLOSURE) != 0) {
		test_fail("direct static function call does not allocate a closure");
	}

	i32 call_index = backend_find_first_bytecode(result, BC_CALL);
	if (call_index >= 0) {
		Bytecode call = backend_test_bytes(result)[call_index];
		if (call.b_y < 0 || (u32)call.b_y >= result.function.module->bytecode_function_count || call.b_z != 3 || call.b_w != 1) {
			test_fail("direct static call encodes function, argument, and return counts");
		}
	}
	backend_test_destroy(result);
}

static void test_backend_lowers_trailing_closure(void)
{
	BackendCompileResult result = backend_test_compile_source(
		"apply :: fun(value, increment = 1, callback) { ret callback(value + increment) }\n"
		"offset := 1\n"
		"ret apply(value = 40) -> (value) { ret value + offset }\n");
	if (!bc_function_ref_is_valid(result.function))
	{
		test_fail("trailing closure compiles");
	}
	else
	{
		elf_Closure *closure = elf_gc_alloc(result.state, ELF_OBJECT_CLOSURE, sizeof(*closure));
		closure->function = result.function;
		push_value(result.state, value_from_closure(closure));
		elf_push_nil(result.state);
		elf_call(result.state, 1, 1);
		elf_Int value = 0;
		if (!elf_to_int(result.state, -1, &value) || value != 42) {
			test_fail("trailing closure is appended as the final call argument");
		}
	}
	backend_test_destroy(result);
}

static void test_backend_calls_imported_static_function(void)
{
	BackendCompileResult result = backend_test_compile_file("test/imports/statics/root.elf");
	if (!bc_function_ref_is_valid(result.function))
	{
		test_fail("backend compiles imported static namespace");
	}
	else
	{
		if (result.function.module->bytecode_function_count != 2 || backend_count_bytecode(result, BC_CALL) != 1 || backend_count_bytecode(result, BC_DYNAMIC_CALL) != 0) {
			test_fail("imported static function emits a direct call");
		}

		elf_Closure *closure = elf_gc_alloc(result.state, ELF_OBJECT_CLOSURE, sizeof(*closure));
		closure->function = result.function;
		push_value(result.state, value_from_closure(closure));
		elf_push_nil(result.state);
		elf_call(result.state, 1, 1);
		elf_Int value = 0;
		if (!elf_to_int(result.state, -1, &value) || value != 42) {
			test_fail("imported static function executes from its source namespace");
		}
		elf_pop(result.state, 1);
	}
	backend_test_destroy(result);
}

static void test_backend_rejects_private_imported_static(void)
{
	BackendCompileResult result = backend_test_compile_file("test/imports/statics/private_root.elf");
	if (bc_function_ref_is_valid(result.function)) {
		test_fail("imported namespace hides private static declarations");
	}
	backend_test_destroy(result);
}

static void test_backend_emits_direct_static_recurse_call(void)
{
	BackendCompileResult result = backend_test_compile_source(
		"count :: fun(n) { if n ? { ret recurse(n - 1) } ret 0 }\n"
		"ret count(3)\n");

	elf_Module *module = result.function.module;
	u32 direct_calls = 0;
	u32 dynamic_calls = 0;
	u32 closures = 0;
	for (u32 i = 0; i < module->bytecode_count; ++i) {
		direct_calls += module->bytecode[i].b_type == BC_CALL;
		dynamic_calls += module->bytecode[i].b_type == BC_DYNAMIC_CALL;
		closures += module->bytecode[i].b_type == BC_CLOSURE;
	}
	if (direct_calls != 2 || dynamic_calls != 0) {
		test_fail("static recurse and its caller emit direct calls");
	}
	if (closures != 0) {
		test_fail("calling recurse in a static function does not allocate a closure");
	}
	backend_test_destroy(result);
}

static void test_backend_materializes_static_recurse_value(void)
{
	BackendCompileResult result = backend_test_compile_source(
		"self :: fun() { ret recurse }\n"
		"ret self()\n");

	elf_Module *module = result.function.module;
	u32 direct_calls = 0;
	u32 dynamic_calls = 0;
	u32 closures = 0;
	for (u32 i = 0; i < module->bytecode_count; ++i) {
		direct_calls += module->bytecode[i].b_type == BC_CALL;
		dynamic_calls += module->bytecode[i].b_type == BC_DYNAMIC_CALL;
		closures += module->bytecode[i].b_type == BC_CLOSURE;
	}
	if (direct_calls != 1 || dynamic_calls != 0) {
		test_fail("calling a static function that returns recurse remains direct");
	}
	if (closures != 1) {
		test_fail("using static recurse as a value materializes one closure");
	}
	backend_test_destroy(result);
}

static void test_backend_emits_parameter_type_enforcement(void)
{
	BackendCompileResult result = backend_test_compile_source("probe := fun(a: int, b: str) { ret [a, b] }\nret probe");
	if (!bc_function_ref_is_valid(result.function) || result.function.module->bytecode_function_count != 2)
	{
		test_fail("backend compiles typed function parameters");
	}
	else
	{
		BcFunction *function = result.function.module->bytecode_functions + 1;
		Bytecode *bytes = result.function.module->bytecode + function->offset;
		if (function->length < 2 || bytes[0].b_type != BC_ENFORCE || bytes[0].b_x != 1 || bytes[0].b_y != TRULE_INTEGER || bytes[1].b_type != BC_ENFORCE || bytes[1].b_x != 2 || bytes[1].b_y != TRULE_STRING)
		{
			test_fail("backend emits parameter type enforcement at function entry");
		}
	}
	backend_test_destroy(result);
}

static void run_backend_tests(void)
{
	test_backend_source_map();
	test_backend_module_owns_source();
	test_backend_short_circuit_and();
	test_backend_short_circuit_or();
	test_backend_short_circuit_nested_and_or();
	test_backend_short_circuit_nested_or_and();
	test_backend_short_circuit_mixed_groups();
	test_backend_while_loop();
	test_backend_while_short_circuit_loop();
	test_backend_range_for_loop_shape();
	test_backend_range_for_collection_call_is_hoisted();
	test_backend_array_bytecode();
	test_backend_local_initializer_reuses_result_slot();
	test_backend_truthy_or_reuses_left_result_slot();
	test_backend_logical_expr_delays_result_slot();
	test_backend_formats_bytecode_function();
	test_backend_outputs_strings();
	test_backend_compiles_static_functions();
	test_backend_emits_direct_static_call();
	test_backend_lowers_trailing_closure();
	test_backend_calls_imported_static_function();
	test_backend_rejects_private_imported_static();
	test_backend_emits_direct_static_recurse_call();
	test_backend_materializes_static_recurse_value();
	test_backend_emits_parameter_type_enforcement();
}
