typedef struct
{
	Arena       arena;
	Compiler   *compiler;
	Com_Source *source;
	Com_Status  status;
}
CheckerTest;

static void checker_test_init(CheckerTest *test, const char *name, const char *source_text)
{
	zero_memory(test, sizeof(*test));
	test->arena = arena_create(0);
	test->compiler = compiler_create(&test->arena);
	elf_StrSlice source_name = {(char *)name, strlen(name)};
	elf_StrSlice source = {(char *)source_text, strlen(source_text)};
	test->source = parser_test_add_source(test->compiler, source_name, source);

	Parser parser = {0};
	if (!parser_init(&parser)) {
		test_fail("checker test initializes parser");
		return;
	}
	Ast file = parser_parse_file(&parser, test->source);
	parser_destroy(&parser);
	if (!file || ast_is_error(file)) {
		test_fail("checker test parses source");
		return;
	}
	test->source->ast = file;
	test->source->state = COM_SOURCE_PARSED;
	test->status = check_source(test->source);
	test->source->state = COM_SOURCE_CHECKED;
}

static void checker_test_destroy(CheckerTest *test)
{
	arena_destroy(&test->arena);
}

static Ast checker_test_stat(CheckerTest *test, u32 index, Ast_Kind kind, const char *label)
{
	Ast file = test->source ? test->source->ast : 0;
	Ast body = file && file->kind == AST_FILE ? file->file.body : 0;
	if (!body || body->kind != AST_BLOCK_STAT || index >= body->block.count) {
		test_fail(label);
		return 0;
	}
	Ast stat = body->block.items[index];
	if (!stat || stat->kind != kind) {
		test_fail(label);
		return 0;
	}
	return stat;
}

static Ast checker_test_tuple_item(Ast tuple, u32 index, Ast_Kind kind, const char *label)
{
	if (!tuple || tuple->kind != AST_TUPLE || index >= tuple->tuple.count) {
		test_fail(label);
		return 0;
	}
	Ast item = tuple->tuple.items[index];
	if (!item || item->kind != kind) {
		test_fail(label);
		return 0;
	}
	return item;
}

static Ast checker_test_single_expression(Ast expr, Ast_Kind kind, const char *label)
{
	return checker_test_tuple_item(expr, 0, kind, label);
}

static void test_checker_resolves_static_forward_reference(void)
{
	CheckerTest test;
	checker_test_init(&test, "checker_static_forward", "first :: second\nsecond :: 42\nret first");
	Ast first_decl = checker_test_stat(&test, 0, AST_STATIC_DECL, "checker parses first static declaration");
	Ast second_decl = checker_test_stat(&test, 1, AST_STATIC_DECL, "checker parses second static declaration");
	Ast return_stat = checker_test_stat(&test, 2, AST_RETURN, "checker parses static return");
	Entity *first = first_decl ? first_decl->entity : 0;
	Entity *second = second_decl ? second_decl->entity : 0;
	if (!first || !second || first->kind != ENTITY_STATIC || second->kind != ENTITY_STATIC) {
		test_fail("checker predeclares source statics");
	}
	if (!first || first->state != ENTITY_RESOLVED || first->value.type != STATIC_VALUE_INTEGER || first->value.integer != 42) {
		test_fail("checker lazily resolves static forward reference");
	}
	if (!second || second->state != ENTITY_RESOLVED || second->value.type != STATIC_VALUE_INTEGER || second->value.integer != 42) {
		test_fail("checker resolves referenced static value");
	}
	if (!first_decl || first_decl->static_decl.name->entity != first || !second_decl || second_decl->static_decl.name->entity != second) {
		test_fail("checker attaches static entities to declaration names");
	}
	Ast return_expr = return_stat ? checker_test_single_expression(return_stat->return_stat.expr, AST_IDENT, "checker finds static return expression") : 0;
	if (!return_expr || return_expr->entity != first) {
		test_fail("checker attaches static entity to identifier use");
	}
	checker_test_destroy(&test);
}

static void test_checker_tags_exported_static(void)
{
	CheckerTest test;
	checker_test_init(&test, "checker_export", "export answer :: 42\nret answer");
	Ast declaration = checker_test_stat(&test, 0, AST_STATIC_DECL, "checker parses exported static");
	Entity *entity = declaration ? declaration->entity : 0;
	if (!entity || !(entity->tags & ENTITY_TAG_EXPORTED)) {
		test_fail("checker tags exported source static");
	}
	checker_test_destroy(&test);
}

static void test_checker_declares_local_after_initializer(void)
{
	CheckerTest test;
	checker_test_init(&test, "checker_initializer", "value := value\nret value");
	Ast declaration = checker_test_stat(&test, 0, AST_DECL_STAT, "checker parses local declaration");
	Ast return_stat = checker_test_stat(&test, 1, AST_RETURN, "checker parses local return");
	Ast name = declaration ? checker_test_tuple_item(declaration->decl.name, 0, AST_IDENT, "checker finds local declaration name") : 0;
	Ast initializer = declaration ? checker_test_tuple_item(declaration->decl.expr, 0, AST_IDENT, "checker finds local initializer") : 0;
	Entity *local = name ? name->entity : 0;
	Entity *prior = initializer ? initializer->entity : 0;
	if (!local || local->kind != ENTITY_LOCAL || !prior || prior->kind != ENTITY_GLOBAL || local == prior) {
		test_fail("checker resolves initializer before declaring local names");
	}
	Ast return_expr = return_stat ? checker_test_single_expression(return_stat->return_stat.expr, AST_IDENT, "checker finds local return expression") : 0;
	if (!return_expr || return_expr->entity != local) {
		test_fail("checker exposes local after its declaration");
	}
	checker_test_destroy(&test);
}

static void test_checker_preserves_block_shadowing(void)
{
	CheckerTest test;
	checker_test_init(&test, "checker_shadow", "value := 1\nif 1 ? { value := value\nret value }\nret value");
	Ast outer_decl = checker_test_stat(&test, 0, AST_DECL_STAT, "checker parses outer declaration");
	Ast if_stat = checker_test_stat(&test, 1, AST_IF, "checker parses shadowing block");
	Ast outer_return = checker_test_stat(&test, 2, AST_RETURN, "checker parses outer return");
	Ast inner_block = if_stat ? if_stat->if_stat.true_clause : 0;
	Ast inner_decl = inner_block && inner_block->kind == AST_BLOCK_STAT && inner_block->block.count > 0 ? inner_block->block.items[0] : 0;
	Ast inner_return = inner_block && inner_block->kind == AST_BLOCK_STAT && inner_block->block.count > 1 ? inner_block->block.items[1] : 0;
	Ast outer_name = outer_decl ? checker_test_tuple_item(outer_decl->decl.name, 0, AST_IDENT, "checker finds outer name") : 0;
	Ast inner_name = inner_decl && inner_decl->kind == AST_DECL_STAT ? checker_test_tuple_item(inner_decl->decl.name, 0, AST_IDENT, "checker finds inner name") : 0;
	Ast inner_initializer = inner_decl && inner_decl->kind == AST_DECL_STAT ? checker_test_tuple_item(inner_decl->decl.expr, 0, AST_IDENT, "checker finds shadow initializer") : 0;
	Entity *outer = outer_name ? outer_name->entity : 0;
	Entity *inner = inner_name ? inner_name->entity : 0;
	if (!outer || !inner || outer == inner || inner->scope->parent != outer->scope) {
		test_fail("checker creates a persistent child scope for block shadowing");
	}
	if (!inner_initializer || inner_initializer->entity != outer) {
		test_fail("checker resolves shadow initializer through parent scope");
	}
	Ast inner_return_expr = inner_return && inner_return->kind == AST_RETURN ? checker_test_single_expression(inner_return->return_stat.expr, AST_IDENT, "checker finds inner return expression") : 0;
	if (!inner_return_expr || inner_return_expr->entity != inner) {
		test_fail("checker resolves block use to shadowing entity");
	}
	Ast outer_return_expr = outer_return ? checker_test_single_expression(outer_return->return_stat.expr, AST_IDENT, "checker finds outer return expression") : 0;
	if (!outer_return_expr || outer_return_expr->entity != outer) {
		test_fail("checker restores parent scope after block");
	}
	checker_test_destroy(&test);
}

static void test_checker_does_not_leak_block_locals(void)
{
	CheckerTest test;
	checker_test_init(&test, "checker_scope_leak", "if 1 ? { hidden := 1 }\nret hidden");
	Ast if_stat = checker_test_stat(&test, 0, AST_IF, "checker parses local block");
	Ast return_stat = checker_test_stat(&test, 1, AST_RETURN, "checker parses use after block");
	Ast block = if_stat ? if_stat->if_stat.true_clause : 0;
	Ast declaration = block && block->kind == AST_BLOCK_STAT && block->block.count ? block->block.items[0] : 0;
	Ast name = declaration && declaration->kind == AST_DECL_STAT ? checker_test_tuple_item(declaration->decl.name, 0, AST_IDENT, "checker finds block local") : 0;
	Entity *local = name ? name->entity : 0;
	Ast return_expr = return_stat ? checker_test_single_expression(return_stat->return_stat.expr, AST_IDENT, "checker finds use after block") : 0;
	Entity *outside = return_expr ? return_expr->entity : 0;
	if (!local || local->kind != ENTITY_LOCAL || !outside || outside->kind != ENTITY_GLOBAL || outside == local) {
		test_fail("checker does not leak block locals into parent scope");
	}
	checker_test_destroy(&test);
}

static void test_checker_attaches_function_entities(void)
{
	CheckerTest test;
	checker_test_init(&test, "checker_function", "outer := fun(param) { local := param\ninner := fun() { ret local }\nret inner }");
	Ast outer_decl = checker_test_stat(&test, 0, AST_DECL_STAT, "checker parses outer function declaration");
	Ast outer_function = outer_decl ? checker_test_tuple_item(outer_decl->decl.expr, 0, AST_FUNCTION, "checker finds outer function") : 0;
	Ast parameter = outer_function && outer_function->function.params.count ? outer_function->function.params.items[0] : 0;
	Ast parameter_name = parameter && parameter->kind == AST_FUNCTION_PARAM ? parameter->param.name : 0;
	Ast body = outer_function ? outer_function->function.body : 0;
	Ast local_decl = body && body->kind == AST_BLOCK_STAT && body->block.count > 0 ? body->block.items[0] : 0;
	Ast inner_decl = body && body->kind == AST_BLOCK_STAT && body->block.count > 1 ? body->block.items[1] : 0;
	Ast local_name = local_decl && local_decl->kind == AST_DECL_STAT ? checker_test_tuple_item(local_decl->decl.name, 0, AST_IDENT, "checker finds captured local") : 0;
	Ast parameter_use = local_decl && local_decl->kind == AST_DECL_STAT ? checker_test_tuple_item(local_decl->decl.expr, 0, AST_IDENT, "checker finds parameter use") : 0;
	Ast inner_function = inner_decl && inner_decl->kind == AST_DECL_STAT ? checker_test_tuple_item(inner_decl->decl.expr, 0, AST_FUNCTION, "checker finds inner function") : 0;
	Ast inner_body = inner_function ? inner_function->function.body : 0;
	Ast inner_return = inner_body && inner_body->kind == AST_BLOCK_STAT && inner_body->block.count ? inner_body->block.items[0] : 0;
	Entity *parameter_entity = parameter_name ? parameter_name->entity : 0;
	Entity *local_entity = local_name ? local_name->entity : 0;
	if (!parameter_entity || parameter_entity->kind != ENTITY_PARAMETER || parameter_entity->parent != outer_function) {
		test_fail("checker attaches parameter entity to owning function");
	}
	if (!parameter_use || parameter_use->entity != parameter_entity) {
		test_fail("checker resolves function parameter use");
	}
	if (!local_entity || local_entity->kind != ENTITY_LOCAL || local_entity->parent != outer_function) {
		test_fail("checker records local owner function");
	}
	Ast inner_return_expr = inner_return && inner_return->kind == AST_RETURN ? checker_test_single_expression(inner_return->return_stat.expr, AST_IDENT, "checker finds captured local use") : 0;
	if (!inner_return_expr || inner_return_expr->entity != local_entity) {
		test_fail("checker binds nested function reference to outer local entity");
	}
	checker_test_destroy(&test);
}

static void test_checker_binds_call_arguments_to_parameter_slots(void)
{
	CheckerTest test;
	checker_test_init(&test, "checker_named_arguments", "target :: fun(first, second, third, fourth) {}\ntarget(1, third = 3, fourth = 4)");
	Ast statement = checker_test_stat(&test, 1, AST_TUPLE, "checker parses named argument call");
	Ast call = statement ? checker_test_tuple_item(statement, 0, AST_CALL, "checker finds named argument call") : 0;
	if (test.status != COM_STATUS_OK || !call || call->call.args.count != 3)
	{
		test_fail("checker preserves call arguments while binding parameter slots");
	}
	else if (call->call.args.items[0]->param_slot != 0 || call->call.args.items[1]->param_slot != 2 || call->call.args.items[2]->param_slot != 3)
	{
		test_fail("checker binds positional and named arguments to parameter slots");
	}
	checker_test_destroy(&test);
}

static void checker_expect_call_error(const char *name, const char *source, ErrorCode code, const char *label)
{
	CheckerTest test;
	checker_test_init(&test, name, source);
	Com_Message *message = test.source ? test.source->report.first : 0;
	if (test.status != COM_STATUS_REPORTED || !message || test.source->report.error_count != 1 || message->code != code || message->phase != ERROR_PHASE_CHECKER) {
		test_fail(label);
	}
	checker_test_destroy(&test);
}

static void test_checker_reports_invalid_named_arguments(void)
{
	checker_expect_call_error("checker_dynamic_named_argument", "target := fun(first) {}\ntarget(first = 1)", ERROR_NAMED_ARGUMENT_TARGET, "checker rejects named arguments for dynamic calls");
	checker_expect_call_error("checker_unknown_named_argument", "target :: fun(first) {}\ntarget(missing = 1)", ERROR_UNKNOWN_NAMED_ARGUMENT, "checker reports unknown named arguments");
	checker_expect_call_error("checker_duplicate_argument", "target :: fun(first, second) {}\ntarget(1, first = 2)", ERROR_DUPLICATE_ARGUMENT, "checker reports duplicate argument bindings");
	checker_expect_call_error("checker_named_argument_order", "target :: fun(first, second, third) {}\ntarget(third = 3, second = 2)", ERROR_ARGUMENT_ORDER, "checker enforces named argument parameter order");
	checker_expect_call_error("checker_positional_argument_order", "target :: fun(first, second) {}\ntarget(first = 1, 2)", ERROR_ARGUMENT_ORDER, "checker rejects positional arguments after named arguments");
}

static void test_checker_reports_duplicate_local(void)
{
	CheckerTest test;
	checker_test_init(&test, "checker_duplicate", "value := 1\nvalue := 2");
	Com_Message *message = test.source ? test.source->report.first : 0;
	if (!test.source || test.source->report.error_count != 1 || !message || message->code != ERROR_DUPLICATE_DECLARATION || message->phase != ERROR_PHASE_CHECKER) {
		test_fail("checker reports duplicate local declaration");
	}
	checker_test_destroy(&test);
}

static void test_checker_reports_duplicate_static(void)
{
	CheckerTest test;
	checker_test_init(&test, "checker_duplicate_static", "value :: 1\nvalue :: 2");
	Com_Message *message = test.source ? test.source->report.first : 0;
	if (test.status != COM_STATUS_REPORTED || !message || test.source->report.error_count != 1 || message->code != ERROR_DUPLICATE_STATIC_DECLARATION || message->phase != ERROR_PHASE_CHECKER) {
		test_fail("checker reports duplicate static declaration");
	}
	checker_test_destroy(&test);
}

static void test_checker_resolves_imported_export(void)
{
	Arena arena = arena_create(0);
	Compiler *compiler = compiler_create(&arena);
	Com_Source *root = parser_test_add_source(compiler, (elf_StrSlice) {(char *)"checker_root", sizeof("checker_root") - 1}, (elf_StrSlice) {(char *)"module :: import \"checker_module\"\nret module.answer", sizeof("module :: import \"checker_module\"\nret module.answer") - 1});
	Com_Source *module = parser_test_add_source(compiler, (elf_StrSlice) {(char *)"checker_module", sizeof("checker_module") - 1}, (elf_StrSlice) {(char *)"export answer :: 42", sizeof("export answer :: 42") - 1});
	Parser parser = {0};
	if (!parser_init(&parser)) {
		test_fail("checker import test initializes parser");
		arena_destroy(&arena);
		return;
	}
	root->ast = parser_parse_file(&parser, root);
	module->ast = parser_parse_file(&parser, module);
	parser_destroy(&parser);
	root->state = COM_SOURCE_PARSED;
	module->state = COM_SOURCE_PARSED;
	Ast import = root->ast && root->ast->kind == AST_FILE && root->ast->file.imports.count ? root->ast->file.imports.items[0] : 0;
	if (import && import->kind == AST_IMPORT) {
		import->import.target = module;
		check_source(root);
	}
	else {
		test_fail("checker import test parses import");
	}
	Ast root_return = root->ast && root->ast->kind == AST_FILE && root->ast->file.body->block.count > 1 ? root->ast->file.body->block.items[1] : 0;
	Ast field = root_return && root_return->kind == AST_RETURN ? checker_test_single_expression(root_return->return_stat.expr, AST_FIELD, "checker finds imported field expression") : 0;
	Atom *answer_name = atom_from_data_size(compiler->atoms, "answer", sizeof("answer") - 1);
	Entity *answer = module->scope ? scope_find_current(module->scope, answer_name) : 0;
	if (!answer || answer->kind != ENTITY_STATIC || !(answer->tags & ENTITY_TAG_EXPORTED) || answer->value.type != STATIC_VALUE_INTEGER || answer->value.integer != 42) {
		test_fail("checker lazily resolves imported export");
	}
	if (!field || field->kind != AST_FIELD || field->entity != answer) {
		test_fail("checker attaches imported entity to field expression");
	}
	arena_destroy(&arena);
}

typedef struct
{
	Arena       arena;
	Compiler   *compiler;
	Com_Source *root;
	Com_Source *module;
}
CheckerImportTest;

static void checker_import_test_init(CheckerImportTest *test, const char *root_text, const char *module_text)
{
	zero_memory(test, sizeof(*test));
	test->arena = arena_create(0);
	test->compiler = compiler_create(&test->arena);
	test->root = parser_test_add_source(test->compiler, (elf_StrSlice) {(char *)"checker_import_root", sizeof("checker_import_root") - 1}, (elf_StrSlice) {(char *)root_text, strlen(root_text)});
	test->module = parser_test_add_source(test->compiler, (elf_StrSlice) {(char *)"checker_import_module", sizeof("checker_import_module") - 1}, (elf_StrSlice) {(char *)module_text, strlen(module_text)});

	Parser parser = {0};
	if (!parser_init(&parser)) {
		test_fail("checker error import test initializes parser");
		return;
	}
	test->root->ast = parser_parse_file(&parser, test->root);
	test->module->ast = parser_parse_file(&parser, test->module);
	parser_destroy(&parser);
	if (!test->root->ast || ast_is_error(test->root->ast) || !test->module->ast || ast_is_error(test->module->ast)) {
		test_fail("checker error import test parses sources");
		return;
	}
	test->root->state = COM_SOURCE_PARSED;
	test->module->state = COM_SOURCE_PARSED;
	Ast import = test->root->ast->file.imports.count ? test->root->ast->file.imports.items[0] : 0;
	if (!import || import->kind != AST_IMPORT) {
		test_fail("checker error import test finds import");
		return;
	}
	import->import.target = test->module;
	check_source(test->root);
}

static void checker_import_test_destroy(CheckerImportTest *test)
{
	arena_destroy(&test->arena);
}

static void test_checker_reports_static_cycle(void)
{
	CheckerTest test;
	checker_test_init(&test, "checker_cycle", "first :: second\nsecond :: first\nret first");
	Com_Message *message = test.source ? test.source->report.first : 0;
	Ast first_decl = checker_test_stat(&test, 0, AST_STATIC_DECL, "checker parses cyclic static");
	Ast second_decl = checker_test_stat(&test, 1, AST_STATIC_DECL, "checker parses cyclic static dependency");
	if (test.status != COM_STATUS_REPORTED || !message || test.source->report.error_count != 1 || message->code != ERROR_CYCLIC_STATIC_DEFINITION || message->phase != ERROR_PHASE_CHECKER) {
		test_fail("checker reports cyclic static definition");
	}
	if (!first_decl || !second_decl || first_decl->entity->value.type != STATIC_VALUE_ERROR || first_decl->entity->value.status != COM_STATUS_REPORTED || second_decl->entity->value.type != STATIC_VALUE_ERROR || second_decl->entity->value.status != COM_STATUS_REPORTED) {
		test_fail("checker propagates cyclic static error through entities");
	}
	checker_test_destroy(&test);
}

static void test_checker_reports_invalid_static_expression(void)
{
	CheckerTest test;
	checker_test_init(&test, "checker_invalid_static", "answer :: runtime_value\nret answer");
	Com_Message *message = test.source ? test.source->report.first : 0;
	Ast declaration = checker_test_stat(&test, 0, AST_STATIC_DECL, "checker parses invalid static");
	if (test.status != COM_STATUS_REPORTED || !message || test.source->report.error_count != 1 || message->code != ERROR_INVALID_STATIC_EXPRESSION || message->phase != ERROR_PHASE_CHECKER) {
		test_fail("checker reports expression without static value");
	}
	if (!declaration || !declaration->entity || declaration->entity->value.type != STATIC_VALUE_ERROR || declaration->entity->value.status != COM_STATUS_REPORTED) {
		test_fail("checker stores invalid static result on entity");
	}
	checker_test_destroy(&test);
}

static void test_checker_reports_unresolved_import(void)
{
	CheckerTest test;
	checker_test_init(&test, "checker_unresolved_import", "module :: import \"missing\"\nret module.answer");
	Com_Message *message = test.source ? test.source->report.first : 0;
	if (test.status != COM_STATUS_REPORTED || !message || test.source->report.error_count != 1 || message->code != ERROR_IMPORT || message->phase != ERROR_PHASE_CHECKER) {
		test_fail("checker reports import without resolved source");
	}
	checker_test_destroy(&test);
}

static void test_checker_reports_missing_imported_static(void)
{
	CheckerImportTest test;
	checker_import_test_init(&test, "module :: import \"checker_import_module\"\nret module.missing", "export answer :: 42");
	Com_Message *message = test.root ? test.root->report.first : 0;
	if (!message || test.root->report.error_count != 1 || message->code != ERROR_STATIC_LOOKUP || message->phase != ERROR_PHASE_CHECKER) {
		test_fail("checker reports missing imported static");
	}
	checker_import_test_destroy(&test);
}

static void test_checker_reports_private_imported_static(void)
{
	CheckerImportTest test;
	checker_import_test_init(&test, "module :: import \"checker_import_module\"\nret module.answer", "answer :: 42");
	Com_Message *message = test.root ? test.root->report.first : 0;
	if (!message || test.root->report.error_count != 1 || message->code != ERROR_STATIC_LOOKUP || message->phase != ERROR_PHASE_CHECKER) {
		test_fail("checker reports private imported static");
	}
	checker_import_test_destroy(&test);
}

static void test_checker_routes_imported_static_error_to_source(void)
{
	CheckerImportTest test;
	checker_import_test_init(&test, "module :: import \"checker_import_module\"\nret module.answer", "export answer :: runtime_value");
	Com_Message *message = test.module ? test.module->report.first : 0;
	if (!message || test.module->report.error_count != 1 || message->code != ERROR_INVALID_STATIC_EXPRESSION || test.root->report.error_count != 0) {
		test_fail("checker routes imported static error to defining source");
	}
	checker_import_test_destroy(&test);
}

static void test_checker_propagates_diagnostic_oom(void)
{
	Arena arena = arena_create(0);
	Compiler *compiler = compiler_create(&arena);
	char source_text[] = "value := 42";
	Com_Source *source = parser_test_add_source(compiler, (elf_StrSlice) {(char *)"checker_oom", sizeof("checker_oom") - 1}, (elf_StrSlice) {source_text, sizeof(source_text) - 1});
	Parser parser = {0};
	ASSERT(parser_init(&parser));
	source->ast = parser_parse_file(&parser, source);
	parser_destroy(&parser);
	source->state = COM_SOURCE_PARSED;
	u64 capacity = arena.capacity;
	arena.capacity = arena.used;
	Com_Status status = check_source(source);
	arena.capacity = capacity;
	if (status != COM_STATUS_OOM || compiler->error_count != 0 || source->report.error_count != 0) {
		test_fail("checker propagates failure to allocate an out-of-memory diagnostic");
	}
	arena_destroy(&arena);
}

static void run_checker_tests(void)
{
	test_checker_resolves_static_forward_reference();
	test_checker_tags_exported_static();
	test_checker_declares_local_after_initializer();
	test_checker_preserves_block_shadowing();
	test_checker_does_not_leak_block_locals();
	test_checker_attaches_function_entities();
	test_checker_binds_call_arguments_to_parameter_slots();
	test_checker_reports_invalid_named_arguments();
	test_checker_reports_duplicate_local();
	test_checker_reports_duplicate_static();
	test_checker_resolves_imported_export();
	test_checker_reports_static_cycle();
	test_checker_reports_invalid_static_expression();
	test_checker_reports_unresolved_import();
	test_checker_reports_missing_imported_static();
	test_checker_reports_private_imported_static();
	test_checker_routes_imported_static_error_to_source();
	test_checker_propagates_diagnostic_oom();
}
