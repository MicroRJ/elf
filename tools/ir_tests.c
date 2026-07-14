static LowerContext *ir_test_lower_source(elf_State *state, Arena *arena, const char *source_text)
{
	SourceBuffer source = {(char *)source_text, (u64)strlen(source_text)};
	Parser *parser = elf_create_parser(state, arena, "ir_tests", source);
	AstRef file = elf_parse_file(parser);

	LowerContext *ctx = elf_create_lower_context(state, arena);
	elf_lower_ast_file(ctx, file);
	return ctx;
}

static FunctionIR *ir_test_main_function(LowerContext *ctx)
{
	if (!ctx || ctx->num_functions != 1) {
		test_fail("lowering produced one main function");
		return 0;
	}
	return &ctx->functions[0];
}

static IR ir_test_main_body(LowerContext *ctx)
{
	FunctionIR *function = ir_test_main_function(ctx);
	if (!function) {
		return 0;
	}
	if (!function->body || function->body->kind != IR_BLOCK) {
		test_fail("lowering produced main block");
		return 0;
	}
	return function->body;
}

static IR ir_test_body_stat(IR body, u32 index, IRKind kind, const char *label)
{
	if (!body || body->kind != IR_BLOCK || index >= body->ir_block.stats.count) {
		test_fail(label);
		return 0;
	}
	IR stat = body->ir_block.stats.items[index];
	if (!stat || stat->kind != kind) {
		test_fail(label);
		return 0;
	}
	return stat;
}

static void expect_ir_kind(IR ir, IRKind kind, const char *label)
{
	if (!ir || ir->kind != kind) {
		test_fail(label);
	}
}

static void expect_ir_i64(IR ir, i64 value, const char *label)
{
	if (!ir || ir->kind != IR_INTEGER || ir->ir_int != value) {
		test_fail(label);
	}
}

static void expect_ir_atom(IR ir, const char *text, const char *label)
{
	if (!ir || ir->kind != IR_ATOM || !ir->atom || strcmp(elf_atom_data(ir->atom), text) != 0) {
		test_fail(label);
	}
}

static void expect_load_local(IR ir, IR local, const char *label)
{
	if (!ir || ir->kind != IR_LOAD_LOCAL || ir->ir_load_local.local != local) {
		test_fail(label);
	}
}

static void test_ir_lowers_declaration_expression(elf_State *state)
{
	Arena arena = create_arena(0);
	LowerContext *ctx = ir_test_lower_source(state, &arena, "x := 1 + 2 * 3");
	IR body = ir_test_main_body(ctx);
	IR memory = ir_test_body_stat(body, 0, IR_LOCAL, "lower declaration to local");

	IR expr = memory->ir_local.expr;
	expect_ir_kind(expr, IR_ADD, "lower addition");
	expect_ir_i64(expr->ir_binary.x, 1, "lower add lhs");
	expect_ir_kind(expr->ir_binary.y, IR_MUL, "lower multiply rhs");
	expect_ir_i64(expr->ir_binary.y->ir_binary.x, 2, "lower multiply lhs");
	expect_ir_i64(expr->ir_binary.y->ir_binary.y, 3, "lower multiply rhs");
	destroy_arena(&arena);
}

static void test_ir_lowers_assignment_to_local(elf_State *state)
{
	Arena arena = create_arena(0);
	LowerContext *ctx = ir_test_lower_source(state, &arena, "x := 1\nx = 2");
	IR body = ir_test_main_body(ctx);
	IR memory = ir_test_body_stat(body, 0, IR_LOCAL, "lower local declaration");
	IR store = ir_test_body_stat(body, 1, IR_STORE, "lower assignment to store");

	expect_load_local(store ? store->ir_binary.x : 0, memory, "assignment destination loads local");
	expect_ir_i64(store->ir_binary.y, 2, "assignment expression lowered");
	destroy_arena(&arena);
}

static void test_ir_lowers_atoms_and_globals(elf_State *state)
{
	Arena arena = create_arena(0);
	LowerContext *ctx = ir_test_lower_source(state, &arena, "ret \"hello\"\nret foo");
	IR body = ir_test_main_body(ctx);

	IR ret_atom = ir_test_body_stat(body, 0, IR_RETURN, "lower atom return");
	expect_ir_atom(ret_atom->ir_return.expr, "hello", "string literal lowers to atom ir");

	IR ret_global = ir_test_body_stat(body, 1, IR_RETURN, "lower global return");
	expect_ir_kind(ret_global->ir_return.expr, IR_LOAD_GLOBAL, "identifier without local lowers to global load");
	destroy_arena(&arena);
}

static void test_ir_lowers_field_and_call(elf_State *state)
{
	Arena arena = create_arena(0);
	LowerContext *ctx = ir_test_lower_source(state, &arena, "ret obj.name\nmake {x = 1}");
	IR body = ir_test_main_body(ctx);

	IR ret = ir_test_body_stat(body, 0, IR_RETURN, "lower field return");
	IR field = ret->ir_return.expr;
	expect_ir_kind(field, IR_FIELD, "field access lowers to field ir");
	expect_ir_kind(field->ir_binary.x, IR_LOAD_GLOBAL, "field object lowers to global load");
	expect_ir_atom(field->ir_binary.y, "name", "field key lowers to atom ir");

	IR call = ir_test_body_stat(body, 1, IR_CALL, "lower call expression statement");
	expect_ir_kind(call->ir_call.expr, IR_LOAD_GLOBAL, "call callee lowers to global load");
	if (call && call->ir_call.args.count != 1) {
		test_fail("call keeps one table argument");
	}
	IR table_block = call->ir_call.args.items[0];
	expect_ir_kind(table_block, IR_EXPR_BLOCK, "call table argument lowers to expression block");
	IR table_local = table_block && table_block->kind == IR_EXPR_BLOCK && table_block->ir_expr_block.stats.count
		? table_block->ir_expr_block.stats.items[0]
		: 0;
	expect_ir_kind(table_local, IR_LOCAL, "table expression block creates table local");
	expect_ir_kind(table_local ? table_local->ir_local.expr : 0, IR_TABLE, "table expression block creates table ir");
	destroy_arena(&arena);
}

static void test_ir_lowers_if_else(elf_State *state)
{
	Arena arena = create_arena(0);
	LowerContext *ctx = ir_test_lower_source(state, &arena, "if flag ? { ret 1 } else { ret 2 }");
	IR body = ir_test_main_body(ctx);
	IR ir_if = ir_test_body_stat(body, 0, IR_IF, "lower if statement");

	expect_ir_kind(ir_if->ir_if.pred, IR_LOAD_GLOBAL, "if predicate lowers to global load");
	expect_ir_kind(ir_if->ir_if.true_clause, IR_BLOCK, "if true clause lowers to block");
	expect_ir_kind(ir_if->ir_if.else_clause, IR_BLOCK, "if else clause lowers to block");
	destroy_arena(&arena);
}

static void test_ir_lowers_while(elf_State *state)
{
	Arena arena = create_arena(0);
	LowerContext *ctx = ir_test_lower_source(state, &arena, "while flag ? { x = 1 }");
	IR body = ir_test_main_body(ctx);
	IR loop = ir_test_body_stat(body, 0, IR_BLOCK, "while lowers to label block");

	expect_ir_kind(ir_test_body_stat(loop, 0, IR_LABEL, "while starts with label"), IR_LABEL, "while start label");
	IR branch = ir_test_body_stat(loop, 1, IR_JUMP_IF_FALSE, "while branches to exit");
	expect_ir_kind(branch ? branch->ir_jump_if_false.pred : 0, IR_LOAD_GLOBAL, "while predicate lowers to global load");
	IR loop_body = ir_test_body_stat(loop, 2, IR_BLOCK, "while body lowers to block");
	expect_ir_kind(ir_test_body_stat(loop, 3, IR_JUMP, "while jumps back to start"), IR_JUMP, "while back jump");
	expect_ir_kind(ir_test_body_stat(loop, 4, IR_LABEL, "while ends with exit label"), IR_LABEL, "while exit label");

	IR store = ir_test_body_stat(loop_body, 0, IR_STORE, "while body lowers statements");
	expect_ir_kind(store->ir_binary.x, IR_LOAD_GLOBAL, "while body assignment destination");
	expect_ir_i64(store->ir_binary.y, 1, "while body assignment value");
	destroy_arena(&arena);
}

static void test_ir_lowers_nil_assign(elf_State *state)
{
	Arena arena = create_arena(0);
	LowerContext *ctx = ir_test_lower_source(state, &arena, "x := nil\nx ?= 1");
	IR body = ir_test_main_body(ctx);

	IR memory = ir_test_body_stat(body, 0, IR_LOCAL, "nil assignment keeps declaration local");
	IR ir_if = ir_test_body_stat(body, 1, IR_IF, "nil assignment lowers to if");

	IR pred = ir_if->ir_if.pred;
	expect_ir_kind(pred, IR_EQ, "nil assignment predicate compares against nil");
	expect_load_local(pred ? pred->ir_binary.x : 0, memory, "nil assignment predicate reads destination local");
	expect_ir_kind(pred ? pred->ir_binary.y : 0, IR_NIL, "nil assignment predicate rhs is nil");

	IR true_clause = ir_if->ir_if.true_clause;
	expect_ir_kind(true_clause, IR_BLOCK, "nil assignment true clause is block");
	IR store = ir_test_body_stat(true_clause, 0, IR_STORE, "nil assignment true clause stores value");
	expect_load_local(store ? store->ir_binary.x : 0, memory, "nil assignment store writes destination local");
	expect_ir_i64(store ? store->ir_binary.y : 0, 1, "nil assignment store value");
	if (ir_if && ir_if->ir_if.else_clause) {
		test_fail("nil assignment has no else clause");
	}
	destroy_arena(&arena);
}

static void test_ir_lowers_nil_assign_field_once(elf_State *state)
{
	Arena arena = create_arena(0);
	LowerContext *ctx = ir_test_lower_source(state, &arena, "obj := nil\nobj.child.name ?= 1");
	IR body = ir_test_main_body(ctx);

	IR object_memory = ir_test_body_stat(body, 0, IR_LOCAL, "field nil assignment keeps object declaration");
	IR receiver_memory = ir_test_body_stat(body, 1, IR_LOCAL, "field nil assignment saves receiver once");
	IR ir_if = ir_test_body_stat(body, 2, IR_IF, "field nil assignment lowers to if");

	IR receiver_expr = receiver_memory ? receiver_memory->ir_local.expr : 0;
	expect_ir_kind(receiver_expr, IR_FIELD, "field nil assignment temp stores receiver field");
	expect_load_local(receiver_expr ? receiver_expr->ir_binary.x : 0, object_memory, "field nil assignment receiver starts from object local");
	expect_ir_atom(receiver_expr ? receiver_expr->ir_binary.y : 0, "child", "field nil assignment receiver field name");

	IR pred = ir_if ? ir_if->ir_if.pred : 0;
	expect_ir_kind(pred, IR_EQ, "field nil assignment predicate compares against nil");
	IR pred_field = pred ? pred->ir_binary.x : 0;
	expect_ir_kind(pred_field, IR_FIELD, "field nil assignment predicate reads final field");
	expect_load_local(pred_field ? pred_field->ir_binary.x : 0, receiver_memory, "field nil assignment predicate reuses receiver temp");
	expect_ir_atom(pred_field ? pred_field->ir_binary.y : 0, "name", "field nil assignment final field name");

	IR true_clause = ir_if ? ir_if->ir_if.true_clause : 0;
	IR store = ir_test_body_stat(true_clause, 0, IR_STORE, "field nil assignment true clause stores value");
	IR store_field = store ? store->ir_binary.x : 0;
	expect_ir_kind(store_field, IR_FIELD, "field nil assignment store writes final field");
	expect_load_local(store_field ? store_field->ir_binary.x : 0, receiver_memory, "field nil assignment store reuses receiver temp");
	expect_ir_i64(store ? store->ir_binary.y : 0, 1, "field nil assignment store value");
	destroy_arena(&arena);
}

static void test_ir_lowers_function_expression(elf_State *state)
{
	Arena arena = create_arena(0);
	LowerContext *ctx = ir_test_lower_source(state, &arena, "add := fun(a, b) { ret a + b }");

	if (!ctx || ctx->num_functions != 2) {
		test_fail("function expression adds bytecode function");
		destroy_arena(&arena);
		return;
	}

	IR main_body = ctx->functions[0].body;
	IR memory = ir_test_body_stat(main_body, 0, IR_LOCAL, "function declaration lowers to local");
	expect_ir_kind(memory ? memory->ir_local.expr : 0, IR_FUNCTION, "function expression lowers to function ir");
	if (memory && memory->ir_local.expr && memory->ir_local.expr->ir_function != 1) {
		test_fail("function ir points at nested function");
	}

	FunctionIR *function = &ctx->functions[1];
	if (function->arity != IMPLICIT_PARAM_COUNT + 2) {
		test_fail("function arity includes implicit this and params");
	}

	IR body = function->body;
	IR ret = ir_test_body_stat(body, 0, IR_RETURN, "function body lowers return");
	IR add = ret ? ret->ir_return.expr : 0;
	expect_ir_kind(add, IR_ADD, "function return lowers expression");
	expect_ir_kind(add ? add->ir_binary.x : 0, IR_LOAD_LOCAL, "first param lowers to local load");
	expect_ir_kind(add ? add->ir_binary.y : 0, IR_LOAD_LOCAL, "second param lowers to local load");
	IR first_param = add && add->ir_binary.x ? add->ir_binary.x->ir_load_local.local : 0;
	IR second_param = add && add->ir_binary.y ? add->ir_binary.y->ir_load_local.local : 0;
	if (first_param && first_param->ir_local.slot.slot != 1) {
		test_fail("first param uses frame slot 1");
	}
	if (second_param && second_param->ir_local.slot.slot != 2) {
		test_fail("second param uses frame slot 2");
	}

	destroy_arena(&arena);
}

static void test_ir_lowers_get_mem_macro(elf_State *state)
{
	Arena arena = create_arena(0);
	LowerContext *ctx = ir_test_lower_source(state, &arena, "x := 1\nret #get_mem(x)");
	IR body = ir_test_main_body(ctx);

	IR memory = ir_test_body_stat(body, 0, IR_LOCAL, "get_mem keeps local declaration");
	IR ret = ir_test_body_stat(body, 1, IR_RETURN, "get_mem lowers return");
	IR get_mem = ret ? ret->ir_return.expr : 0;
	expect_ir_kind(get_mem, IR_GET_MEM, "get_mem lowers to get_mem ir");
	expect_load_local(get_mem ? get_mem->ir_unary : 0, memory, "get_mem targets local load");

	destroy_arena(&arena);
}

static void run_ir_tests(elf_State *state)
{
	test_ir_lowers_declaration_expression(state);
	test_ir_lowers_assignment_to_local(state);
	test_ir_lowers_atoms_and_globals(state);
	test_ir_lowers_field_and_call(state);
	test_ir_lowers_if_else(state);
	test_ir_lowers_while(state);
	test_ir_lowers_nil_assign(state);
	test_ir_lowers_nil_assign_field_once(state);
	test_ir_lowers_function_expression(state);
	test_ir_lowers_get_mem_macro(state);
}
