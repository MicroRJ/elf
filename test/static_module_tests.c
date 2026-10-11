static void test_static_module_collection(elf_State *state)
{
	elf_Arena arena = elf_arena_create(0);
	const char *source_text = "answer :: 42\nrender :: fun() {}\nruntime := 1";
	elf_StrSlice source = {(char *)source_text, (u64)strlen(source_text)};
	Compiler *compiler = compiler_create(state, &arena, "static_module_tests", source);
	compiler->log_diagnostics = false;
	Parser *parser = elf_create_parser(compiler);
	Ast file = elf_parse_file(parser);
	StaticModule *module = static_module_build(compiler, file);

	if (module->ast != file) {
		test_fail("static module retains its ast");
	}
	if (module->symbol_count != 2) {
		test_fail("static module collects top-level static declarations");
	}
	else
	{
		StaticSymbol *answer = module->symbols + 0;
		StaticSymbol *render = module->symbols + 1;
		if (strcmp(atom_data(answer->name), "answer") != 0 || answer->declaration->kind != AST_STATIC_DECL) {
			test_fail("static module stores first declaration");
		}
		if (strcmp(atom_data(render->name), "render") != 0 || render->declaration->kind != AST_STATIC_DECL) {
			test_fail("static module stores second declaration");
		}
		if (static_module_find_symbol(module, answer->name) != 0 || static_module_find_symbol(module, render->name) != 1) {
			test_fail("static module assigns stable symbol indices");
		}
		if (answer->state != STATIC_SYMBOL_RESOLVED || answer->value.kind != STATIC_VALUE_INTEGER || answer->value.integer != 42) {
			test_fail("static module evaluates integer symbols");
		}
		if (render->state != STATIC_SYMBOL_RESOLVED || render->value.kind != STATIC_VALUE_FUNCTION) {
			test_fail("static module evaluates function symbols");
		}
	}
	if (compiler->error_count != 0) {
		test_fail("static module collection accepts unique declarations");
	}

	compiler_finish_report(compiler, 0);
	elf_arena_destroy(&arena);
}

static void test_static_module_duplicate(elf_State *state)
{
	elf_Arena arena = elf_arena_create(0);
	const char *source_text = "answer :: 1\nanswer :: 2";
	elf_StrSlice source = {(char *)source_text, (u64)strlen(source_text)};
	Compiler *compiler = compiler_create(state, &arena, "static_module_tests", source);
	compiler->log_diagnostics = false;
	Parser *parser = elf_create_parser(compiler);
	Ast file = elf_parse_file(parser);
	StaticModule *module = static_module_build(compiler, file);

	if (module->symbol_count != 1) {
		test_fail("static module ignores duplicate declaration");
	}
	if (compiler->error_count != 1 || compiler->diagnostic_count != 1) {
		test_fail("static module reports duplicate declaration");
	}
	else if (compiler->diagnostics[0].phase != ELF_DIAGNOSTIC_PHASE_LOWERING) {
		test_fail("static module duplicate uses lowering diagnostic phase");
	}

	compiler_finish_report(compiler, 0);
	elf_arena_destroy(&arena);
}

static void test_static_module_forward_reference(elf_State *state)
{
	elf_Arena arena = elf_arena_create(0);
	const char *source_text = "first :: second\nsecond :: 42";
	elf_StrSlice source = {(char *)source_text, (u64)strlen(source_text)};
	Compiler *compiler = compiler_create(state, &arena, "static_module_tests", source);
	compiler->log_diagnostics = false;
	Parser *parser = elf_create_parser(compiler);
	Ast file = elf_parse_file(parser);
	StaticModule *module = static_module_build(compiler, file);

	if (compiler->error_count != 0 || module->symbol_count != 2) {
		test_fail("static module resolves forward references");
	}
	else if (module->symbols[0].value.kind != STATIC_VALUE_INTEGER || module->symbols[0].value.integer != 42) {
		test_fail("static forward reference receives resolved value");
	}

	compiler_finish_report(compiler, 0);
	elf_arena_destroy(&arena);
}

static void test_static_module_cycle(elf_State *state)
{
	elf_Arena arena = elf_arena_create(0);
	const char *source_text = "first :: second\nsecond :: first";
	elf_StrSlice source = {(char *)source_text, (u64)strlen(source_text)};
	Compiler *compiler = compiler_create(state, &arena, "static_module_tests", source);
	compiler->log_diagnostics = false;
	Parser *parser = elf_create_parser(compiler);
	Ast file = elf_parse_file(parser);
	StaticModule *module = static_module_build(compiler, file);

	if (compiler->error_count != 1 || module->symbols[0].state != STATIC_SYMBOL_RESOLVED || module->symbols[1].state != STATIC_SYMBOL_RESOLVED) {
		test_fail("static module rejects definition cycles");
	}

	compiler_finish_report(compiler, 0);
	elf_arena_destroy(&arena);
}

static void run_static_module_tests(elf_State *state)
{
	test_static_module_collection(state);
	test_static_module_duplicate(state);
	test_static_module_forward_reference(state);
	test_static_module_cycle(state);
}
