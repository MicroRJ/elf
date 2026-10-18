static Com_Source *parser_test_add_source(Compiler *compiler, elf_StrSlice name, elf_StrSlice source_text)
{
	ASSERT(compiler);
	ASSERT(name.size <= UINT32_MAX);
	ASSERT(source_text.size <= UINT32_MAX);
	Atom *name_atom = atom_from_data_size(compiler->atoms, name.data, (u32)name.size);
	Com_Source *source = arena_push_zero(compiler->arena, sizeof(*source), _Alignof(Com_Source));
	ASSERT(name_atom);
	ASSERT(source);
	source->id = compiler->source_count++;
	source->name = name_atom;
	source->path = name_atom;
	source->source = (BcSourceBuffer) {source_text.data, (u32)source_text.size};
	if (compiler->last_source)
	{
		compiler->last_source->next = source;
	}
	else
	{
		compiler->first_source = source;
	}
	compiler->last_source = source;
	return source;
}

static Ast parser_test_parse_file(elf_State *state, const char *source_text)
{
	elf_StrSlice source = {(char *)source_text, (u64)strlen(source_text)};
	Compiler *compiler = compiler_create(&state->arena);
	Com_Source *compiler_source = parser_test_add_source(compiler, (elf_StrSlice) {(char *)"parser_tests", sizeof("parser_tests") - 1}, source);
	Parser parser = {};
	ASSERT(parser_init(&parser, compiler));
	Ast file = parser_parse_file(&parser, compiler_source);
	parser_destroy(&parser);
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

static Ast parser_test_stat(Ast file, u32 index, Ast_Kind kind, const char *label)
{
	Ast body = parser_test_body(file);
	if (!body) {
		return 0;
	}
	if (index >= body->block.count) {
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

static Ast parser_test_tuple_item(Ast tuple, u32 index, Ast_Kind kind, const char *label)
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

static void expect_ast_kind(Ast ast, Ast_Kind kind, const char *label)
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

static void test_parser_decl_precedence_and_strings(elf_State *state)
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

static void test_parser_measure_operator(elf_State *state)
{
	Ast file = parser_test_parse_file(state, "ret *items * 2");
	Ast ret = parser_test_stat(file, 0, AST_RETURN, "parse measure return");
	Ast multiply = parser_test_tuple_item(ret->return_stat.expr, 0, AST_MUL, "measure binds before multiplication");
	Ast measure = multiply ? multiply->binary.x : 0;
	expect_ast_kind(measure, AST_MEASURE, "parse measure expression");
	expect_ast_atom(measure ? measure->unary : 0, "items", "measure keeps operand expression");
	expect_ast_i64(multiply ? multiply->binary.y : 0, 2, "measure multiplication rhs");
}

static void test_parser_constant_declaration(elf_State *state)
{
	Ast file = parser_test_parse_file(state, "answer ::= 42");
	Ast decl = parser_test_stat(file, 0, AST_DECL_STAT, "parse constant declaration");
	if (decl && !decl->decl.constant) {
		test_fail("hard bind marks declaration constant");
	}
}

static void test_parser_static_declaration(elf_State *state)
{
	Ast file = parser_test_parse_file(state, "answer :: 40 + 2");
	Ast decl = parser_test_stat(file, 0, AST_STATIC_DECL, "parse static declaration");
	if (!decl) {
		return;
	}

	expect_ast_kind(decl->static_decl.name, AST_IDENT, "static declaration stores identifier");
	expect_ast_atom(decl->static_decl.name, "answer", "static declaration identifier name");
	expect_ast_kind(decl->static_decl.expr, AST_ADD, "static declaration stores expression");
	expect_ast_i64(decl->static_decl.expr->binary.x, 40, "static declaration expression lhs");
	expect_ast_i64(decl->static_decl.expr->binary.y, 2, "static declaration expression rhs");
}

static void test_parser_exported_static_declaration(elf_State *state)
{
	Ast file = parser_test_parse_file(state, "export answer :: 42");
	Ast decl = parser_test_stat(file, 0, AST_STATIC_DECL, "parse exported static declaration");
	if (!decl) {
		return;
	}
	if (!decl->static_decl.exported) {
		test_fail("export marks static declaration");
	}
	expect_ast_atom(decl->static_decl.name, "answer", "exported declaration identifier name");
	expect_ast_i64(decl->static_decl.expr, 42, "exported declaration expression");
}

static void test_parser_import_expression(elf_State *state)
{
	Ast file = parser_test_parse_file(state, "math :: import \"math.elf\"\nloader := fun() { ret import \"nested.elf\" }");
	Ast decl = parser_test_stat(file, 0, AST_STATIC_DECL, "parse imported module declaration");
	if (!decl) {
		return;
	}

	Ast import = decl->static_decl.expr;
	expect_ast_kind(import, AST_IMPORT, "static declaration stores import expression");
	if (!import || !import->import.path || strcmp(atom_data(import->import.path), "math.elf") != 0) {
		test_fail("import expression stores path atom");
	}
	if (file->file.imports.count != 2 || file->file.imports.items[0] != import) {
		test_fail("file records imports in source order");
	}
	else
	{
		Ast nested = file->file.imports.items[1];
		if (nested->kind != AST_IMPORT || strcmp(atom_data(nested->import.path), "nested.elf") != 0) {
			test_fail("file records imports nested inside functions");
		}
	}
}

#if 0
static void test_parser_module_discovery(elf_State *state)
{
	(void)state;
	Arena arena = arena_create(0);
	Compiler *compiler = compiler_create(&arena);
	if (compiler_add_root_file(compiler, "test/imports/discovery/root.elf") == COM_INVALID_SOURCE_ID) {
		test_fail("compiler loads root source file directly");
		arena_destroy(&arena);
		return;
	}
	if (!compiler_parse_sources(compiler)) {
		test_fail("compiler parses imported module graph");
		arena_destroy(&arena);
		return;
	}

	if (compiler->source_count != 3) {
		test_fail("module discovery deduplicates canonical paths");
		arena_destroy(&arena);
		return;
	}
	for (Com_SourceId source_id = 0; source_id < compiler->source_count; ++source_id)
	{
		if (!compiler->sources[source_id].ast || compiler->sources[source_id].ast->kind != AST_FILE) {
			test_fail("module discovery stores every parsed source AST");
		}
	}

	Com_Source *root = compiler->sources + 0;
	Com_Source *a = compiler->sources + 1;
	Com_Source *b = compiler->sources + 2;
	if (root->import_count != 2 || root->imports[0].resolved_source != 1 || root->imports[1].resolved_source != 1) {
		test_fail("root imports resolve to deduplicated source id");
	}
	if (a->import_count != 1 || a->imports[0].resolved_source != 2) {
		test_fail("imported module resolves transitive import");
	}
	if (b->import_count != 1 || b->imports[0].resolved_source != 0) {
		test_fail("module discovery resolves import cycle to existing root");
	}
	if (a->imports[0].site.source != 1 || b->imports[0].site.source != 2) {
		test_fail("import records retain source ids");
	}
	arena_destroy(&arena);
}

static void test_parser_missing_import(elf_State *state)
{
	(void)state;
	Arena arena = arena_create(0);
	Compiler *compiler = compiler_create(&arena);
	const char *source_text = "missing :: import \"missing.elf\"";
	elf_StrSlice source = {(char *)source_text, strlen(source_text)};
	compiler_add_root_source(compiler, "test/imports/discovery/missing-root.elf", source);
	if (compiler_parse_sources(compiler)) {
		test_fail("missing imported source fails module discovery");
	}
	if (compiler->report.error_count != 1) {
		test_fail("missing imported source reports one compiler error");
	}
	arena_destroy(&arena);
}
#endif

static void test_parser_string_strings(elf_State *state)
{
	Ast file = parser_test_parse_file(state, "name := \"hello\"");
	Ast decl = parser_test_stat(file, 0, AST_DECL_STAT, "parse string declaration");
	Ast string = parser_test_tuple_item(decl->decl.expr, 0, AST_STRING_LITERAL, "parse string literal");
	expect_ast_atom(string, "hello", "string ast stores string");
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

	Ast_Array parts = string->interpolated_string;
	if (parts.count != 5) {
		test_fail("interpolated string keeps alternating text and expression parts");
		return;
	}
	expect_ast_kind(parts.items[0], AST_STRING_LITERAL, "interpolated string starts with text");
	expect_ast_atom(parts.items[0], "hello ", "interpolated string leading text");
	expect_ast_kind(parts.items[1], AST_IDENT, "interpolated string keeps first expression");
	expect_ast_atom(parts.items[1], "name", "interpolated string first expression identifier");
	expect_ast_kind(parts.items[2], AST_STRING_LITERAL, "interpolated string keeps middle text");
	expect_ast_atom(parts.items[2], ", count ", "interpolated string middle text payload");
	expect_ast_kind(parts.items[3], AST_ADD, "interpolated string parses full binary expression");
	expect_ast_kind(parts.items[3]->binary.x, AST_IDENT, "interpolated binary expression lhs");
	expect_ast_i64(parts.items[3]->binary.y, 1, "interpolated binary expression rhs");
	expect_ast_kind(parts.items[4], AST_STRING_LITERAL, "interpolated string ends with text");
	expect_ast_atom(parts.items[4], "", "interpolated string keeps empty trailing text");

	file = parser_test_parse_file(state, "ret f\"outer ${f\"inner ${value}\"}\"");
	Ast ret = parser_test_stat(file, 0, AST_RETURN, "parse nested interpolated string return");
	Ast outer = parser_test_tuple_item(ret->return_stat.expr, 0, AST_INTERPOLATED_STRING,
		"parse outer interpolated string");
	if (!outer || outer->interpolated_string.count != 3) {
		test_fail("outer interpolated string keeps nested expression");
		return;
	}
	Ast inner = outer->interpolated_string.items[1];
	expect_ast_kind(inner, AST_INTERPOLATED_STRING, "interpolated expression can contain interpolated string");
	if (inner && inner->interpolated_string.count == 3) {
		expect_ast_atom(inner->interpolated_string.items[0], "inner ", "nested interpolated leading text");
		expect_ast_atom(inner->interpolated_string.items[1], "value", "nested interpolated expression");
		expect_ast_atom(inner->interpolated_string.items[2], "", "nested interpolated trailing text");
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
	expect_ast_atom(stat->if_stat.pred, "flag", "parse if predicate string");
	expect_ast_kind(stat->if_stat.true_clause, AST_BLOCK_STAT, "parse if true block");
	expect_ast_kind(stat->if_stat.else_clause, AST_BLOCK_STAT, "parse if else block");

	Ast ret = stat->if_stat.true_clause->block.items[0];
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
	if (decl->decl.name->tuple.count != 2) {
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
	(void)state;
	Arena arena = arena_create(0);
	char source_text[] = "make {x = 1, [2] = 3}";
	Compiler *compiler = compiler_create(&arena);
	Com_Source *source = parser_test_add_source(compiler, (elf_StrSlice) {(char *)"deprecated_table_call", sizeof("deprecated_table_call") - 1}, (elf_StrSlice) {source_text, sizeof(source_text) - 1});
	Parser parser = {};
	ASSERT(parser_init(&parser, compiler));
	Ast file = parser_parse_file(&parser, source);
	parser_destroy(&parser);
	Ast stat = parser_test_stat(file, 0, AST_TUPLE, "parse call expression statement tuple");
	Ast call = parser_test_tuple_item(stat, 0, AST_CALL, "parse call statement");

	expect_ast_kind(call->call.expr, AST_IDENT, "parse call callee");
	expect_ast_atom(call->call.expr, "make", "parse call callee string");
	if (call->call.args.count != 1) {
		test_fail("table call shorthand counts argument");
		arena_destroy(&arena);
		return;
	}

	Ast table = call->call.args.items[0];
	expect_ast_kind(table, AST_TABLE, "parse table argument");
	if (table->table.count != 2) {
		test_fail("table parser counts entries");
	}
	if (compiler->report.error_count != 0 || compiler->report.warning_count != 1 || !compiler->report.first || compiler->report.first->code != ERROR_SYNTAX_DEPRECATION || compiler->report.first->phase != ERROR_PHASE_PARSER) {
		test_fail("table call shorthand reports one parser deprecation warning");
	}
	arena_destroy(&arena);
}

static void test_parser_named_call_arguments(elf_State *state)
{
	Ast file = parser_test_parse_file(state, "build(1, width = 20 + 2, title = make(\"ok\"))");
	Ast stat = parser_test_stat(file, 0, AST_TUPLE, "parse named call argument statement tuple");
	Ast call = parser_test_tuple_item(stat, 0, AST_CALL, "parse call with named arguments");
	if (!call || call->call.args.count != 3) {
		test_fail("named and positional call arguments share the call argument list");
		return;
	}

	expect_ast_i64(call->call.args.items[0], 1, "positional argument remains an expression");
	Ast width = call->call.args.items[1];
	expect_ast_kind(width, AST_NAMED_ARGUMENT, "parse first named argument");
	if (width && width->kind == AST_NAMED_ARGUMENT)
	{
		expect_ast_kind(width->named_argument.name, AST_IDENT, "named argument stores identifier name");
		expect_ast_atom(width->named_argument.name, "width", "named argument stores name text");
		expect_ast_kind(width->named_argument.expr, AST_ADD, "named argument stores full value expression");
	}

	Ast title = call->call.args.items[2];
	expect_ast_kind(title, AST_NAMED_ARGUMENT, "parse second named argument");
	if (title && title->kind == AST_NAMED_ARGUMENT)
	{
		expect_ast_atom(title->named_argument.name, "title", "second named argument stores name text");
		expect_ast_kind(title->named_argument.expr, AST_CALL, "named argument value may be a call");
	}
}

static void test_parser_array_literals(elf_State *state)
{
	Ast file = parser_test_parse_file(state, "ret []\nret [1, 2 + 3, [4]]\nret [5][0]");

	Ast empty_ret = parser_test_stat(file, 0, AST_RETURN, "parse empty array return");
	Ast empty = parser_test_tuple_item(empty_ret->return_stat.expr, 0, AST_ARRAY, "empty brackets parse as an array literal");
	if (empty && empty->array.count != 0)
	{
		test_fail("empty array literal has no items");
	}

	Ast values_ret = parser_test_stat(file, 1, AST_RETURN, "parse populated array return");
	Ast values = parser_test_tuple_item(values_ret->return_stat.expr, 0, AST_ARRAY, "populated brackets parse as an array literal");
	if (!values || values->array.count != 3)
	{
		test_fail("array literal keeps all items");
		return;
	}
	expect_ast_i64(values->array.items[0], 1, "array literal keeps first item");
	expect_ast_kind(values->array.items[1], AST_ADD, "array literal keeps expression item");
	expect_ast_kind(values->array.items[2], AST_ARRAY, "array literal keeps nested array");

	Ast index_ret = parser_test_stat(file, 2, AST_RETURN, "parse array literal index return");
	Ast index = parser_test_tuple_item(index_ret->return_stat.expr, 0, AST_INDEX, "array literal accepts index postfix");
	expect_ast_kind(index ? index->binary.x : 0, AST_ARRAY, "array index keeps literal receiver");
	expect_ast_i64(index ? index->binary.y : 0, 0, "array index keeps index expression");
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
	Ast file = parser_test_parse_file(state, "ret t[0]\nret t[key]\nret t.name");

	Ast integer_ret = parser_test_stat(file, 0, AST_RETURN, "parse integer index return");
	Ast integer_index = parser_test_tuple_item(integer_ret->return_stat.expr, 0, AST_INDEX, "plain brackets parse as an index");
	expect_ast_kind(integer_index ? integer_index->binary.x : 0, AST_IDENT, "integer index keeps receiver");
	expect_ast_i64(integer_index ? integer_index->binary.y : 0, 0, "integer index keeps key");

	Ast computed_ret = parser_test_stat(file, 1, AST_RETURN, "parse computed index return");
	Ast computed_index = parser_test_tuple_item(computed_ret->return_stat.expr, 0, AST_INDEX, "plain brackets accept a computed key");
	expect_ast_kind(computed_index ? computed_index->binary.x : 0, AST_IDENT, "computed index keeps receiver");
	expect_ast_kind(computed_index ? computed_index->binary.y : 0, AST_IDENT, "computed index keeps key expression");
	expect_ast_atom(computed_index ? computed_index->binary.y : 0, "key", "computed index key identifier");

	Ast dot_ret = parser_test_stat(file, 2, AST_RETURN, "parse dot field return");
	Ast dot_field = parser_test_tuple_item(dot_ret->return_stat.expr, 0, AST_FIELD, "dot identifier parses as field");
	expect_ast_atom(dot_field ? dot_field->binary.y : 0, "name", "dot field key identifier");
}

static void parser_expect_source_error(elf_State *state, const char *source_text, const char *label)
{
	Arena arena = arena_create(0);
	elf_StrSlice source = {(char *)source_text, (u64)strlen(source_text)};
	Compiler *compiler = compiler_create(&arena);
	Com_Source *compiler_source = parser_test_add_source(compiler, (elf_StrSlice) {(char *)"parser_source_error", sizeof("parser_source_error") - 1}, source);
	Parser parser = {};
	ASSERT(parser_init(&parser, compiler));
	Ast file = parser_parse_file(&parser, compiler_source);
	parser_destroy(&parser);
	if (file != ERROR_AST || compiler->report.error_count != 1 || compiler->report.first->phase != ERROR_PHASE_PARSER)
	{
		test_fail(label);
	}
	arena_destroy(&arena);
}

static void test_parser_rejects_legacy_table_syntax(elf_State *state)
{
	parser_expect_source_error(state, "ret {1}", "parser rejects unkeyed table entries");
	parser_expect_source_error(state, "ret table.[key]", "parser rejects dot bracket field access");
}

static void test_parser_rejects_trailing_closures(elf_State *state)
{
	parser_expect_source_error(state, "ret run -> { ret 1 }", "parser rejects removed trailing closure syntax");
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
	Ast file = parser_test_parse_file(state, "add := fun(a: int, b: str) { ret a + b }");
	Ast decl = parser_test_stat(file, 0, AST_DECL_STAT, "parse function declaration");
	Ast function = parser_test_tuple_item(decl->decl.expr, 0, AST_FUNCTION, "parse function expression");

	if (!function) {
		return;
	}
	if (function->function.params.count != 2) {
		test_fail("function expression keeps parameters");
		return;
	}

	Ast first_param = function->function.params.items[0];
	Ast second_param = function->function.params.items[1];
	expect_ast_kind(first_param, AST_FUNCTION_PARAM, "parse first function param");
	expect_ast_kind(second_param, AST_FUNCTION_PARAM, "parse second function param");
	expect_ast_atom(first_param ? first_param->param.name : 0, "a", "first function param name");
	expect_ast_atom(second_param ? second_param->param.name : 0, "b", "second function param name");
	expect_ast_atom(first_param ? first_param->param.type : 0, "int", "first function param type");
	expect_ast_atom(second_param ? second_param->param.type : 0, "str", "second function param type");
	expect_ast_kind(function->function.body, AST_BLOCK_STAT, "function body parses as block");

	file = parser_test_parse_file(state, "configure := fun(required, count = 1 + 2, label: str = \"elf\") { ret required }");
	decl = parser_test_stat(file, 0, AST_DECL_STAT, "parse function declaration with defaults");
	function = parser_test_tuple_item(decl->decl.expr, 0, AST_FUNCTION, "parse function expression with defaults");
	if (!function || function->function.params.count != 3)
	{
		test_fail("function expression keeps defaulted parameters");
		return;
	}

	Ast required_param = function->function.params.items[0];
	Ast count_param = function->function.params.items[1];
	Ast label_param = function->function.params.items[2];
	if (required_param->param.expr) {
		test_fail("required function parameter has no default expression");
	}
	expect_ast_kind(count_param->param.expr, AST_ADD, "function parameter keeps expression default");
	expect_ast_i64(count_param->param.expr ? count_param->param.expr->binary.x : 0, 1, "default expression keeps left operand");
	expect_ast_i64(count_param->param.expr ? count_param->param.expr->binary.y : 0, 2, "default expression keeps right operand");
	expect_ast_atom(label_param->param.type, "str", "defaulted function parameter keeps type");
	expect_ast_kind(label_param->param.expr, AST_STRING_LITERAL, "typed function parameter keeps default");
	expect_ast_atom(label_param->param.expr, "elf", "typed function parameter keeps default text");

	file = parser_test_parse_file(state, "collect := fun(a, ...) { ret elf.varg(0) }");
	decl = parser_test_stat(file, 0, AST_DECL_STAT, "parse variadic function declaration");
	function = parser_test_tuple_item(decl->decl.expr, 0, AST_FUNCTION, "parse variadic function expression");
	if (!function) {
		return;
	}

	if (function->function.params.count != 1) {
		test_fail("variadic marker is not counted as a named parameter");
		return;
	}

	expect_ast_atom(function->function.params.items[0] ? function->function.params.items[0]->param.name : 0, "a", "variadic function named parameter");
	if (!function->function.variadic) {
		test_fail("function tracks variadic marker separately");
	}

	file = parser_test_parse_file(state, "callback := fun { ret 42 }");
	decl = parser_test_stat(file, 0, AST_DECL_STAT, "parse function declaration without parameter list");
	function = parser_test_tuple_item(decl->decl.expr, 0, AST_FUNCTION, "parse function expression without parameter list");
	if (function && function->function.params.count != 0) {
		test_fail("function without parameter list has no parameters");
	}

	parser_expect_source_error(state, "ret fun(value: str | int) { ret value }", "function parameter type requires one identifier");
}

static void test_parser_get_mem_macro(elf_State *state)
{
	Ast file = parser_test_parse_file(state, "ret #get_mem(x)");
	Ast ret = parser_test_stat(file, 0, AST_RETURN, "parse get_mem return");
	Ast expr = parser_test_tuple_item(ret->return_stat.expr, 0, AST_GET_MEM, "parse get_mem macro expression");

	expect_ast_kind(expr ? expr->unary : 0, AST_IDENT, "get_mem keeps target expression");
	expect_ast_atom(expr ? expr->unary : 0, "x", "get_mem target string");
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

static void test_parser_line_boundaries(elf_State *state)
{
	Ast file = parser_test_parse_file(state, "ret /* same */ 1\nret /* split\n*/ 2");

	Ast first_return = parser_test_stat(file, 0, AST_RETURN, "same-line comment keeps return value");
	Ast first_value = parser_test_tuple_item(first_return->return_stat.expr, 0, AST_INTEGER_LITERAL,
		"same-line comment return expression");
	expect_ast_i64(first_value, 1, "same-line comment return value");

	Ast second_return = parser_test_stat(file, 1, AST_RETURN, "multiline comment ends bare return");
	if (second_return && second_return->return_stat.expr) {
		test_fail("multiline comment return has no expression");
	}

	Ast after_return = parser_test_stat(file, 2, AST_TUPLE, "expression after multiline comment return");
	Ast after_return_value = parser_test_tuple_item(after_return, 0, AST_INTEGER_LITERAL,
		"expression after multiline comment return value");
	expect_ast_i64(after_return_value, 2, "expression after multiline comment integer");

	file = parser_test_parse_file(state, "ret 1 +\n2\nret 3\n+4");

	first_return = parser_test_stat(file, 0, AST_RETURN, "operator before newline continues return");
	Ast add = parser_test_tuple_item(first_return->return_stat.expr, 0, AST_ADD,
		"operator before newline additive expression");
	expect_ast_i64(add ? add->binary.x : 0, 1, "continued addition left value");
	expect_ast_i64(add ? add->binary.y : 0, 2, "continued addition right value");

	second_return = parser_test_stat(file, 1, AST_RETURN, "operator after newline does not continue return");
	Ast second_value = parser_test_tuple_item(second_return->return_stat.expr, 0, AST_INTEGER_LITERAL,
		"operator after newline return expression");
	expect_ast_i64(second_value, 3, "operator after newline return value");

	Ast unary_statement = parser_test_stat(file, 2, AST_TUPLE, "operator after newline begins statement");
	Ast unary_value = parser_test_tuple_item(unary_statement, 0, AST_INTEGER_LITERAL,
		"operator after newline unary expression");
	expect_ast_i64(unary_value, 4, "operator after newline unary value");

	file = parser_test_parse_file(state, "ret \"\"\"one\ntwo\"\"\":trim()\nret target\n(value)");

	first_return = parser_test_stat(file, 0, AST_RETURN, "postfix follows multiline token");
	Ast call = parser_test_tuple_item(first_return->return_stat.expr, 0, AST_CALL,
		"postfix call follows multiline token");
	expect_ast_kind(call ? call->call.expr : 0, AST_META_FIELD, "multiline token keeps metafield postfix");

	second_return = parser_test_stat(file, 1, AST_RETURN, "newline ends postfix chain");
	Ast target = parser_test_tuple_item(second_return->return_stat.expr, 0, AST_IDENT,
		"newline postfix return expression");
	expect_ast_atom(target, "target", "newline postfix return target");

	Ast parenthesized = parser_test_stat(file, 2, AST_TUPLE, "parenthesized expression after postfix newline");
	Ast parenthesized_value = parser_test_tuple_item(parenthesized, 0, AST_IDENT,
		"parenthesized expression value");
	expect_ast_atom(parenthesized_value, "value", "parenthesized expression string");
}

static void test_parser_lexer_error_boundary(elf_State *state)
{
	Arena arena = arena_create(0);
	elf_StrSlice source = {(char *)"@ ignored", 9};
	Compiler *compiler = compiler_create(&arena);
	Com_Source *compiler_source = parser_test_add_source(compiler, (elf_StrSlice) {(char *)"parser_lexer_error", sizeof("parser_lexer_error") - 1}, source);
	Parser parser = {};
	ASSERT(parser_init(&parser, compiler));
	Ast file = parser_parse_file(&parser, compiler_source);
	parser_destroy(&parser);
	if (file != ERROR_AST || compiler->report.error_count != 1 || compiler->report.first->phase != ERROR_PHASE_LEXER) {
		test_fail("parser propagates a lexer error token");
	}
	arena_destroy(&arena);
}

#if 0
static void test_parser_take_token_error(elf_State *state)
{
	Arena arena = arena_create(0);
	elf_StrSlice source = {(char *)"value", 5};
	Compiler *compiler = compiler_create(&arena);
	compiler_add_root_source(compiler, "parser_take_token_error", source);
	Parser *parser = parser_create(compiler);
	parser_begin_source(parser, 0);
	Token token = take_token(parser, TOK_LEFT_PAREN);
	parser_destroy(parser);
	if (!token_is_error(token) || token.status != COM_STATUS_REPORTED || ast_from_token_error(token) != ERROR_AST || compiler->report.error_count != 1) {
		test_fail("take token returns its source error");
	}
	arena_destroy(&arena);
}

static void test_parser_take_token_oom(elf_State *state)
{
	Arena arena = arena_create(0);
	elf_StrSlice source = {(char *)"value", 5};
	Compiler *compiler = compiler_create(&arena);
	compiler_add_root_source(compiler, "parser_take_token_oom", source);
	Parser *parser = parser_create(compiler);
	parser_begin_source(parser, 0);
	u64 capacity = arena.capacity;
	arena.capacity = arena.used;
	Token token = take_token(parser, TOK_LEFT_PAREN);
	arena.capacity = capacity;
	parser_destroy(parser);
	if (!token_is_error(token) || token.status != COM_STATUS_OOM || ast_from_token_error(token) != ERROR_OOM_AST || compiler->report.error_count != 0) {
		test_fail("take token returns diagnostic allocation failure");
	}
	arena_destroy(&arena);
}
#endif

static void test_parser_ast_oom(elf_State *state)
{
	Arena arena = arena_create(0);
	elf_StrSlice source = {(char *)"42", 2};
	Compiler *compiler = compiler_create(&arena);
	Com_Source *compiler_source = parser_test_add_source(compiler, (elf_StrSlice) {(char *)"parser_ast_oom", sizeof("parser_ast_oom") - 1}, source);
	Parser parser = {};
	ASSERT(parser_init(&parser, compiler));
	u64 capacity = arena.capacity;
	arena.capacity = arena.used;
	Ast ast = parser_parse_constant_expr(&parser, compiler_source);
	arena.capacity = capacity;
	parser_destroy(&parser);
	if (ast != ERROR_OOM_AST || ast->status != COM_STATUS_OOM) {
		test_fail("parser returns AST allocation failure");
	}
	arena_destroy(&arena);
}

static void run_parser_tests(elf_State *state)
{
	test_parser_decl_precedence_and_strings(state);
	test_parser_measure_operator(state);
	test_parser_constant_declaration(state);
	test_parser_static_declaration(state);
	test_parser_exported_static_declaration(state);
	test_parser_import_expression(state);
	test_parser_string_strings(state);
	test_parser_interpolated_strings(state);
	test_parser_if_else_blocks(state);
	test_parser_for_loops(state);
	test_parser_call_with_table_argument(state);
	test_parser_named_call_arguments(state);
	test_parser_array_literals(state);
	test_parser_nil_assign(state);
	test_parser_table_access_modes(state);
	test_parser_rejects_legacy_table_syntax(state);
	test_parser_rejects_trailing_closures(state);
	test_parser_open_range_indexes(state);
	test_parser_function_expression(state);
	test_parser_get_mem_macro(state);
	test_parser_integer_literal_boundaries(state);
	test_parser_source_slices(state);
	test_parser_line_boundaries(state);
	test_parser_lexer_error_boundary(state);
	test_parser_ast_oom(state);
}
