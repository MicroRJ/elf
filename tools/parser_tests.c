static Ast parser_test_parse_file(elf_State *state, const char *source_text)
{
	elf_Arena arena = elf_arena_create(0);
	elf_StrSlice source = {(char *)source_text, (u64)strlen(source_text)};
	Parser *parser = elf_create_parser(state, &arena, "parser_tests", source);
	Ast file = elf_parse_file(parser);
	return file;
}

static Ast parser_test_body(Ast file)
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

static Ast parser_test_stat(Ast file, u32 index, AstType kind, const char *label)
{
	Ast body = parser_test_body(file);
	if (!body) {
		return 0;
	}
	if (index >= body->block.nstats) {
		test_fail(label);
		return 0;
	}
	Ast stat = body->block.stats[index];
	if (!stat || stat->kind != kind) {
		test_fail(label);
		return 0;
	}
	return stat;
}

static Ast parser_test_tuple_item(Ast tuple, u32 index, AstType kind, const char *label)
{
	if (!tuple || tuple->kind != AST_TUPLE || index >= tuple->tuple.nargs) {
		test_fail(label);
		return 0;
	}
	Ast item = tuple->tuple.args[index];
	if (!item || item->kind != kind) {
		test_fail(label);
		return 0;
	}
	return item;
}

static void expect_ast_kind(Ast ast, AstType kind, const char *label)
{
	if (!ast || ast->kind != kind) {
		test_fail(label);
	}
}

static void expect_ast_atom(Ast ast, const char *text, const char *label)
{
	if (!ast || !ast->atom || strcmp(atom_data(ast->atom), text) != 0) {
		test_fail(label);
	}
}

static void expect_ast_i64(Ast ast, i64 value, const char *label)
{
	if (!ast || ast->kind != AST_INTEGER_LITERAL || ast->integer_value != value) {
		test_fail(label);
	}
}

static void test_parser_decl_precedence_and_atoms(elf_State *state)
{
	Ast file = parser_test_parse_file(state, "x := 1 + 2 * 3");
	Ast decl = parser_test_stat(file, 0, AST_DECL_STAT, "parse declaration statement");

	Ast name = parser_test_tuple_item(decl->decl.name, 0, AST_IDENT, "parse declaration name");
	expect_ast_atom(name, "x", "identifier ast stores atom");

	Ast expr = parser_test_tuple_item(decl->decl.expr, 0, AST_ADD, "parse additive expression");
	expect_ast_kind(expr->binary.x, AST_INTEGER_LITERAL, "parse add lhs");
	expect_ast_i64(expr->binary.x, 1, "parse add lhs integer");
	expect_ast_kind(expr->binary.y, AST_MUL, "multiplication binds tighter than addition");
	expect_ast_i64(expr->binary.y->binary.x, 2, "parse multiply lhs integer");
	expect_ast_i64(expr->binary.y->binary.y, 3, "parse multiply rhs integer");
}

static void test_parser_constant_declaration(elf_State *state)
{
	Ast file = parser_test_parse_file(state, "answer ::= 42");
	Ast decl = parser_test_stat(file, 0, AST_DECL_STAT, "parse constant declaration");
	if (decl && !(decl->decl.tags & AST_DECL_TAG_CONSTANT)) {
		test_fail("hard bind marks declaration constant");
	}
}

static void test_parser_string_atoms(elf_State *state)
{
	Ast file = parser_test_parse_file(state, "name := \"hello\"");
	Ast decl = parser_test_stat(file, 0, AST_DECL_STAT, "parse string declaration");
	Ast string = parser_test_tuple_item(decl->decl.expr, 0, AST_STRING_LITERAL, "parse string literal");
	expect_ast_atom(string, "hello", "string ast stores atom");
}

static void test_parser_interpolated_strings(elf_State *state)
{
	Ast file = parser_test_parse_file(state,
		"message := f\"hello ${name}, count ${count + 1}\"");
	Ast decl = parser_test_stat(file, 0, AST_DECL_STAT, "parse interpolated string declaration");
	Ast string = parser_test_tuple_item(decl->decl.expr, 0, AST_INTERPOLATED_STRING,
		"parse interpolated string expression");
	if (!string) {
		return;
	}

	AstArray parts = string->interpolated_string;
	if (parts.nargs != 5) {
		test_fail("interpolated string keeps alternating text and expression parts");
		return;
	}
	expect_ast_kind(parts.args[0], AST_STRING_LITERAL, "interpolated string starts with text");
	expect_ast_atom(parts.args[0], "hello ", "interpolated string leading text");
	expect_ast_kind(parts.args[1], AST_IDENT, "interpolated string keeps first expression");
	expect_ast_atom(parts.args[1], "name", "interpolated string first expression identifier");
	expect_ast_kind(parts.args[2], AST_STRING_LITERAL, "interpolated string keeps middle text");
	expect_ast_atom(parts.args[2], ", count ", "interpolated string middle text payload");
	expect_ast_kind(parts.args[3], AST_ADD, "interpolated string parses full binary expression");
	expect_ast_kind(parts.args[3]->binary.x, AST_IDENT, "interpolated binary expression lhs");
	expect_ast_i64(parts.args[3]->binary.y, 1, "interpolated binary expression rhs");
	expect_ast_kind(parts.args[4], AST_STRING_LITERAL, "interpolated string ends with text");
	expect_ast_atom(parts.args[4], "", "interpolated string keeps empty trailing text");

	file = parser_test_parse_file(state, "ret f\"outer ${f\"inner ${value}\"}\"");
	Ast ret = parser_test_stat(file, 0, AST_RETURN, "parse nested interpolated string return");
	Ast outer = parser_test_tuple_item(ret->return_stat.expr, 0, AST_INTERPOLATED_STRING,
		"parse outer interpolated string");
	if (!outer || outer->interpolated_string.nargs != 3) {
		test_fail("outer interpolated string keeps nested expression");
		return;
	}
	Ast inner = outer->interpolated_string.args[1];
	expect_ast_kind(inner, AST_INTERPOLATED_STRING, "interpolated expression can contain interpolated string");
	if (inner && inner->interpolated_string.nargs == 3) {
		expect_ast_atom(inner->interpolated_string.args[0], "inner ", "nested interpolated leading text");
		expect_ast_atom(inner->interpolated_string.args[1], "value", "nested interpolated expression");
		expect_ast_atom(inner->interpolated_string.args[2], "", "nested interpolated trailing text");
	}
	else {
		test_fail("nested interpolated string keeps three parts");
	}

	file = parser_test_parse_file(state, "ret f\"plain\"");
	ret = parser_test_stat(file, 0, AST_RETURN, "parse plain format-prefixed string return");
	Ast plain = parser_test_tuple_item(ret->return_stat.expr, 0, AST_STRING_LITERAL,
		"format prefix without interpolation remains string literal");
	expect_ast_atom(plain, "plain", "plain format-prefixed string payload");
}

static void test_parser_if_else_blocks(elf_State *state)
{
	Ast file = parser_test_parse_file(state, "if flag ? { ret 1 } else { ret nil }");
	Ast stat = parser_test_stat(file, 0, AST_IF, "parse if statement");

	expect_ast_kind(stat->if_stat.pred, AST_IDENT, "parse if predicate");
	expect_ast_atom(stat->if_stat.pred, "flag", "parse if predicate atom");
	expect_ast_kind(stat->if_stat.true_clause, AST_BLOCK_STAT, "parse if true block");
	expect_ast_kind(stat->if_stat.else_clause, AST_BLOCK_STAT, "parse if else block");

	Ast ret = stat->if_stat.true_clause->block.stats[0];
	expect_ast_kind(ret, AST_RETURN, "parse return in true block");
	Ast value = parser_test_tuple_item(ret->return_stat.expr, 0, AST_INTEGER_LITERAL, "parse return value");
	expect_ast_i64(value, 1, "parse return integer");
}

static void test_parser_for_loops(elf_State *state)
{
	Ast file = parser_test_parse_file(state, "for key, value := 0 ... 4 ? {}");
	Ast loop = parser_test_stat(file, 0, AST_FOR_RANGE, "parse range for statement");
	if (!loop) {
		return;
	}

	Ast decl = loop->for_range_stat.decl;
	expect_ast_kind(decl, AST_DECL_STAT, "range for statement stores declaration");
	if (!decl) {
		return;
	}

	Ast key = parser_test_tuple_item(decl->decl.name, 0, AST_IDENT,
		"for declaration parses first identifier");
	Ast value = parser_test_tuple_item(decl->decl.name, 1, AST_IDENT,
		"for declaration parses second identifier");
	expect_ast_atom(key, "key", "for declaration keeps first identifier");
	expect_ast_atom(value, "value", "for declaration keeps second identifier");
	if (decl->decl.name->tuple.nargs != 2) {
		test_fail("for declaration contains only its identifier list");
	}
	Ast range = parser_test_tuple_item(decl->decl.expr, 0, AST_RANGE,
		"range for declaration stores one range expression");
	expect_ast_i64(range ? range->binary.x : 0, 0, "range for stores range start");
	expect_ast_i64(range ? range->binary.y : 0, 4, "range for stores range end");

	file = parser_test_parse_file(state, "for i := 0; i < 10; i += 1 ? {}");
	loop = parser_test_stat(file, 0, AST_FOR, "parse C-style for statement");
	if (!loop) {
		return;
	}

	expect_ast_kind(loop->for_stat.init, AST_DECL_STAT, "C-style for stores initializer");
	expect_ast_kind(loop->for_stat.pred, AST_LESS_THAN, "C-style for stores predicate");
	expect_ast_kind(loop->for_stat.step, AST_ADD_ASSIGN, "C-style for stores update statement");
	expect_ast_kind(loop->for_stat.body, AST_BLOCK_STAT, "C-style for stores body");
}

static void test_parser_call_with_table_argument(elf_State *state)
{
	Ast file = parser_test_parse_file(state, "make {x = 1, 2}");
	Ast stat = parser_test_stat(file, 0, AST_TUPLE, "parse call expression statement tuple");
	Ast call = parser_test_tuple_item(stat, 0, AST_CALL, "parse call statement");

	expect_ast_kind(call->call.expr, AST_IDENT, "parse call callee");
	expect_ast_atom(call->call.expr, "make", "parse call callee atom");
	if (call->call.nargs != 1) {
		test_fail("table call shorthand counts argument");
		return;
	}

	Ast table = call->call.args[0];
	expect_ast_kind(table, AST_TABLE, "parse table argument");
	if (table->table.nargs != 2) {
		test_fail("table parser counts entries");
	}
}

static void test_parser_nil_assign(elf_State *state)
{
	Ast file = parser_test_parse_file(state, "x ?= 1");
	Ast stat = parser_test_stat(file, 0, AST_NIL_ASSIGN, "parse nil assignment statement");

	Ast name = parser_test_tuple_item(stat->binary.x, 0, AST_IDENT, "parse nil assignment destination");
	expect_ast_atom(name, "x", "nil assignment destination stores atom");

	Ast expr = parser_test_tuple_item(stat->binary.y, 0, AST_INTEGER_LITERAL, "parse nil assignment expression");
	expect_ast_i64(expr, 1, "nil assignment expression value");
}

static void test_parser_table_access_modes(elf_State *state)
{
	Ast file = parser_test_parse_file(state, "ret t[0]\nret t.[key]\nret t.name");

	Ast array_ret = parser_test_stat(file, 0, AST_RETURN, "parse array index return");
	Ast array_index = parser_test_tuple_item(array_ret->return_stat.expr, 0, AST_INDEX, "plain brackets parse as array index");
	expect_ast_kind(array_index ? array_index->binary.x : 0, AST_IDENT, "array index keeps receiver");
	expect_ast_i64(array_index ? array_index->binary.y : 0, 0, "array index keeps integer index");

	Ast computed_ret = parser_test_stat(file, 1, AST_RETURN, "parse computed field return");
	Ast computed_field = parser_test_tuple_item(computed_ret->return_stat.expr, 0, AST_FIELD, "dot brackets parse as computed field");
	expect_ast_kind(computed_field ? computed_field->binary.x : 0, AST_IDENT, "computed field keeps receiver");
	expect_ast_kind(computed_field ? computed_field->binary.y : 0, AST_IDENT, "computed field keeps key expression");
	expect_ast_atom(computed_field ? computed_field->binary.y : 0, "key", "computed field key identifier");

	Ast dot_ret = parser_test_stat(file, 2, AST_RETURN, "parse dot field return");
	Ast dot_field = parser_test_tuple_item(dot_ret->return_stat.expr, 0, AST_FIELD, "dot identifier parses as field");
	expect_ast_atom(dot_field ? dot_field->binary.y : 0, "name", "dot field key identifier");
}

static void test_parser_open_range_indexes(elf_State *state)
{
	Ast file = parser_test_parse_file(state, "ret t[...]\nret t[2 ...]\nret t[... 4]");

	Ast full_ret = parser_test_stat(file, 0, AST_RETURN, "parse fully open range index return");
	Ast full_index = parser_test_tuple_item(full_ret->return_stat.expr, 0, AST_RANGE_INDEX,
		"fully open brackets parse as range index");
	Ast full_range = full_index ? full_index->binary.y : 0;
	expect_ast_kind(full_range, AST_RANGE, "fully open range index stores range");
	if (full_range && (full_range->binary.x || full_range->binary.y)) {
		test_fail("fully open range keeps both endpoints omitted");
	}

	Ast tail_ret = parser_test_stat(file, 1, AST_RETURN, "parse open-ended range index return");
	Ast tail_index = parser_test_tuple_item(tail_ret->return_stat.expr, 0, AST_RANGE_INDEX,
		"open-ended brackets parse as range index");
	Ast tail_range = tail_index ? tail_index->binary.y : 0;
	expect_ast_kind(tail_range, AST_RANGE, "open-ended range index stores range");
	expect_ast_i64(tail_range ? tail_range->binary.x : 0, 2, "open-ended range keeps lower bound");
	if (tail_range && tail_range->binary.y) {
		test_fail("open-ended range keeps upper bound omitted");
	}

	Ast prefix_ret = parser_test_stat(file, 2, AST_RETURN, "parse open-start range index return");
	Ast prefix_index = parser_test_tuple_item(prefix_ret->return_stat.expr, 0, AST_RANGE_INDEX,
		"open-start brackets parse as range index");
	Ast prefix_range = prefix_index ? prefix_index->binary.y : 0;
	expect_ast_kind(prefix_range, AST_RANGE, "open-start range index stores range");
	if (prefix_range && prefix_range->binary.x) {
		test_fail("open-start range keeps lower bound omitted");
	}
	expect_ast_i64(prefix_range ? prefix_range->binary.y : 0, 4, "open-start range keeps upper bound");
}

static void test_parser_function_expression(elf_State *state)
{
	Ast file = parser_test_parse_file(state, "add := fun(a, b) { ret a + b }");
	Ast decl = parser_test_stat(file, 0, AST_DECL_STAT, "parse function declaration");
	Ast function = parser_test_tuple_item(decl->decl.expr, 0, AST_FUNCTION, "parse function expression");

	if (!function) {
		return;
	}
	if (function->function.nparams != 2) {
		test_fail("function expression keeps parameters");
		return;
	}

	Ast first_param = function->function.params[0];
	Ast second_param = function->function.params[1];
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
	Ast file = parser_test_parse_file(state, "ret #get_mem(x)");
	Ast ret = parser_test_stat(file, 0, AST_RETURN, "parse get_mem return");
	Ast expr = parser_test_tuple_item(ret->return_stat.expr, 0, AST_GET_MEM, "parse get_mem macro expression");

	expect_ast_kind(expr ? expr->unary : 0, AST_IDENT, "get_mem keeps target expression");
	expect_ast_atom(expr ? expr->unary : 0, "x", "get_mem target atom");
}

static void test_parser_integer_literal_boundaries(elf_State *state)
{
	Ast file = parser_test_parse_file(state, "ret 9223372036854775807\nret -9223372036854775808");

	Ast max_return = parser_test_stat(file, 0, AST_RETURN, "parse i64 max return");
	Ast max_value = parser_test_tuple_item(max_return->return_stat.expr, 0, AST_INTEGER_LITERAL, "parse i64 max literal");
	expect_ast_i64(max_value, 9223372036854775807LL, "i64 max literal value");

	Ast min_return = parser_test_stat(file, 1, AST_RETURN, "parse i64 min return");
	Ast min_value = parser_test_tuple_item(min_return->return_stat.expr, 0, AST_INTEGER_LITERAL, "parse i64 min literal");
	expect_ast_i64(min_value, -9223372036854775807LL - 1LL, "i64 min literal value");
}

static void test_parser_source_slices(elf_State *state)
{
	Ast file = parser_test_parse_file(state, "alpha := 1\n  beta := 2");
	Ast first = parser_test_stat(file, 0, AST_DECL_STAT, "parse first declaration");
	Ast second = parser_test_stat(file, 1, AST_DECL_STAT, "parse second declaration");

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
	test_parser_constant_declaration(state);
	test_parser_string_atoms(state);
	test_parser_interpolated_strings(state);
	test_parser_if_else_blocks(state);
	test_parser_for_loops(state);
	test_parser_call_with_table_argument(state);
	test_parser_nil_assign(state);
	test_parser_table_access_modes(state);
	test_parser_open_range_indexes(state);
	test_parser_function_expression(state);
	test_parser_get_mem_macro(state);
	test_parser_integer_literal_boundaries(state);
	test_parser_source_slices(state);
}
