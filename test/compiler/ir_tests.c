typedef struct
{
	Compiler   *compiler;
	Com_Source *source;
	IrModule   *module;
}
IrTestResult;

static IrTestResult ir_test_lower_source(elf_State *state, Arena *arena, const char *source_text)
{
	IrTestResult result = {0};
	elf_StrSlice source = {(char *)source_text, (u64)strlen(source_text)};
	result.compiler = compiler_create(arena);
	result.compiler->target = state;
	result.source = parser_test_add_source(result.compiler, (elf_StrSlice) {(char *)"ir_tests", sizeof("ir_tests") - 1}, source);

	Parser parser = {0};
	ASSERT(parser_init(&parser));
	Ast file = parser_parse_source(&parser, result.source);
	parser_destroy(&parser);
	result.source->ast = file;
	result.source->state = COM_SOURCE_PARSED;

	if (!ast_is_error(file)) {
		check_source(result.source);
		result.source->state = COM_SOURCE_CHECKED;
		result.module = ir_emit_module(result.source);
	}
	return result;
}

static IrFunction *ir_test_function_at(IrModule *module, u32 index)
{
	IrFunction *function = module ? module->first_function : 0;
	for (u32 i = 0; function && i < index; ++i) {
		function = function->next;
	}
	return function;
}

static IrFunction *ir_test_main_function(IrModule *module)
{
	if (!module || module->function_count != 1) {
		test_fail("lowering produced one main function");
		return 0;
	}
	return module->first_function;
}

static Ir ir_test_main_body(IrModule *module)
{
	IrFunction *function = ir_test_main_function(module);
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
	if (!ir || ir->kind != IR_LOAD || ir->ir_load.local != local) {
		test_fail(label);
	}
}

static void test_ir_lowers_declaration_expression(elf_State *state)
{
	Arena arena = arena_create(0);
	IrTestResult result = ir_test_lower_source(state, &arena, "x := 1 + 2 * 3");
	Ir body = ir_test_main_body(result.module);
	Ir memory = ir_test_body_stat(body, 0, IR_LOCAL, "lower declaration to local");

	Ir expr = memory->ir_local.expr;
	expect_ir_kind(expr, IR_ADD, "lower addition");
	expect_ir_i64(expr->ir_binary.x, 1, "lower add lhs");
	expect_ir_kind(expr->ir_binary.y, IR_MUL, "lower multiply rhs");
	expect_ir_i64(expr->ir_binary.y->ir_binary.x, 2, "lower multiply lhs");
	expect_ir_i64(expr->ir_binary.y->ir_binary.y, 3, "lower multiply rhs");
	arena_destroy(&arena);
}

static void test_ir_lowers_bitwise_not(elf_State *state)
{
	Arena arena = arena_create(0);
	IrTestResult result = ir_test_lower_source(state, &arena, "ret ~1");
	Ir body = ir_test_main_body(result.module);
	Ir ret = ir_test_body_stat(body, 0, IR_RETURN, "lower bitwise not return");
	Ir expr = ret ? ret->ir_return.expr : 0;

	expect_ir_kind(expr, IR_BITWISE_NOT, "lower bitwise not expression");
	expect_ir_i64(expr ? expr->ir_unary : 0, 1, "lower bitwise not operand");
	arena_destroy(&arena);
}

static void test_ir_lowers_tuple_expression(elf_State *state)
{
	Arena arena = arena_create(0);
	IrTestResult result = ir_test_lower_source(state, &arena, "ret 1, 2 + 3, \"four\"");
	Ir body = ir_test_main_body(result.module);
	Ir ret = ir_test_body_stat(body, 0, IR_RETURN, "lower tuple return");
	Ir tuple = ret ? ret->ir_return.expr : 0;

	expect_ir_kind(tuple, IR_TUPLE, "AST tuple lowers to IR tuple");
	if (!tuple || tuple->kind != IR_TUPLE || tuple->ir_tuple.count != 3)
	{
		test_fail("IR tuple retains every AST tuple item");
	}
	else
	{
		expect_ir_i64(tuple->ir_tuple.items[0], 1, "IR tuple lowers first item");
		expect_ir_kind(tuple->ir_tuple.items[1], IR_ADD, "IR tuple lowers second item");
		expect_ir_i64(tuple->ir_tuple.items[1]->ir_binary.x, 2, "IR tuple lowers second item lhs");
		expect_ir_i64(tuple->ir_tuple.items[1]->ir_binary.y, 3, "IR tuple lowers second item rhs");
		expect_ir_atom(tuple->ir_tuple.items[2], "four", "IR tuple lowers third item");
	}
	arena_destroy(&arena);
}

static void test_ir_lowers_assignment_to_local(elf_State *state)
{
	Arena arena = arena_create(0);
	IrTestResult result = ir_test_lower_source(state, &arena, "x := 1\nx = 2");
	Ir body = ir_test_main_body(result.module);
	Ir memory = ir_test_body_stat(body, 0, IR_LOCAL, "lower local declaration");
	Ir store = ir_test_body_stat(body, 1, IR_STORE, "lower assignment to store");

	if (!store || store->ir_binary.x != memory) {
		test_fail("assignment destination is local");
	}
	expect_ir_i64(store->ir_binary.y, 2, "assignment expression lowered");
	arena_destroy(&arena);
}

static void ignore_expected_lowering_error(LogLevel level, const char *message, void *user)
{
	(void)level;
	(void)message;
	(void)user;
}

static void test_ir_rejects_assignment_to_constant(elf_State *state)
{
	Arena arena = arena_create(0);
	log_set_hook(ignore_expected_lowering_error, 0);
	IrTestResult result = ir_test_lower_source(state, &arena, "answer ::= 42\nanswer = 0");
	log_set_hook(0, 0);

	Ir body = ir_test_main_body(result.module);
	ir_test_body_stat(body, 0, IR_LOCAL, "constant declaration lowers to local");
	if (!body || body->kind != IR_BLOCK || body->ir_block.stats.count < 2 || !body->ir_block.stats.items[1] || body->ir_block.stats.items[1]->kind != IR_ERROR)
	{
		test_fail("assignment to constant lowers to an error");
	}
	Ast declaration = result.source->ast->function.body->block.items[0];
	Ast names = ast_ensure_kind(declaration->decl.name, AST_TUPLE);
	if (!names->tuple.items[0]->entity || !entity_is_constant(names->tuple.items[0]->entity)) {
		test_fail("constant declaration marks its entity constant");
	}
	arena_destroy(&arena);
}

static void test_ir_lowers_strings_and_globals(elf_State *state)
{
	Arena arena = arena_create(0);
	IrTestResult result = ir_test_lower_source(state, &arena, "ret \"hello\"\nret foo");
	Ir body = ir_test_main_body(result.module);

	Ir ret_string = ir_test_body_stat(body, 0, IR_RETURN, "lower string return");
	expect_ir_atom(ret_string->ir_return.expr, "hello", "string literal lowers to atom ir");

	Ir ret_global = ir_test_body_stat(body, 1, IR_RETURN, "lower global return");
	expect_ir_kind(ret_global->ir_return.expr, IR_GLOBAL, "identifier without local lowers to global load");
	arena_destroy(&arena);
}

static void test_ir_lowers_interpolated_string(elf_State *state)
{
	Arena arena = arena_create(0);
	IrTestResult result = ir_test_lower_source(state, &arena,
		"name := \"elf\"\nret f\"hello ${name} ${2 + 3}\"");
	Ir body = ir_test_main_body(result.module);
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
	arena_destroy(&arena);
}

static void test_ir_lowers_field_and_call(elf_State *state)
{
	Arena arena = arena_create(0);
	IrTestResult result = ir_test_lower_source(state, &arena, "ret obj.name\nmake({x = 1})");
	Ir body = ir_test_main_body(result.module);

	Ir ret = ir_test_body_stat(body, 0, IR_RETURN, "lower field return");
	Ir field = ret->ir_return.expr;
	expect_ir_kind(field, IR_FIELD, "field access lowers to field ir");
	expect_ir_kind(field->ir_binary.x, IR_GLOBAL, "field object lowers to global load");
	expect_ir_atom(field->ir_binary.y, "name", "field key lowers to atom ir");

	Ir call = ir_test_body_stat(body, 1, IR_CALL, "lower call expression statement");
	expect_ir_kind(call->ir_call.expr, IR_GLOBAL, "call callee lowers to global load");
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
	arena_destroy(&arena);
}

static void test_ir_lowers_array_literal(elf_State *state)
{
	Arena arena = arena_create(0);
	IrTestResult result = ir_test_lower_source(state, &arena, "ret [1, 2]");
	Ir body = ir_test_main_body(result.module);
	Ir ret = ir_test_body_stat(body, 0, IR_RETURN, "lower array return");
	Ir block = ret ? ret->ir_return.expr : 0;
	expect_ir_kind(block, IR_EXPR_BLOCK, "array literal lowers to expression block");
	if (!block || block->kind != IR_EXPR_BLOCK || block->ir_expr_block.stats.count != 3)
	{
		test_fail("array literal lowering emits allocation and item appends");
		arena_destroy(&arena);
		return;
	}

	Ir local = block->ir_expr_block.stats.items[0];
	expect_ir_kind(local, IR_LOCAL, "array literal stores allocation in a local");
	expect_ir_kind(local ? local->ir_local.expr : 0, IR_ARRAY, "array literal allocates an array");
	expect_ir_kind(block->ir_expr_block.stats.items[1], IR_ARRAY_ADD, "array literal appends first item");
	expect_ir_kind(block->ir_expr_block.stats.items[2], IR_ARRAY_ADD, "array literal appends second item");
	expect_load_local(block->ir_expr_block.value, local, "array literal returns allocated array");
	arena_destroy(&arena);
}

static void test_ir_lowers_if_else(elf_State *state)
{
	Arena arena = arena_create(0);
	IrTestResult result = ir_test_lower_source(state, &arena, "if flag ? { ret 1 } else { ret 2 }");
	Ir body = ir_test_main_body(result.module);
	Ir ir_if = ir_test_body_stat(body, 0, IR_IF, "lower if statement");

	expect_ir_kind(ir_if->ir_if.pred, IR_GLOBAL, "if predicate lowers to global load");
	expect_ir_kind(ir_if->ir_if.true_clause, IR_BLOCK, "if true clause lowers to block");
	expect_ir_kind(ir_if->ir_if.else_clause, IR_BLOCK, "if else clause lowers to block");
	arena_destroy(&arena);
}

static void test_ir_lowers_while(elf_State *state)
{
	Arena arena = arena_create(0);
	IrTestResult result = ir_test_lower_source(state, &arena, "while flag ? { x = 1 }");
	Ir body = ir_test_main_body(result.module);
	Ir loop = ir_test_body_stat(body, 0, IR_BLOCK, "while lowers to label block");

	expect_ir_kind(ir_test_body_stat(loop, 0, IR_LABEL, "while starts with label"), IR_LABEL, "while start label");
	Ir branch = ir_test_body_stat(loop, 1, IR_JUMP_IF_FALSE, "while branches to exit");
	expect_ir_kind(branch ? branch->ir_jump_if_false.pred : 0, IR_GLOBAL, "while predicate lowers to global load");
	Ir loop_body = ir_test_body_stat(loop, 2, IR_BLOCK, "while body lowers to block");
	expect_ir_kind(ir_test_body_stat(loop, 3, IR_JUMP, "while jumps back to start"), IR_JUMP, "while back jump");
	expect_ir_kind(ir_test_body_stat(loop, 4, IR_LABEL, "while ends with exit label"), IR_LABEL, "while exit label");

	Ir store = ir_test_body_stat(loop_body, 0, IR_STORE, "while body lowers statements");
	expect_ir_kind(store->ir_binary.x, IR_GLOBAL, "while body assignment destination");
	expect_ir_i64(store->ir_binary.y, 1, "while body assignment value");
	arena_destroy(&arena);
}

static void test_ir_lowers_nil_assign(elf_State *state)
{
	Arena arena = arena_create(0);
	IrTestResult result = ir_test_lower_source(state, &arena, "x := nil\nx ?= 1");
	Ir body = ir_test_main_body(result.module);

	Ir memory = ir_test_body_stat(body, 0, IR_LOCAL, "nil assignment keeps declaration local");
	Ir ir_if = ir_test_body_stat(body, 1, IR_IF, "nil assignment lowers to if");

	Ir pred = ir_if->ir_if.pred;
	expect_ir_kind(pred, IR_EQ, "nil assignment predicate compares against nil");
	expect_load_local(pred ? pred->ir_binary.x : 0, memory, "nil assignment predicate reads destination local");
	expect_ir_kind(pred ? pred->ir_binary.y : 0, IR_NIL, "nil assignment predicate rhs is nil");

	Ir true_clause = ir_if->ir_if.true_clause;
	expect_ir_kind(true_clause, IR_BLOCK, "nil assignment true clause is block");
	Ir store = ir_test_body_stat(true_clause, 0, IR_STORE, "nil assignment true clause stores value");
	if (!store || store->ir_binary.x != memory) {
		test_fail("nil assignment store writes destination local");
	}
	expect_ir_i64(store ? store->ir_binary.y : 0, 1, "nil assignment store value");
	if (ir_if && ir_if->ir_if.else_clause) {
		test_fail("nil assignment has no else clause");
	}
	arena_destroy(&arena);
}

static void test_ir_lowers_nil_assign_field_once(elf_State *state)
{
	Arena arena = arena_create(0);
	IrTestResult result = ir_test_lower_source(state, &arena, "obj := nil\nobj.child.name ?= 1");
	Ir body = ir_test_main_body(result.module);

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
	arena_destroy(&arena);
}

static void test_ir_reports_nil_assignment_arity(elf_State *state)
{
	Arena arena = arena_create(0);
	IrTestResult result = ir_test_lower_source(state, &arena, "x, y := nil, nil\nx, y ?= 1");
	Com_Message *message = result.source ? result.source->report.first : 0;
	if (!message || result.source->report.error_count != 1 || message->code != ERROR_INVALID_ASSIGNMENT || message->phase != ERROR_PHASE_LOWERING) {
		test_fail("nil assignment arity reports a lowering error");
	}
	arena_destroy(&arena);
}

static void test_ir_lowers_function_expression(elf_State *state)
{
	Arena arena = arena_create(0);
	IrTestResult result = ir_test_lower_source(state, &arena, "add := fun(a: int, b: num) { ret a + b }");

	if (!result.module || result.module->function_count != 2) {
		test_fail("function expression adds bytecode function");
		arena_destroy(&arena);
		return;
	}

	Ir main_body = result.module->first_function->body;
	Ir memory = ir_test_body_stat(main_body, 0, IR_LOCAL, "function declaration lowers to local");
	expect_ir_kind(memory ? memory->ir_local.expr : 0, IR_FUNCTION, "function expression lowers to function ir");
	if (memory && memory->ir_local.expr && memory->ir_local.expr->ir_function != result.module->first_function->next) {
		test_fail("function ir points at nested function");
	}

	IrFunction *function = ir_test_function_at(result.module, 1);
	if (result.source->ast->function.ir != result.module->first_function) {
		test_fail("root ast points at entry ir function");
	}
	if (!function->ast || function->ast->kind != AST_FUNCTION || function->ast->function.ir != function) {
		test_fail("function ast points at nested ir function");
	}
	if (function->param_count != IMPLICIT_PARAM_COUNT + 2) {
		test_fail("function parameter count includes implicit this and source params");
	}

	Ir body = function->body;
	Ir ret = ir_test_body_stat(body, 0, IR_RETURN, "function body lowers return");
	Ir add = ret ? ret->ir_return.expr : 0;
	expect_ir_kind(add, IR_ADD, "function return lowers expression");
	expect_ir_kind(add ? add->ir_binary.x : 0, IR_LOAD, "first param lowers to local load");
	expect_ir_kind(add ? add->ir_binary.y : 0, IR_LOAD, "second param lowers to local load");
	Ir first_param = add && add->ir_binary.x ? add->ir_binary.x->ir_load.local : 0;
	Ir second_param = add && add->ir_binary.y ? add->ir_binary.y->ir_load.local : 0;
	Ir implicit_param = function->params[IMPLICIT_PARAM_INDEX];
	expect_ir_kind(implicit_param, IR_LOCAL, "function creates implicit this parameter local");
	if (function->param_count != IMPLICIT_PARAM_COUNT + 2 || function->params[IMPLICIT_PARAM_COUNT] != first_param || function->params[IMPLICIT_PARAM_COUNT + 1] != second_param) {
		test_fail("function retains its implicit and source parameter locals");
	}
	if (!implicit_param || !first_param || !second_param || implicit_param->ir_local.slot != NO_SLOT || first_param->ir_local.slot != NO_SLOT || second_param->ir_local.slot != NO_SLOT) {
		test_fail("lowering leaves parameter frame slots unassigned");
	}
	if (!implicit_param || !first_param || !second_param || implicit_param->type_rule != TRULE_NONE || first_param->type_rule != TRULE_INTEGER || second_param->type_rule != TRULE_NUMBER) {
		test_fail("function parameter locals retain runtime type rules");
	}

	arena_destroy(&arena);
}

static void test_ir_loads_capture_per_reference(elf_State *state)
{
	Arena arena = arena_create(0);
	IrTestResult result = ir_test_lower_source(state, &arena, "outer := fun(value) { ret fun() { ret value + value } }");
	IrFunction *function = ir_test_function_at(result.module, 2);
	if (!function || function->capture_count != 1 || !function->first_capture || function->first_capture != function->last_capture) {
		test_fail("nested function records one capture descriptor");
		arena_destroy(&arena);
		return;
	}

	Ir ret = ir_test_body_stat(function->body, 0, IR_RETURN, "capturing function lowers return");
	Ir add = ret ? ret->ir_return.expr : 0;
	Ir left = add && add->kind == IR_ADD ? add->ir_binary.x : 0;
	Ir right = add && add->kind == IR_ADD ? add->ir_binary.y : 0;
	expect_ir_kind(left, IR_LOAD_CAPTURE, "first capture reference lowers to capture load");
	expect_ir_kind(right, IR_LOAD_CAPTURE, "second capture reference lowers to capture load");
	if (!left || !right || left == right || left->ir_load_capture != function->first_capture || right->ir_load_capture != function->first_capture) {
		test_fail("capture references use distinct loads of one descriptor");
	}
	if (!left || !right || left->site.offset == right->site.offset) {
		test_fail("capture loads retain their source sites");
	}

	arena_destroy(&arena);
}

static void test_ir_loads_implicit_this(elf_State *state)
{
	Arena arena = arena_create(0);
	IrTestResult result = ir_test_lower_source(state, &arena, "get_this := fun() { ret this }");
	IrFunction *function = ir_test_function_at(result.module, 1);
	Ir ret = function ? ir_test_body_stat(function->body, 0, IR_RETURN, "this function lowers return") : 0;
	Ir load = ret ? ret->ir_return.expr : 0;
	expect_ir_kind(load, IR_LOAD, "this lowers to a local load");
	if (!function || !load || load->ir_load.local != function->params[IMPLICIT_PARAM_INDEX]) {
		test_fail("this loads the function's implicit receiver parameter");
	}
	arena_destroy(&arena);
}

static void test_ir_rejects_this_assignment(elf_State *state)
{
	Arena arena = arena_create(0);
	log_set_hook(ignore_expected_lowering_error, 0);
	IrTestResult result = ir_test_lower_source(state, &arena, "set_this := fun() { this = nil }");
	log_set_hook(0, 0);
	Com_Message *message = result.source ? result.source->report.first : 0;
	if (!message || result.source->report.error_count != 1 || message->code != ERROR_INVALID_ASSIGNMENT || message->phase != ERROR_PHASE_LOWERING) {
		test_fail("assigning this reports an invalid assignment");
	}
	arena_destroy(&arena);
}

static void test_ir_reports_cross_function_capture(elf_State *state)
{
	Arena arena = arena_create(0);
	log_set_hook(ignore_expected_lowering_error, 0);
	IrTestResult result = ir_test_lower_source(state, &arena, "main :: fun(a, b = a) {}\nmain(1)");
	log_set_hook(0, 0);
	Com_Message *message = result.source ? result.source->report.first : 0;
	if (!message || result.source->report.error_count != 1 || message->code != ERROR_LOWERING || message->phase != ERROR_PHASE_LOWERING) {
		test_fail("cross-function capture reports a lowering error");
	}
	if (!result.module || !result.module->first_function || result.module->first_function->capture_count != 0) {
		test_fail("invalid capture does not alter the entry function captures");
	}
	arena_destroy(&arena);
}

static void test_ir_rejects_unknown_parameter_type(elf_State *state)
{
	Arena arena = arena_create(0);
	log_set_hook(ignore_expected_lowering_error, 0);
	IrTestResult result = ir_test_lower_source(state, &arena, "probe := fun(value: mystery) { ret value }\nret probe");
	log_set_hook(0, 0);
	if (!result.module || result.source->report.error_count != 1) {
		test_fail("unknown function parameter type reports one lowering error");
	}
	arena_destroy(&arena);
}

static void test_ir_lowers_static_forward_reference(elf_State *state)
{
	Arena arena = arena_create(0);
	IrTestResult result = ir_test_lower_source(state, &arena, "first :: second\nsecond :: 42\nret first");
	Atom *first_name = atom_from_data_size(result.compiler->atoms, "first", sizeof("first") - 1);
	Atom *second_name = atom_from_data_size(result.compiler->atoms, "second", sizeof("second") - 1);
	Entity *first = result.source->scope ? scope_find_current(result.source->scope, first_name) : 0;
	Entity *second = result.source->scope ? scope_find_current(result.source->scope, second_name) : 0;
	if (!first || !second || first->kind != ENTITY_STATIC || second->kind != ENTITY_STATIC) {
		test_fail("checker retains source statics in the source scope");
	}
	Ir body = ir_test_main_body(result.module);
	Ir ret = ir_test_body_stat(body, 0, IR_RETURN, "static declarations emit no runtime statements");
	expect_ir_i64(ret ? ret->ir_return.expr : 0, 42, "static forward reference resolves to literal ir");
	arena_destroy(&arena);
}

static void test_ir_rejects_assignment_to_file_static(elf_State *state)
{
	Arena arena = arena_create(0);
	log_set_hook(ignore_expected_lowering_error, 0);
	IrTestResult result = ir_test_lower_source(state, &arena, "answer :: 42\nanswer = 0");
	log_set_hook(0, 0);
	if (!result.module || result.source->report.error_count != 1) {
		test_fail("assignment to a file static reports one lowering error");
	}
	Ir body = ir_test_main_body(result.module);
	if (!body || body->kind != IR_BLOCK || body->ir_block.stats.count != 1 || !body->ir_block.stats.items[0] || body->ir_block.stats.items[0]->kind != IR_ERROR) {
		test_fail("assignment to a file static lowers to an error");
	}
	arena_destroy(&arena);
}

static void test_ir_lowers_static_call_arguments(elf_State *state)
{
	Arena arena = arena_create(0);
	IrTestResult result = ir_test_lower_source(state, &arena, "build :: fun(required, width, title) { ret required }\nret build(1, 20, \"custom\")");
	if (!result.module || result.module->function_count != 2)
	{
		test_fail("static call argument lowering retains the target function");
		arena_destroy(&arena);
		return;
	}

	Ir body = result.module->first_function->body;
	Ir ret = ir_test_body_stat(body, 0, IR_RETURN, "static call lowers return");
	Ir call = ret ? ret->ir_return.expr : 0;
	expect_ir_kind(call, IR_CALL, "static call lowers call expression");
	expect_ir_kind(call ? call->ir_call.expr : 0, IR_FUNCTION, "static call retains function callee");
	if (!call || call->ir_call.args.count != 3)
	{
		test_fail("static call fills every declared parameter");
	}
	else
	{
		expect_ir_i64(call->ir_call.args.items[0], 1, "static call keeps positional argument");
		expect_ir_i64(call->ir_call.args.items[1], 20, "static call keeps second positional argument");
		expect_ir_atom(call->ir_call.args.items[2], "custom", "static call keeps third positional argument");
	}
	arena_destroy(&arena);
}

static void test_ir_lowers_variadic_static_call_arguments(elf_State *state)
{
	Arena arena = arena_create(0);
	IrTestResult result = ir_test_lower_source(state, &arena, "collect :: fun(first, ...) { ret first }\nret collect(1, 2, 3)");
	if (!result.module || result.source->report.error_count != 0 || result.module->function_count != 2)
	{
		test_fail("variadic static call accepts extra positional arguments");
		arena_destroy(&arena);
		return;
	}

	Ir ret = ir_test_body_stat(result.module->first_function->body, 0, IR_RETURN, "variadic static call lowers return");
	Ir call = ret ? ret->ir_return.expr : 0;
	if (!call || call->kind != IR_CALL || call->ir_call.args.count != 3) {
		test_fail("variadic static call retains all positional arguments");
	}
	arena_destroy(&arena);
}

static void test_ir_lowers_local_shadow_of_file_static(elf_State *state)
{
	Arena arena = arena_create(0);
	IrTestResult result = ir_test_lower_source(state, &arena, "root :: 7\n{\nroot := 48\nx := root * 2\n}\nret root");
	Ir body = ir_test_main_body(result.module);
	Ir nested = ir_test_body_stat(body, 0, IR_BLOCK, "nested local scope lowers to runtime block");
	Ir local = ir_test_body_stat(nested, 1, IR_LOCAL, "nested local declaration is retained");
	Ir mul = local ? local->ir_local.expr : 0;
	expect_ir_kind(mul, IR_MUL, "nested local participates in runtime expression");
	expect_load_local(mul ? mul->ir_binary.x : 0, ir_test_body_stat(nested, 0, IR_LOCAL, "nested local shadows file static"), "nested local shadows file static");
	expect_ir_i64(mul ? mul->ir_binary.y : 0, 2, "nested runtime expression keeps rhs");
	Ir ret = ir_test_body_stat(body, 1, IR_RETURN, "outer static remains visible after nested scope");
	expect_ir_i64(ret ? ret->ir_return.expr : 0, 7, "nested local does not replace file static");
	arena_destroy(&arena);
}

static void test_ir_lowers_get_mem_macro(elf_State *state)
{
	Arena arena = arena_create(0);
	IrTestResult result = ir_test_lower_source(state, &arena, "x := 1\nret #get_mem(x)");
	Ir body = ir_test_main_body(result.module);

	Ir memory = ir_test_body_stat(body, 0, IR_LOCAL, "get_mem keeps local declaration");
	Ir ret = ir_test_body_stat(body, 1, IR_RETURN, "get_mem lowers return");
	Ir get_mem = ret ? ret->ir_return.expr : 0;
	expect_ir_kind(get_mem, IR_GET_MEM, "get_mem lowers to get_mem ir");
	expect_load_local(get_mem ? get_mem->ir_unary : 0, memory, "get_mem targets local load");

	arena_destroy(&arena);
}

static void run_ir_tests(elf_State *state)
{
	test_ir_lowers_declaration_expression(state);
	test_ir_lowers_bitwise_not(state);
	test_ir_lowers_tuple_expression(state);
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
	test_ir_reports_nil_assignment_arity(state);
	test_ir_lowers_function_expression(state);
	test_ir_loads_implicit_this(state);
	test_ir_rejects_this_assignment(state);
	test_ir_loads_capture_per_reference(state);
	test_ir_reports_cross_function_capture(state);
	test_ir_rejects_unknown_parameter_type(state);
	test_ir_lowers_static_forward_reference(state);
	test_ir_rejects_assignment_to_file_static(state);
	test_ir_lowers_static_call_arguments(state);
	test_ir_lowers_variadic_static_call_arguments(state);
	test_ir_lowers_local_shadow_of_file_static(state);
	test_ir_lowers_get_mem_macro(state);
}
