static AstRef parser_test_parse_file(elf_State *state, const char *source_text)
{
	elf_Arena arena = elf_arena_create(0);
	elf_StrSlice source = {(char *)source_text, (u64)strlen(source_text)};
	Parser *parser = elf_create_parser(state, &arena, "parser_tests", source);
	AstRef file = elf_parse_file(parser);
	return file;
}

static AstRef parser_test_body(AstRef file)
{
	if (!file || file->kind != AST_FILE) {
		test_fail("parser produced file ast");
		return 0;
	}
	if (!file->file.body || file->file.body->kind != AST_BLOCK_STAT) {
		test_fail("parser produced file block");
		return 0;
	}
	return file->file.body;
}

static AstRef parser_test_stat(AstRef file, u32 index, AstType kind, const char *label)
{
	AstRef body = parser_test_body(file);
	if (!body) {
		return 0;
	}
	if (index >= body->block.nstats) {
		test_fail(label);
		return 0;
	}
	AstRef stat = body->block.stats[index];
	if (!stat || stat->kind != kind) {
		test_fail(label);
		return 0;
	}
	return stat;
}

static AstRef parser_test_tuple_item(AstRef tuple, u32 index, AstType kind, const char *label)
{
	if (!tuple || tuple->kind != AST_TUPLE || index >= tuple->tuple.nargs) {
		test_fail(label);
		return 0;
	}
	AstRef item = tuple->tuple.args[index];
	if (!item || item->kind != kind) {
		test_fail(label);
		return 0;
	}
	return item;
}

static void expect_ast_kind(AstRef ast, AstType kind, const char *label)
{
	if (!ast || ast->kind != kind) {
		test_fail(label);
	}
}

static void expect_ast_atom(AstRef ast, const char *text, const char *label)
{
	if (!ast || !ast->atom || strcmp(atom_data(ast->atom), text) != 0) {
		test_fail(label);
	}
}

static void expect_ast_i64(AstRef ast, i64 value, const char *label)
{
	if (!ast || ast->kind != AST_INTEGER_LITERAL || ast->integer_value != value) {
		test_fail(label);
	}
}

static void test_parser_decl_precedence_and_atoms(elf_State *state)
{
	AstRef file = parser_test_parse_file(state, "x := 1 + 2 * 3");
	AstRef decl = parser_test_stat(file, 0, AST_DECL_STAT, "parse declaration statement");

	AstRef name = parser_test_tuple_item(decl->decl.name, 0, AST_IDENT, "parse declaration name");
	expect_ast_atom(name, "x", "identifier ast stores atom");

	AstRef expr = parser_test_tuple_item(decl->decl.expr, 0, AST_ADD, "parse additive expression");
	expect_ast_kind(expr->binary.x, AST_INTEGER_LITERAL, "parse add lhs");
	expect_ast_i64(expr->binary.x, 1, "parse add lhs integer");
	expect_ast_kind(expr->binary.y, AST_MUL, "multiplication binds tighter than addition");
	expect_ast_i64(expr->binary.y->binary.x, 2, "parse multiply lhs integer");
	expect_ast_i64(expr->binary.y->binary.y, 3, "parse multiply rhs integer");
}

static void test_parser_string_atoms(elf_State *state)
{
	AstRef file = parser_test_parse_file(state, "name := \"hello\"");
	AstRef decl = parser_test_stat(file, 0, AST_DECL_STAT, "parse string declaration");
	AstRef string = parser_test_tuple_item(decl->decl.expr, 0, AST_STRING_LITERAL, "parse string literal");
	expect_ast_atom(string, "hello", "string ast stores atom");
}

static void test_parser_if_else_blocks(elf_State *state)
{
	AstRef file = parser_test_parse_file(state, "if flag ? { ret 1 } else { ret nil }");
	AstRef stat = parser_test_stat(file, 0, AST_IF, "parse if statement");

	expect_ast_kind(stat->if_stat.pred, AST_IDENT, "parse if predicate");
	expect_ast_atom(stat->if_stat.pred, "flag", "parse if predicate atom");
	expect_ast_kind(stat->if_stat.true_clause, AST_BLOCK_STAT, "parse if true block");
	expect_ast_kind(stat->if_stat.else_clause, AST_BLOCK_STAT, "parse if else block");

	AstRef ret = stat->if_stat.true_clause->block.stats[0];
	expect_ast_kind(ret, AST_RETURN, "parse return in true block");
	AstRef value = parser_test_tuple_item(ret->return_stat.expr, 0, AST_INTEGER_LITERAL, "parse return value");
	expect_ast_i64(value, 1, "parse return integer");
}

static void test_parser_call_with_table_argument(elf_State *state)
{
	AstRef file = parser_test_parse_file(state, "make {x = 1, 2}");
	AstRef stat = parser_test_stat(file, 0, AST_TUPLE, "parse call expression statement tuple");
	AstRef call = parser_test_tuple_item(stat, 0, AST_CALL, "parse call statement");

	expect_ast_kind(call->call.expr, AST_IDENT, "parse call callee");
	expect_ast_atom(call->call.expr, "make", "parse call callee atom");
	if (call->call.nargs != 1) {
		test_fail("table call shorthand counts argument");
		return;
	}

	AstRef table = call->call.args[0];
	expect_ast_kind(table, AST_TABLE, "parse table argument");
	if (table->table.nargs != 2) {
		test_fail("table parser counts entries");
	}
}

static void test_parser_nil_assign(elf_State *state)
{
	AstRef file = parser_test_parse_file(state, "x ?= 1");
	AstRef stat = parser_test_stat(file, 0, AST_NIL_ASSIGN, "parse nil assignment statement");

	AstRef name = parser_test_tuple_item(stat->binary.x, 0, AST_IDENT, "parse nil assignment destination");
	expect_ast_atom(name, "x", "nil assignment destination stores atom");

	AstRef expr = parser_test_tuple_item(stat->binary.y, 0, AST_INTEGER_LITERAL, "parse nil assignment expression");
	expect_ast_i64(expr, 1, "nil assignment expression value");
}

static void test_parser_table_access_modes(elf_State *state)
{
	AstRef file = parser_test_parse_file(state, "ret t[0]\nret t.[key]\nret t.name");

	AstRef array_ret = parser_test_stat(file, 0, AST_RETURN, "parse array index return");
	AstRef array_index = parser_test_tuple_item(array_ret->return_stat.expr, 0, AST_INDEX, "plain brackets parse as array index");
	expect_ast_kind(array_index ? array_index->binary.x : 0, AST_IDENT, "array index keeps receiver");
	expect_ast_i64(array_index ? array_index->binary.y : 0, 0, "array index keeps integer index");

	AstRef computed_ret = parser_test_stat(file, 1, AST_RETURN, "parse computed field return");
	AstRef computed_field = parser_test_tuple_item(computed_ret->return_stat.expr, 0, AST_FIELD, "dot brackets parse as computed field");
	expect_ast_kind(computed_field ? computed_field->binary.x : 0, AST_IDENT, "computed field keeps receiver");
	expect_ast_kind(computed_field ? computed_field->binary.y : 0, AST_IDENT, "computed field keeps key expression");
	expect_ast_atom(computed_field ? computed_field->binary.y : 0, "key", "computed field key identifier");

	AstRef dot_ret = parser_test_stat(file, 2, AST_RETURN, "parse dot field return");
	AstRef dot_field = parser_test_tuple_item(dot_ret->return_stat.expr, 0, AST_FIELD, "dot identifier parses as field");
	expect_ast_atom(dot_field ? dot_field->binary.y : 0, "name", "dot field key identifier");
}

static void test_parser_function_expression(elf_State *state)
{
	AstRef file = parser_test_parse_file(state, "add := fun(a, b) { ret a + b }");
	AstRef decl = parser_test_stat(file, 0, AST_DECL_STAT, "parse function declaration");
	AstRef function = parser_test_tuple_item(decl->decl.expr, 0, AST_FUNCTION, "parse function expression");

	if (!function) {
		return;
	}
	if (function->function.nparams != 2) {
		test_fail("function expression keeps parameters");
		return;
	}

	AstRef first_param = function->function.params[0];
	AstRef second_param = function->function.params[1];
	expect_ast_kind(first_param, AST_FUNCTION_PARAM, "parse first function param");
	expect_ast_kind(second_param, AST_FUNCTION_PARAM, "parse second function param");
	expect_ast_atom(first_param ? first_param->param.name : 0, "a", "first function param name");
	expect_ast_atom(second_param ? second_param->param.name : 0, "b", "second function param name");
	expect_ast_kind(function->function.body, AST_BLOCK_STAT, "function body parses as block");

	file = parser_test_parse_file(state, "collect := fun(a, ...) { ret elf.varg(0) }");
	decl = parser_test_stat(file, 0, AST_DECL_STAT, "parse variadic function declaration");
	function = parser_test_tuple_item(decl->decl.expr, 0, AST_FUNCTION, "parse variadic function expression");
	if (!function) {
		return;
	}

	if (function->function.nparams != 1) {
		test_fail("variadic marker is not counted as a named parameter");
		return;
	}

	expect_ast_atom(function->function.params[0] ? function->function.params[0]->param.name : 0, "a", "variadic function named parameter");
	expect_ast_kind(function->function.variadic, AST_ELLIPSIS, "function tracks variadic marker separately");
}

static void test_parser_get_mem_macro(elf_State *state)
{
	AstRef file = parser_test_parse_file(state, "ret #get_mem(x)");
	AstRef ret = parser_test_stat(file, 0, AST_RETURN, "parse get_mem return");
	AstRef expr = parser_test_tuple_item(ret->return_stat.expr, 0, AST_GET_MEM, "parse get_mem macro expression");

	expect_ast_kind(expr ? expr->unary : 0, AST_IDENT, "get_mem keeps target expression");
	expect_ast_atom(expr ? expr->unary : 0, "x", "get_mem target atom");
}

static void test_parser_integer_literal_boundaries(elf_State *state)
{
	AstRef file = parser_test_parse_file(state, "ret 9223372036854775807\nret -9223372036854775808");

	AstRef max_return = parser_test_stat(file, 0, AST_RETURN, "parse i64 max return");
	AstRef max_value = parser_test_tuple_item(max_return->return_stat.expr, 0, AST_INTEGER_LITERAL, "parse i64 max literal");
	expect_ast_i64(max_value, 9223372036854775807LL, "i64 max literal value");

	AstRef min_return = parser_test_stat(file, 1, AST_RETURN, "parse i64 min return");
	AstRef min_value = parser_test_tuple_item(min_return->return_stat.expr, 0, AST_INTEGER_LITERAL, "parse i64 min literal");
	expect_ast_i64(min_value, -9223372036854775807LL - 1LL, "i64 min literal value");
}

static void test_parser_source_slices(elf_State *state)
{
	AstRef file = parser_test_parse_file(state, "alpha := 1\n  beta := 2");
	AstRef first = parser_test_stat(file, 0, AST_DECL_STAT, "parse first declaration");
	AstRef second = parser_test_stat(file, 1, AST_DECL_STAT, "parse second declaration");

	if (first && first->site.line_index != 1) {
		test_fail("first statement source line");
	}
	if (second && second->site.line_index != 2) {
		test_fail("second statement source line");
	}
}

static void run_parser_tests(elf_State *state)
{
	test_parser_decl_precedence_and_atoms(state);
	test_parser_string_atoms(state);
	test_parser_if_else_blocks(state);
	test_parser_call_with_table_argument(state);
	test_parser_nil_assign(state);
	test_parser_table_access_modes(state);
	test_parser_function_expression(state);
	test_parser_get_mem_macro(state);
	test_parser_integer_literal_boundaries(state);
	test_parser_source_slices(state);
}
