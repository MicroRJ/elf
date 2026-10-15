static IrFunctionEmitter *ir_test_lower_source(elf_State *state, elf_Arena *arena, const char *source_text)
{
	elf_StrSlice source = {(char *)source_text, (u64)strlen(source_text)};
	Compiler *compiler = compiler_create(arena);
	compiler_add_root_source(compiler, "ir_tests", source);
	Parser *parser = parser_create(compiler, 0);
	Ast file = elf_parse_file(parser);
	compiler->sources[0].ast = file;
	compiler_resolve_statics(compiler, COM_UNIT_FUNCTION);

	IrModuleEmitter *module_emitter = ir_create_module_emitter(compiler);
	IrFunctionEmitter *ctx = ir_create_function_emitter(module_emitter);
	for (u32 i = 0; i < compiler->function_count; ++i) {
		ir_emit_function(ctx, compiler->functions + i);
	}
	compiler_report_unreferenced_statics(compiler, 0);
	IrModule module = module_emitter->module;
	if (module.functions != ctx->module->module.functions ||
		module.function_count != ctx->module->module.function_count ||
		!module.has_entry ||
		module.entry_index != 0)
	{
		test_fail("lowering returns ir module rooted at function zero");
	}
	return ctx;
}

static IrFunction *ir_test_main_function(IrFunctionEmitter *ctx)
{
	if (!ctx || ctx->module->module.function_count != 1) {
		test_fail("lowering produced one main function");
		return 0;
	}
	return &ctx->module->module.functions[0];
}

static Ir ir_test_main_body(IrFunctionEmitter *ctx)
{
	IrFunction *function = ir_test_main_function(ctx);
	if (!function) {
		return 0;
	}
	if (!function->body || function->body->kind != IR_BLOCK) {
		test_fail("lowering produced main block");
		return 0;
	}
	return function->body;
}

static Ir ir_test_body_stat(Ir body, u32 index, IrKind kind, const char *label)
{
	if (!body || body->kind != IR_BLOCK || index >= body->ir_block.stats.count) {
		test_fail(label);
		return 0;
	}
	Ir stat = body->ir_block.stats.items[index];
	if (!stat || stat->kind != kind) {
		test_fail(label);
		return 0;
	}
	return stat;
}

static void expect_ir_kind(Ir ir, IrKind kind, const char *label)
{
	if (!ir || ir->kind != kind) {
		test_fail(label);
	}
}

static void expect_ir_i64(Ir ir, i64 value, const char *label)
{
	if (!ir || ir->kind != IR_INTEGER || ir->ir_int != value) {
		test_fail(label);
	}
}

static void expect_ir_atom(Ir ir, const char *text, const char *label)
{
	if (!ir || ir->kind != IR_ATOM || !ir->atom || strcmp(atom_data(ir->atom), text) != 0) {
		test_fail(label);
	}
}

static void expect_load_local(Ir ir, Ir local, const char *label)
{
	if (!ir || ir->kind != IR_LOAD_LOCAL || ir->ir_load_local.local != local) {
		test_fail(label);
	}
}

static void test_ir_lowers_declaration_expression(elf_State *state)
{
	elf_Arena arena = elf_arena_create(0);
	IrFunctionEmitter *ctx = ir_test_lower_source(state, &arena, "x := 1 + 2 * 3");
	Ir body = ir_test_main_body(ctx);
	Ir memory = ir_test_body_stat(body, 0, IR_LOCAL, "lower declaration to local");

	Ir expr = memory->ir_local.expr;
	expect_ir_kind(expr, IR_ADD, "lower addition");
	expect_ir_i64(expr->ir_binary.x, 1, "lower add lhs");
	expect_ir_kind(expr->ir_binary.y, IR_MUL, "lower multiply rhs");
	expect_ir_i64(expr->ir_binary.y->ir_binary.x, 2, "lower multiply lhs");
	expect_ir_i64(expr->ir_binary.y->ir_binary.y, 3, "lower multiply rhs");
	elf_arena_destroy(&arena);
}

static void test_ir_lowers_assignment_to_local(elf_State *state)
{
	elf_Arena arena = elf_arena_create(0);
	IrFunctionEmitter *ctx = ir_test_lower_source(state, &arena, "x := 1\nx = 2");
	Ir body = ir_test_main_body(ctx);
	Ir memory = ir_test_body_stat(body, 0, IR_LOCAL, "lower local declaration");
	Ir store = ir_test_body_stat(body, 1, IR_STORE, "lower assignment to store");

	expect_load_local(store ? store->ir_binary.x : 0, memory, "assignment destination loads local");
	expect_ir_i64(store->ir_binary.y, 2, "assignment expression lowered");
	elf_arena_destroy(&arena);
}

static void ignore_expected_lowering_error(LogLevel level, const char *message, void *user)
{
	(void)level;
	(void)message;
	(void)user;
}

static void test_ir_rejects_assignment_to_constant(elf_State *state)
{
	elf_Arena arena = elf_arena_create(0);
	log_set_hook(ignore_expected_lowering_error, 0);
	IrFunctionEmitter *ctx = ir_test_lower_source(state, &arena, "answer ::= 42\nanswer = 0");
	log_set_hook(0, 0);

	Ir body = ir_test_main_body(ctx);
	ir_test_body_stat(body, 0, IR_LOCAL, "constant declaration lowers to local");
	if (!body || body->kind != IR_BLOCK || body->ir_block.stats.count < 2 ||
		body->ir_block.stats.items[1] != ERROR_IR)
	{
		test_fail("assignment to constant lowers to an error");
	}
	if (!ctx || !(ctx->entities[0].tags & ENTITY_TAG_CONSTANT)) {
		test_fail("constant declaration tags its entity");
	}
	elf_arena_destroy(&arena);
}

static void test_ir_lowers_strings_and_globals(elf_State *state)
{
	elf_Arena arena = elf_arena_create(0);
	IrFunctionEmitter *ctx = ir_test_lower_source(state, &arena, "ret \"hello\"\nret foo");
	Ir body = ir_test_main_body(ctx);

	Ir ret_string = ir_test_body_stat(body, 0, IR_RETURN, "lower string return");
	expect_ir_atom(ret_string->ir_return.expr, "hello", "string literal lowers to atom ir");

	Ir ret_global = ir_test_body_stat(body, 1, IR_RETURN, "lower global return");
	expect_ir_kind(ret_global->ir_return.expr, IR_LOAD_GLOBAL, "identifier without local lowers to global load");
	elf_arena_destroy(&arena);
}

static void test_ir_lowers_interpolated_string(elf_State *state)
{
	elf_Arena arena = elf_arena_create(0);
	IrFunctionEmitter *ctx = ir_test_lower_source(state, &arena,
		"name := \"elf\"\nret f\"hello ${name} ${2 + 3}\"");
	Ir body = ir_test_main_body(ctx);
	Ir name_local = ir_test_body_stat(body, 0, IR_LOCAL, "lower interpolated string local");
	Ir ret = ir_test_body_stat(body, 1, IR_RETURN, "lower interpolated string return");
	Ir trailing = ret ? ret->ir_return.expr : 0;

	expect_ir_kind(trailing, IR_ADD, "interpolated string lowers final text join");
	expect_ir_atom(trailing ? trailing->ir_binary.y : 0, "", "interpolated string lowers empty trailing text");

	Ir numeric = trailing ? trailing->ir_binary.x : 0;
	expect_ir_kind(numeric, IR_ADD, "interpolated string lowers numeric expression join");
	expect_ir_kind(numeric ? numeric->ir_binary.y : 0, IR_ADD, "interpolated string preserves binary expression");
	expect_ir_i64(numeric && numeric->ir_binary.y ? numeric->ir_binary.y->ir_binary.x : 0, 2,
		"interpolated numeric expression lhs");
	expect_ir_i64(numeric && numeric->ir_binary.y ? numeric->ir_binary.y->ir_binary.y : 0, 3,
		"interpolated numeric expression rhs");

	Ir space = numeric ? numeric->ir_binary.x : 0;
	expect_ir_kind(space, IR_ADD, "interpolated string lowers middle text join");
	expect_ir_atom(space ? space->ir_binary.y : 0, " ", "interpolated string lowers middle text");

	Ir name = space ? space->ir_binary.x : 0;
	expect_ir_kind(name, IR_ADD, "interpolated string lowers identifier join");
	expect_ir_atom(name ? name->ir_binary.x : 0, "hello ", "interpolated string lowers leading text");
	expect_load_local(name ? name->ir_binary.y : 0, name_local, "interpolated string loads local expression");
	elf_arena_destroy(&arena);
}

static void test_ir_lowers_field_and_call(elf_State *state)
{
	elf_Arena arena = elf_arena_create(0);
	IrFunctionEmitter *ctx = ir_test_lower_source(state, &arena, "ret obj.name\nmake({x = 1})");
	Ir body = ir_test_main_body(ctx);

	Ir ret = ir_test_body_stat(body, 0, IR_RETURN, "lower field return");
	Ir field = ret->ir_return.expr;
	expect_ir_kind(field, IR_FIELD, "field access lowers to field ir");
	expect_ir_kind(field->ir_binary.x, IR_LOAD_GLOBAL, "field object lowers to global load");
	expect_ir_atom(field->ir_binary.y, "name", "field key lowers to atom ir");

	Ir call = ir_test_body_stat(body, 1, IR_CALL, "lower call expression statement");
	if (call && call->ir_call.type != IR_CALL_DYNAMIC) {
		test_fail("runtime callee lowers to a dynamic call");
	}
	expect_ir_kind(call->ir_call.expr, IR_LOAD_GLOBAL, "call callee lowers to global load");
	if (call && call->ir_call.args.count != 1) {
		test_fail("call keeps one table argument");
	}
	Ir table_block = call->ir_call.args.items[0];
	expect_ir_kind(table_block, IR_EXPR_BLOCK, "call table argument lowers to expression block");
	Ir table_local = table_block && table_block->kind == IR_EXPR_BLOCK && table_block->ir_expr_block.stats.count
		? table_block->ir_expr_block.stats.items[0]
		: 0;
	expect_ir_kind(table_local, IR_LOCAL, "table expression block creates table local");
	expect_ir_kind(table_local ? table_local->ir_local.expr : 0, IR_TABLE, "table expression block creates table ir");
	elf_arena_destroy(&arena);
}

static void test_ir_lowers_array_literal(elf_State *state)
{
	elf_Arena arena = elf_arena_create(0);
	IrFunctionEmitter *ctx = ir_test_lower_source(state, &arena, "ret [1, 2]");
	Ir body = ir_test_main_body(ctx);
	Ir ret = ir_test_body_stat(body, 0, IR_RETURN, "lower array return");
	Ir block = ret ? ret->ir_return.expr : 0;
	expect_ir_kind(block, IR_EXPR_BLOCK, "array literal lowers to expression block");
	if (!block || block->kind != IR_EXPR_BLOCK || block->ir_expr_block.stats.count != 3)
	{
		test_fail("array literal lowering emits allocation and item appends");
		elf_arena_destroy(&arena);
		return;
	}

	Ir local = block->ir_expr_block.stats.items[0];
	expect_ir_kind(local, IR_LOCAL, "array literal stores allocation in a local");
	expect_ir_kind(local ? local->ir_local.expr : 0, IR_ARRAY, "array literal allocates an array");
	expect_ir_kind(block->ir_expr_block.stats.items[1], IR_ARRAY_ADD, "array literal appends first item");
	expect_ir_kind(block->ir_expr_block.stats.items[2], IR_ARRAY_ADD, "array literal appends second item");
	expect_load_local(block->ir_expr_block.value, local, "array literal returns allocated array");
	elf_arena_destroy(&arena);
}

static void test_ir_lowers_if_else(elf_State *state)
{
	elf_Arena arena = elf_arena_create(0);
	IrFunctionEmitter *ctx = ir_test_lower_source(state, &arena, "if flag ? { ret 1 } else { ret 2 }");
	Ir body = ir_test_main_body(ctx);
	Ir ir_if = ir_test_body_stat(body, 0, IR_IF, "lower if statement");

	expect_ir_kind(ir_if->ir_if.pred, IR_LOAD_GLOBAL, "if predicate lowers to global load");
	expect_ir_kind(ir_if->ir_if.true_clause, IR_BLOCK, "if true clause lowers to block");
	expect_ir_kind(ir_if->ir_if.else_clause, IR_BLOCK, "if else clause lowers to block");
	elf_arena_destroy(&arena);
}

static void test_ir_lowers_while(elf_State *state)
{
	elf_Arena arena = elf_arena_create(0);
	IrFunctionEmitter *ctx = ir_test_lower_source(state, &arena, "while flag ? { x = 1 }");
	Ir body = ir_test_main_body(ctx);
	Ir loop = ir_test_body_stat(body, 0, IR_BLOCK, "while lowers to label block");

	expect_ir_kind(ir_test_body_stat(loop, 0, IR_LABEL, "while starts with label"), IR_LABEL, "while start label");
	Ir branch = ir_test_body_stat(loop, 1, IR_JUMP_IF_FALSE, "while branches to exit");
	expect_ir_kind(branch ? branch->ir_jump_if_false.pred : 0, IR_LOAD_GLOBAL, "while predicate lowers to global load");
	Ir loop_body = ir_test_body_stat(loop, 2, IR_BLOCK, "while body lowers to block");
	expect_ir_kind(ir_test_body_stat(loop, 3, IR_JUMP, "while jumps back to start"), IR_JUMP, "while back jump");
	expect_ir_kind(ir_test_body_stat(loop, 4, IR_LABEL, "while ends with exit label"), IR_LABEL, "while exit label");

	Ir store = ir_test_body_stat(loop_body, 0, IR_STORE, "while body lowers statements");
	expect_ir_kind(store->ir_binary.x, IR_LOAD_GLOBAL, "while body assignment destination");
	expect_ir_i64(store->ir_binary.y, 1, "while body assignment value");
	elf_arena_destroy(&arena);
}

static void test_ir_lowers_nil_assign(elf_State *state)
{
	elf_Arena arena = elf_arena_create(0);
	IrFunctionEmitter *ctx = ir_test_lower_source(state, &arena, "x := nil\nx ?= 1");
	Ir body = ir_test_main_body(ctx);

	Ir memory = ir_test_body_stat(body, 0, IR_LOCAL, "nil assignment keeps declaration local");
	Ir ir_if = ir_test_body_stat(body, 1, IR_IF, "nil assignment lowers to if");

	Ir pred = ir_if->ir_if.pred;
	expect_ir_kind(pred, IR_EQ, "nil assignment predicate compares against nil");
	expect_load_local(pred ? pred->ir_binary.x : 0, memory, "nil assignment predicate reads destination local");
	expect_ir_kind(pred ? pred->ir_binary.y : 0, IR_NIL, "nil assignment predicate rhs is nil");

	Ir true_clause = ir_if->ir_if.true_clause;
	expect_ir_kind(true_clause, IR_BLOCK, "nil assignment true clause is block");
	Ir store = ir_test_body_stat(true_clause, 0, IR_STORE, "nil assignment true clause stores value");
	expect_load_local(store ? store->ir_binary.x : 0, memory, "nil assignment store writes destination local");
	expect_ir_i64(store ? store->ir_binary.y : 0, 1, "nil assignment store value");
	if (ir_if && ir_if->ir_if.else_clause) {
		test_fail("nil assignment has no else clause");
	}
	elf_arena_destroy(&arena);
}

static void test_ir_lowers_nil_assign_field_once(elf_State *state)
{
	elf_Arena arena = elf_arena_create(0);
	IrFunctionEmitter *ctx = ir_test_lower_source(state, &arena, "obj := nil\nobj.child.name ?= 1");
	Ir body = ir_test_main_body(ctx);

	Ir object_memory = ir_test_body_stat(body, 0, IR_LOCAL, "field nil assignment keeps object declaration");
	Ir receiver_memory = ir_test_body_stat(body, 1, IR_LOCAL, "field nil assignment saves receiver once");
	Ir ir_if = ir_test_body_stat(body, 2, IR_IF, "field nil assignment lowers to if");

	Ir receiver_expr = receiver_memory ? receiver_memory->ir_local.expr : 0;
	expect_ir_kind(receiver_expr, IR_FIELD, "field nil assignment temp stores receiver field");
	expect_load_local(receiver_expr ? receiver_expr->ir_binary.x : 0, object_memory, "field nil assignment receiver starts from object local");
	expect_ir_atom(receiver_expr ? receiver_expr->ir_binary.y : 0, "child", "field nil assignment receiver field name");

	Ir pred = ir_if ? ir_if->ir_if.pred : 0;
	expect_ir_kind(pred, IR_EQ, "field nil assignment predicate compares against nil");
	Ir pred_field = pred ? pred->ir_binary.x : 0;
	expect_ir_kind(pred_field, IR_FIELD, "field nil assignment predicate reads final field");
	expect_load_local(pred_field ? pred_field->ir_binary.x : 0, receiver_memory, "field nil assignment predicate reuses receiver temp");
	expect_ir_atom(pred_field ? pred_field->ir_binary.y : 0, "name", "field nil assignment final field name");

	Ir true_clause = ir_if ? ir_if->ir_if.true_clause : 0;
	Ir store = ir_test_body_stat(true_clause, 0, IR_STORE, "field nil assignment true clause stores value");
	Ir store_field = store ? store->ir_binary.x : 0;
	expect_ir_kind(store_field, IR_FIELD, "field nil assignment store writes final field");
	expect_load_local(store_field ? store_field->ir_binary.x : 0, receiver_memory, "field nil assignment store reuses receiver temp");
	expect_ir_i64(store ? store->ir_binary.y : 0, 1, "field nil assignment store value");
	elf_arena_destroy(&arena);
}

static void test_ir_lowers_function_expression(elf_State *state)
{
	elf_Arena arena = elf_arena_create(0);
	IrFunctionEmitter *ctx = ir_test_lower_source(state, &arena, "add := fun(a: int, b: num) { ret a + b }");

	if (!ctx || ctx->module->module.function_count != 2) {
		test_fail("function expression adds bytecode function");
		elf_arena_destroy(&arena);
		return;
	}

	Ir main_body = ctx->module->module.functions[0].body;
	Ir memory = ir_test_body_stat(main_body, 0, IR_LOCAL, "function declaration lowers to local");
	expect_ir_kind(memory ? memory->ir_local.expr : 0, IR_FUNCTION, "function expression lowers to function ir");
	if (memory && memory->ir_local.expr && memory->ir_local.expr->ir_function.index != 1) {
		test_fail("function ir points at nested function");
	}

	IrFunction *function = &ctx->module->module.functions[1];
	if (function->param_count != IMPLICIT_PARAM_COUNT + 2) {
		test_fail("function parameter count includes implicit this and source params");
	}

	Ir body = function->body;
	Ir ret = ir_test_body_stat(body, 0, IR_RETURN, "function body lowers return");
	Ir add = ret ? ret->ir_return.expr : 0;
	expect_ir_kind(add, IR_ADD, "function return lowers expression");
	expect_ir_kind(add ? add->ir_binary.x : 0, IR_LOAD_LOCAL, "first param lowers to local load");
	expect_ir_kind(add ? add->ir_binary.y : 0, IR_LOAD_LOCAL, "second param lowers to local load");
	Ir first_param = add && add->ir_binary.x ? add->ir_binary.x->ir_load_local.local : 0;
	Ir second_param = add && add->ir_binary.y ? add->ir_binary.y->ir_load_local.local : 0;
	Ir implicit_param = function->params[IMPLICIT_PARAM_INDEX];
	expect_ir_kind(implicit_param, IR_LOCAL, "function creates implicit this parameter local");
	if (function->param_count != IMPLICIT_PARAM_COUNT + 2 || function->params[IMPLICIT_PARAM_COUNT] != first_param || function->params[IMPLICIT_PARAM_COUNT + 1] != second_param) {
		test_fail("function retains its implicit and source parameter locals");
	}
	if (!implicit_param || !first_param || !second_param || bc_slot_is_valid(implicit_param->ir_local.slot) || bc_slot_is_valid(first_param->ir_local.slot) || bc_slot_is_valid(second_param->ir_local.slot)) {
		test_fail("lowering leaves parameter frame slots unassigned");
	}
	if (!implicit_param || !first_param || !second_param || implicit_param->type_rule != TRULE_NONE || first_param->type_rule != TRULE_INTEGER || second_param->type_rule != TRULE_NUMBER) {
		test_fail("function parameter locals retain runtime type rules");
	}

	elf_arena_destroy(&arena);
}

static void test_ir_rejects_unknown_parameter_type(elf_State *state)
{
	elf_Arena arena = elf_arena_create(0);
	log_set_hook(ignore_expected_lowering_error, 0);
	IrFunctionEmitter *ctx = ir_test_lower_source(state, &arena, "probe := fun(value: mystery) { ret value }\nret probe");
	log_set_hook(0, 0);
	if (!ctx || ctx->module->compiler->report.error_count != 1) {
		test_fail("unknown function parameter type reports one lowering error");
	}
	elf_arena_destroy(&arena);
}

static void test_ir_lowers_static_forward_reference(elf_State *state)
{
	elf_Arena arena = elf_arena_create(0);
	IrFunctionEmitter *ctx = ir_test_lower_source(state, &arena, "first :: second\nsecond :: 42\nret first");
	if (!ctx || ctx->module->compiler->source_count != 1 || ctx->current_source != 0 || ctx->module->compiler->sources[0].statics.symbol_count != 2) {
		test_fail("lowering stores root static symbols in source scope zero");
	}
	Ir body = ir_test_main_body(ctx);
	Ir ret = ir_test_body_stat(body, 0, IR_RETURN, "static declarations emit no runtime statements");
	expect_ir_i64(ret ? ret->ir_return.expr : 0, 42, "static forward reference resolves to literal ir");
	elf_arena_destroy(&arena);
}

static void test_ir_resolves_static_function_defaults(elf_State *state)
{
	(void)state;
	elf_Arena arena = elf_arena_create(0);
	Compiler *compiler = compiler_create(&arena);
	compiler_add_root_file(compiler, "test/imports/defaults/root.elf");
	b32 parsed = compiler_parse_sources(compiler);
	b32 resolved = parsed && compiler_resolve_statics(compiler, COM_UNIT_FUNCTION);
	if (!resolved || compiler->source_count != 2)
	{
		test_fail("compiler resolves imported static function defaults");
		elf_arena_destroy(&arena);
		return;
	}

	Com_StaticSymbol *make = 0;
	Com_StaticScope *config_scope = &compiler->sources[1].statics;
	for (u32 i = 0; i < config_scope->symbol_count; ++i)
	{
		Com_StaticSymbol *symbol = config_scope->symbols + i;
		if (strcmp(atom_data(symbol->name), "make") == 0) {
			make = symbol;
		}
	}
	if (!make || make->value.kind != COM_STATIC_FUNCTION || make->value.function >= compiler->function_count)
	{
		test_fail("imported static function has a resolved function descriptor");
		elf_arena_destroy(&arena);
		return;
	}

	Com_Function *function = compiler->functions + make->value.function;
	if (function->source != 1 || function->param_count != IMPLICIT_PARAM_COUNT + 3) {
		test_fail("static function descriptor retains source and parameters");
	}
	else
	{
		Com_FunctionParameter *required = function->params + 0;
		Com_FunctionParameter *size = function->params + 1;
		Com_FunctionParameter *label = function->params + 2;
		if (required->has_default || required->default_value.kind != COM_STATIC_INVALID) {
			test_fail("required static parameter has no resolved default");
		}
		if (!size->has_default || size->default_value.kind != COM_STATIC_INTEGER || size->default_value.integer != 42) {
			test_fail("static parameter default resolves in declaration source");
		}
		if (!label->has_default || label->default_value.kind != COM_STATIC_STRING || strcmp(atom_data(label->default_value.string), "elf") != 0) {
			test_fail("literal static parameter default is retained");
		}
	}
	elf_arena_destroy(&arena);
}

static void test_ir_lowers_static_call_arguments(elf_State *state)
{
	elf_Arena arena = elf_arena_create(0);
	IrFunctionEmitter *ctx = ir_test_lower_source(state, &arena, "build :: fun(required, width = 20, title = \"elf\") { ret required }\nret build(1, title = \"custom\")");
	if (!ctx || ctx->module->module.function_count != 2)
	{
		test_fail("static call argument lowering retains the target function");
		elf_arena_destroy(&arena);
		return;
	}

	Ir body = ctx->module->module.functions[0].body;
	Ir ret = ir_test_body_stat(body, 0, IR_RETURN, "static call lowers return");
	Ir call = ret ? ret->ir_return.expr : 0;
	expect_ir_kind(call, IR_CALL, "static call lowers call expression");
	if (call && call->ir_call.type != IR_CALL_DIRECT) {
		test_fail("static call arguments remain on a direct call");
	}
	if (!call || call->ir_call.args.count != 3)
	{
		test_fail("static call fills every declared parameter");
	}
	else
	{
		expect_ir_i64(call->ir_call.args.items[0], 1, "static call keeps positional argument");
		expect_ir_i64(call->ir_call.args.items[1], 20, "static call fills skipped parameter default");
		expect_ir_atom(call->ir_call.args.items[2], "custom", "static call lowers ordered named argument");
	}
	elf_arena_destroy(&arena);
}

static void test_ir_lowers_variadic_static_call_arguments(elf_State *state)
{
	elf_Arena arena = elf_arena_create(0);
	IrFunctionEmitter *ctx = ir_test_lower_source(state, &arena, "collect :: fun(first, ...) { ret first }\nret collect(1, 2, 3)");
	if (!ctx || ctx->module->compiler->report.error_count != 0 || ctx->module->module.function_count != 2)
	{
		test_fail("variadic static call accepts extra positional arguments");
		elf_arena_destroy(&arena);
		return;
	}

	Ir ret = ir_test_body_stat(ctx->module->module.functions[0].body, 0, IR_RETURN, "variadic static call lowers return");
	Ir call = ret ? ret->ir_return.expr : 0;
	if (!call || call->kind != IR_CALL || call->ir_call.type != IR_CALL_DIRECT || call->ir_call.args.count != 3) {
		test_fail("variadic static call retains all positional arguments");
	}
	elf_arena_destroy(&arena);
}

static void test_ir_rejects_invalid_static_call_arguments(elf_State *state)
{
	const char *sources[] =
	{
		"target :: fun(required, optional = 2) { ret required }\nret target()",
		"target :: fun(first = 1, second = 2) { ret first }\nret target(second = 2, first = 1)",
		"target :: fun(first = 1, second = 2) { ret first }\nret target(first = 1, 2)",
		"target :: fun(value = 1) { ret value }\nret target(unknown = 2)",
		"target :: fun(value = 1) { ret value }\nret target(value = 1, value = 2)",
		"target :: fun(value) { ret value }\nret target(1, 2)",
		"target := fun(value) { ret value }\nret target(value = 1)",
	};
	const char *labels[] =
	{
		"static call rejects missing required argument",
		"static call rejects out-of-order named arguments",
		"static call rejects positional arguments after named arguments",
		"static call rejects unknown named arguments",
		"static call rejects duplicated named arguments",
		"static call rejects excess positional arguments",
		"dynamic call rejects named arguments",
	};

	log_set_hook(ignore_expected_lowering_error, 0);
	for (u32 i = 0; i < ARRAY_COUNT(sources); ++i)
	{
		elf_Arena arena = elf_arena_create(0);
		IrFunctionEmitter *ctx = ir_test_lower_source(state, &arena, sources[i]);
		if (!ctx || ctx->module->compiler->report.error_count != 1) {
			test_fail(labels[i]);
		}
		elf_arena_destroy(&arena);
	}
	log_set_hook(0, 0);
}

static void test_ir_rejects_runtime_static_function_default(elf_State *state)
{
	elf_Arena arena = elf_arena_create(0);
	log_set_hook(ignore_expected_lowering_error, 0);
	IrFunctionEmitter *ctx = ir_test_lower_source(state, &arena, "bad :: fun(value = runtime_default()) { ret value }\nret bad");
	log_set_hook(0, 0);
	if (!ctx || ctx->module->compiler->report.error_count != 1 || ctx->module->compiler->report.first->phase != COMPILER_PHASE_EVALUATION) {
		test_fail("runtime static function default reports one evaluation error");
	}
	elf_arena_destroy(&arena);
}

static void test_ir_lowers_local_shadow_of_file_static(elf_State *state)
{
	elf_Arena arena = elf_arena_create(0);
	IrFunctionEmitter *ctx = ir_test_lower_source(state, &arena, "root :: 7\n{\nroot := 48\nx := root * 2\n}\nret root");
	Ir body = ir_test_main_body(ctx);
	Ir nested = ir_test_body_stat(body, 0, IR_BLOCK, "nested local scope lowers to runtime block");
	Ir local = ir_test_body_stat(nested, 1, IR_LOCAL, "nested local declaration is retained");
	Ir mul = local ? local->ir_local.expr : 0;
	expect_ir_kind(mul, IR_MUL, "nested local participates in runtime expression");
	expect_load_local(mul ? mul->ir_binary.x : 0, ir_test_body_stat(nested, 0, IR_LOCAL, "nested local shadows file static"), "nested local shadows file static");
	expect_ir_i64(mul ? mul->ir_binary.y : 0, 2, "nested runtime expression keeps rhs");
	Ir ret = ir_test_body_stat(body, 1, IR_RETURN, "outer static remains visible after nested scope");
	expect_ir_i64(ret ? ret->ir_return.expr : 0, 7, "nested local does not replace file static");
	elf_arena_destroy(&arena);
}

static void test_ir_lowers_mutually_recursive_static_functions(elf_State *state)
{
	elf_Arena arena = elf_arena_create(0);
	IrFunctionEmitter *ctx = ir_test_lower_source(state, &arena, "foo :: fun() { ret bar() }\nbar :: fun() { ret foo() }\nret foo");
	if (!ctx || ctx->module->module.function_count != 3)
	{
		test_fail("mutually recursive statics lower two functions");
		elf_arena_destroy(&arena);
		return;
	}

	Ir main_ret = ir_test_body_stat(ctx->module->module.functions[0].body, 0, IR_RETURN, "main returns static function");
	Ir foo_value = main_ret ? main_ret->ir_return.expr : 0;
	expect_ir_kind(foo_value, IR_FUNCTION, "static function lowers directly to function ir");
	if (foo_value && foo_value->ir_function.index != 1) {
		test_fail("main static function value points to foo");
	}

	Ir foo_ret = ir_test_body_stat(ctx->module->module.functions[1].body, 0, IR_RETURN, "foo lowers return");
	Ir foo_call = foo_ret ? foo_ret->ir_return.expr : 0;
	expect_ir_kind(foo_call, IR_CALL, "foo calls bar");
	if (foo_call && foo_call->ir_call.type != IR_CALL_DIRECT) {
		test_fail("static foo-to-bar call lowers as direct");
	}
	Ir bar_value = foo_call ? foo_call->ir_call.expr : 0;
	expect_ir_kind(bar_value, IR_FUNCTION, "foo resolves bar statically");
	if (bar_value && bar_value->ir_function.index != 2) {
		test_fail("foo points to bar function");
	}

	Ir bar_ret = ir_test_body_stat(ctx->module->module.functions[2].body, 0, IR_RETURN, "bar lowers return");
	Ir bar_call = bar_ret ? bar_ret->ir_return.expr : 0;
	expect_ir_kind(bar_call, IR_CALL, "bar calls foo");
	if (bar_call && bar_call->ir_call.type != IR_CALL_DIRECT) {
		test_fail("static bar-to-foo call lowers as direct");
	}
	Ir recursive_foo = bar_call ? bar_call->ir_call.expr : 0;
	expect_ir_kind(recursive_foo, IR_FUNCTION, "bar resolves foo statically");
	if (recursive_foo && recursive_foo->ir_function.index != 1) {
		test_fail("bar points back to foo function");
	}
	if (ctx->module->module.functions[1].capture_count || ctx->module->module.functions[2].capture_count) {
		test_fail("static function references do not create captures");
	}
	elf_arena_destroy(&arena);
}

static void test_ir_rejects_static_cycle(elf_State *state)
{
	elf_Arena arena = elf_arena_create(0);
	log_set_hook(ignore_expected_lowering_error, 0);
	IrFunctionEmitter *ctx = ir_test_lower_source(state, &arena, "first :: second\nsecond :: first\nret first");
	log_set_hook(0, 0);
	if (!ctx || ctx->module->compiler->report.error_count != 1) {
		test_fail("static cycle reports one lowering error");
	}
	elf_arena_destroy(&arena);
}

static void test_ir_rejects_non_file_static_declarations(elf_State *state)
{
	elf_Arena arena = elf_arena_create(0);
	log_set_hook(ignore_expected_lowering_error, 0);
	IrFunctionEmitter *ctx = ir_test_lower_source(state, &arena, "root :: 7\n{\nlocal :: 48\n}\nholder := fun() {\ninside :: 1\n}\nret root");
	log_set_hook(0, 0);
	if (!ctx || ctx->module->compiler->report.error_count != 2) {
		test_fail("static declarations outside file scope report lowering errors");
	}
	elf_arena_destroy(&arena);
}

static void test_ir_lowers_get_mem_macro(elf_State *state)
{
	elf_Arena arena = elf_arena_create(0);
	IrFunctionEmitter *ctx = ir_test_lower_source(state, &arena, "x := 1\nret #get_mem(x)");
	Ir body = ir_test_main_body(ctx);

	Ir memory = ir_test_body_stat(body, 0, IR_LOCAL, "get_mem keeps local declaration");
	Ir ret = ir_test_body_stat(body, 1, IR_RETURN, "get_mem lowers return");
	Ir get_mem = ret ? ret->ir_return.expr : 0;
	expect_ir_kind(get_mem, IR_GET_MEM, "get_mem lowers to get_mem ir");
	expect_load_local(get_mem ? get_mem->ir_unary : 0, memory, "get_mem targets local load");

	elf_arena_destroy(&arena);
}

static void run_ir_tests(elf_State *state)
{
	test_ir_lowers_declaration_expression(state);
	test_ir_lowers_assignment_to_local(state);
	test_ir_rejects_assignment_to_constant(state);
	test_ir_lowers_strings_and_globals(state);
	test_ir_lowers_interpolated_string(state);
	test_ir_lowers_field_and_call(state);
	test_ir_lowers_array_literal(state);
	test_ir_lowers_if_else(state);
	test_ir_lowers_while(state);
	test_ir_lowers_nil_assign(state);
	test_ir_lowers_nil_assign_field_once(state);
	test_ir_lowers_function_expression(state);
	test_ir_rejects_unknown_parameter_type(state);
	test_ir_lowers_static_forward_reference(state);
	test_ir_resolves_static_function_defaults(state);
	test_ir_lowers_static_call_arguments(state);
	test_ir_lowers_variadic_static_call_arguments(state);
	test_ir_rejects_invalid_static_call_arguments(state);
	test_ir_rejects_runtime_static_function_default(state);
	test_ir_lowers_local_shadow_of_file_static(state);
	test_ir_lowers_mutually_recursive_static_functions(state);
	test_ir_rejects_static_cycle(state);
	test_ir_rejects_non_file_static_declarations(state);
	test_ir_lowers_get_mem_macro(state);
}
