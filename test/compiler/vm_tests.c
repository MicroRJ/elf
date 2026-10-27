typedef struct
{
	elf_Ref callback;
}
VmNestedCallContext;

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
	elf_Prototype *caller_prototype = caller ? frame_proto(caller) : 0;
	Bytecode *caller_next_instruction = caller ? frame_next_instr(caller) : 0;
	b32 preserved = true;

	if (!context)
	{
		test_fail("nested host call expected user context, got null");
		preserved = false;
	}
	if (!caller)
	{
		test_fail("nested host call expected a bytecode caller frame, got none");
		preserved = false;
	}
	else if (!caller_prototype)
	{
		test_fail("nested host call expected its caller frame to have a prototype, got null");
		preserved = false;
	}
	else
	{
		BcType caller_type = BC_TYPE(*frame_active_instr(caller));
		if (caller_type != BC_CALL_DYNAMIC)
		{
			TEST_FAIL("nested host call expected active caller instruction 'call_dynamic', got '%s'", bytecode_type_name(caller_type));
			preserved = false;
		}
	}
	if (preserved && !elf_push_ref(S, context->callback))
	{
		test_fail("nested host call expected its callback reference to remain valid, got an invalid reference");
		preserved = false;
	}
	if (preserved)
	{
		elf_call(S, 0);
		StackFrame *restored_caller = S->frame > S->frame_stack ? S->frame - 1 : 0;
		if (S->frame != native_frame_pointer)
		{
			TEST_FAIL("nested host call expected current frame %p, got %p", (void *)native_frame_pointer, (void *)S->frame);
			preserved = false;
		}
		if (S->frame->base != native_frame.base)
		{
			TEST_FAIL("nested host call expected native frame base %p, got %p", (void *)native_frame.base, (void *)S->frame->base);
			preserved = false;
		}
		if (S->frame->regs != native_frame.regs)
		{
			TEST_FAIL("nested host call expected native registers %p, got %p", (void *)native_frame.regs, (void *)S->frame->regs);
			preserved = false;
		}
		if (S->frame->size != native_frame.size)
		{
			TEST_FAIL("nested host call expected native frame size %u, got %u", native_frame.size, S->frame->size);
			preserved = false;
		}
		if (S->frame->nargs != native_frame.nargs)
		{
			TEST_FAIL("nested host call expected native argument count %u, got %u", native_frame.nargs, S->frame->nargs);
			preserved = false;
		}
		if (!restored_caller)
		{
			test_fail("nested host call expected its bytecode caller to remain on the frame stack, got none");
			preserved = false;
		}
		else
		{
			if (frame_proto(restored_caller) != caller_prototype)
			{
				TEST_FAIL("nested host call expected caller prototype %p, got %p", (void *)caller_prototype, (void *)frame_proto(restored_caller));
				preserved = false;
			}
			if (frame_next_instr(restored_caller) != caller_next_instruction)
			{
				TEST_FAIL("nested host call expected caller continuation %p, got %p", (void *)caller_next_instruction, (void *)frame_next_instr(restored_caller));
				preserved = false;
			}
		}
	}
	elf_push_int(S, preserved);
	return 1;
}

static void test_vm_preserves_actual_returns(void)
{
	elf_State *state = elf_create_state();
	char source_text[] = "ret 11, 22";
	elf_StrSlice source = {source_text, sizeof(source_text) - 1};
	if (elf_push_code_source(state, 0, ELF_LIT("actual-returns"), source) != ELF_ERROR_NONE)
	{
		test_fail("actual-return test source compiles");
		elf_destroy_state(state);
		return;
	}

	elf_Value *results = state->stack_top - 1;
	u32 returned_count = elf_call(state, 0);
	if (returned_count != 2)
	{
		test_fail("bytecode call reports its actual return count");
	}
	if (state->stack_top != results + 2)
	{
		test_fail("bytecode call exposes its actual return values");
	}
	if (!value_is_integer(results[0]) || value_as_integer(results[0]) != 11 || !value_is_integer(results[1]) || value_as_integer(results[1]) != 22)
	{
		test_fail("bytecode call preserves all actual return values");
	}

	elf_destroy_state(state);
}

static ELF_FUNCTION(vm_test_symbol_function)
{
	(void)nargs;
	elf_push_int(S, 8);
	return 1;
}

static void test_vm_symbols(void)
{
	elf_State *state = elf_create_state();
	elf_SymTable *symbols = 0;
	elf_SymTable *host = 0;
	elf_ErrorCode create_symbols_error = elf_sym_create(&symbols);
	elf_ErrorCode create_host_error = elf_sym_create(&host);
	elf_ErrorCode answer_error = elf_sym_int(host, ELF_LIT("answer"), 34);
	elf_ErrorCode function_error = elf_sym_fun(host, ELF_LIT("get"), vm_test_symbol_function);
	elf_ErrorCode host_error = elf_sym_tab(symbols, ELF_LIT("host"), host);
	char source_text[] = "ret host.answer + host.get()";
	elf_ErrorCode compile_error = elf_push_code_source(state, symbols, ELF_LIT("symbols"), (elf_StrSlice) {source_text, sizeof(source_text) - 1});
	if (create_symbols_error != ELF_ERROR_NONE || create_host_error != ELF_ERROR_NONE || answer_error != ELF_ERROR_NONE || function_error != ELF_ERROR_NONE || host_error != ELF_ERROR_NONE || compile_error != ELF_ERROR_NONE)
	{
		test_fail("VM compiles environment symbols");
		elf_sym_destroy(symbols);
		elf_destroy_state(state);
		return;
	}
	elf_sym_destroy(symbols);

	elf_call(state, 0);
	elf_Value result = state->stack_top[-1];
	if (!value_is_integer(result) || value_as_integer(result) != 42)
	{
		test_fail("VM executes environment symbols");
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

	if (elf_push_code_source(state, 0, ELF_LIT("first"), first_source) != ELF_ERROR_NONE) {
		test_fail("first closure module compiles");
		elf_destroy_state(state);
		return;
	}
	elf_Closure *first_entry = value_as_closure(state->stack_top[-1]);
	elf_Prototype *first_entry_prototype = first_entry->prototype;
	if (first_entry_prototype != state->prototypes[0]) {
		test_fail("first compiled entry uses the first prototype index");
	}
	elf_call(state, 0);
	elf_Value first_factory = state->stack_top[-1];
	if (!value_is_prototype(first_factory)) {
		test_fail("zero-capture factory compiles to a prototype value");
		elf_destroy_state(state);
		return;
	}
	elf_Prototype *first_factory_prototype = value_as_prototype(first_factory);
	elf_Ref first_factory_ref = elf_create_ref(state, -1);
	elf_pop(state, 1);

	if (elf_push_code_source(state, 0, ELF_LIT("second"), second_source) != ELF_ERROR_NONE) {
		test_fail("second closure module compiles");
		elf_destroy_state(state);
		return;
	}
	elf_Closure *second_entry = value_as_closure(state->stack_top[-1]);
	elf_Prototype *second_entry_prototype = second_entry->prototype;
	if (second_entry_prototype == first_entry_prototype) {
		test_fail("each compilation publishes a distinct entry prototype");
	}
	elf_call(state, 0);
	elf_Value second_factory = state->stack_top[-1];
	if (!value_is_prototype(second_factory)) {
		test_fail("second zero-capture factory compiles to a prototype value");
		elf_destroy_state(state);
		return;
	}
	elf_Prototype *second_factory_prototype = value_as_prototype(second_factory);
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
	elf_push_int(state, 100);
	elf_call(state, 1);
	elf_Closure *first_closure = value_as_closure(state->stack_top[-1]);
	elf_Prototype *first_closure_prototype = first_closure->prototype;
	elf_Ref first = elf_create_ref(state, -1);
	elf_pop(state, 1);

	if (!elf_push_ref(state, second_factory_ref)) {
		test_fail("second compiled factory remains referenceable");
		elf_destroy_state(state);
		return;
	}
	elf_push_int(state, 200);
	elf_call(state, 1);
	elf_Closure *second_closure = value_as_closure(state->stack_top[-1]);
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
	elf_call(state, 0);
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
	elf_call(state, 0);
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
	if (elf_push_code_source(state, 0, ELF_LIT("nested-callback"), callback_source) != ELF_ERROR_NONE) {
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
	elf_SymTable *symbols = 0;
	if (elf_sym_create(&symbols) != ELF_ERROR_NONE || elf_sym_fun(symbols, ELF_LIT("context_probe"), vm_test_nested_call_preserves_context) != ELF_ERROR_NONE) {
		test_fail("nested context probe binding registration");
	}

	char caller_source_text[] = "ret context_probe()";
	elf_StrSlice caller_source = {caller_source_text, sizeof(caller_source_text) - 1};
	if (elf_push_code_source(state, symbols, ELF_LIT("nested-caller"), caller_source) != ELF_ERROR_NONE) {
		test_fail("nested caller compiles");
		elf_sym_destroy(symbols);
		elf_release_ref(state, context.callback);
		elf_destroy_state(state);
		return;
	}
	elf_sym_destroy(symbols);
	elf_call(state, 0);

	elf_Int preserved = 0;
	if (!elf_to_int(state, -1, &preserved))
	{
		TEST_FAIL("nested host context probe expected type 'integer', got '%s'", value_type_name(elf_type(state, -1)));
	}
	else if (preserved != 1)
	{
		TEST_FAIL("nested host context probe expected success 1, got %lld after the preceding context failure", (long long)preserved);
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
	if (elf_push_code_source(state, 0, ELF_LIT("diagnostic-stack"), source) != ELF_ERROR_NONE)
	{
		elf_destroy_state(state);
		return 2;
	}
	elf_call(state, 0);
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
		dy_String args[] = {DY_LIT("--runtime-error-diagnostic-child")};
		dy_Process_Start_Options options = {
			.executable = executable,
			.args = {args, ARRAY_COUNT(args)},
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

static void test_vm_module_converts_atom_constants(void)
{
	elf_State *state = elf_create_state();
	char source_text[] = "ret [\"same\", \"same\", \"other\"]";
	elf_StrSlice source = {source_text, sizeof(source_text) - 1};
	if (elf_push_code_source(state, 0, ELF_LIT("module-strings"), source) != ELF_ERROR_NONE) {
		test_fail("module string source compiles");
		elf_destroy_state(state);
		return;
	}

	elf_Prototype *prototype = value_as_closure(state->stack_top[-1])->prototype;
	if (prototype->string_count != 2 || strcmp(elf_string_data(prototype->strings[0]), "same") != 0 || strcmp(elf_string_data(prototype->strings[1]), "other") != 0)
	{
		test_fail("function converts deduplicated atom constants to strings");
	}
	vm_force_gc_allocations(state, 256);
	if (strcmp(elf_string_data(prototype->strings[0]), "same") != 0 || strcmp(elf_string_data(prototype->strings[1]), "other") != 0) {
		test_fail("function string constants survive GC");
	}

	elf_call(state, 0);
	elf_Array *result = value_as_array(state->stack_top[-1]);
	if (elf_array_length(result) != 3 || !elf_string_equal(value_as_string(elf_array_get(state, result, 0)), prototype->strings[0]) || !elf_string_equal(value_as_string(elf_array_get(state, result, 1)), prototype->strings[0]) || !elf_string_equal(value_as_string(elf_array_get(state, result, 2)), prototype->strings[1]))
	{
		test_fail("VM loads string constants from their function");
	}

	elf_destroy_state(state);
}

static void test_vm_error_snapshots_call_frames(void)
{
	elf_State *state = elf_create_state();
	char source_text[] =
		"make_error :: fun() ret elf.error(\"failure\", { kind = \"trace\" })\n"
		"outer :: fun() ret make_error()\n"
		"ret outer()";
	elf_StrSlice source = {source_text, sizeof(source_text) - 1};
	if (elf_push_code_source(state, 0, ELF_LIT("error-trace"), source) != ELF_ERROR_NONE)
	{
		test_fail("error trace source compiles");
		elf_destroy_state(state);
		return;
	}

	elf_call(state, 0);
	elf_Value result = state->stack_top[-1];
	if (!value_is_error(result))
	{
		test_fail("error trace source returns an error");
	}
	else
	{
		elf_GCError *error = value_as_error(result);
		if (error->frame_count != 3)
		{
			TEST_FAIL("error snapshots 3 bytecode frames, got %u", error->frame_count);
		}
		for (u32 index = 0; index < error->frame_count; ++index)
		{
			elf_ErrorFrame frame = error->frames[index];
			if (!frame.prototype || frame.instruction >= frame.prototype->bytecode_count)
			{
				test_fail("error snapshot contains valid bytecode locations");
			}
		}
		if (!error->data)
		{
			test_fail("error snapshot retains attached data");
		}
	}

	elf_destroy_state(state);
}

static void run_vm_tests(void)
{
	TEST_RUN("vm.preserves_actual_returns", test_vm_preserves_actual_returns);
	TEST_RUN("vm.symbols", test_vm_symbols);
	TEST_RUN("vm.compiled_closure_keeps_function_identity", test_vm_compiled_closure_keeps_function_identity);
	TEST_RUN("vm.nested_host_call_preserves_diagnostics_context", test_vm_nested_host_call_preserves_diagnostics_context);
	TEST_RUN("vm.runtime_error_diagnostics", test_vm_runtime_error_diagnostics);
	TEST_RUN("vm.module_converts_atom_constants", test_vm_module_converts_atom_constants);
	TEST_RUN("vm.error_snapshots_call_frames", test_vm_error_snapshots_call_frames);
}
