static ELF_FUNCTION(symbol_test_function)
{
	(void)S;
	(void)nargs;
	(void)nrets;
	return 0;
}

static elf_SymField *symbol_field_at(elf_SymTable *table, u32 index)
{
	elf_SymField *field = table ? table->first_field : 0;
	for (u32 i = 0; field && i < index; ++i)
	{
		field = field->next;
	}
	return field;
}

static void test_symbol_tables_store_owned_values(void)
{
	elf_SymTable *root = 0;
	elf_SymTable *app = 0;
	char integer_name[] = "answer";
	char string_value[] = "hello";
	elf_ErrorCode create_root_error = elf_sym_create(&root);
	elf_ErrorCode create_app_error = elf_sym_create(&app);
	elf_ErrorCode integer_error = elf_sym_int(app, (elf_StrSlice) {integer_name, sizeof(integer_name) - 1}, 42);
	elf_ErrorCode number_error = elf_sym_num(app, ELF_LIT("ratio"), 1.5);
	elf_ErrorCode string_error = elf_sym_str(app, ELF_LIT("message"), (elf_StrSlice) {string_value, sizeof(string_value) - 1});
	elf_ErrorCode function_error = elf_sym_fun(app, ELF_LIT("run"), symbol_test_function);
	elf_ErrorCode table_error = elf_sym_tab(root, ELF_LIT("app"), app);

	integer_name[0] = 'x';
	string_value[0] = 'x';

	elf_SymField *table = symbol_field_at(root, 0);
	elf_SymField *integer = symbol_field_at(app, 0);
	elf_SymField *number = symbol_field_at(app, 1);
	elf_SymField *string = symbol_field_at(app, 2);
	elf_SymField *function = symbol_field_at(app, 3);
	b32 calls_succeeded = create_root_error == ELF_ERROR_NONE && create_app_error == ELF_ERROR_NONE && integer_error == ELF_ERROR_NONE && number_error == ELF_ERROR_NONE && string_error == ELF_ERROR_NONE && function_error == ELF_ERROR_NONE && table_error == ELF_ERROR_NONE;
	b32 table_is_valid = table && table->type == ELF_SYMBOL_TABLE && table->table == app && app->parent == root;
	b32 integer_is_valid = integer && integer->type == ELF_SYMBOL_INTEGER && integer->integer == 42 && integer->name.size == sizeof(integer_name) - 1 && !memcmp(integer->name.data, "answer", sizeof(integer_name) - 1);
	b32 number_is_valid = number && number->type == ELF_SYMBOL_NUMBER && number->number == 1.5;
	b32 string_is_valid = string && string->type == ELF_SYMBOL_STRING && string->string.size == sizeof(string_value) - 1 && !memcmp(string->string.data, "hello", sizeof(string_value) - 1);
	b32 function_is_valid = function && function->type == ELF_SYMBOL_FUNCTION && function->function == symbol_test_function;
	if (!calls_succeeded || root->field_count != 1 || app->field_count != 4 || !table_is_valid || !integer_is_valid || !number_is_valid || !string_is_valid || !function_is_valid)
	{
		test_fail("symbol tables store owned values");
	}
	elf_sym_destroy(root);
}

static void test_symbol_tables_reject_invalid_and_duplicate_names(void)
{
	elf_SymTable *table = 0;
	elf_sym_create(&table);
	elf_ErrorCode first_error = elf_sym_int(table, ELF_LIT("answer"), 42);
	elf_ErrorCode duplicate_error = elf_sym_int(table, ELF_LIT("answer"), 0);
	elf_ErrorCode empty_name_error = elf_sym_int(table, ELF_LIT(""), 0);
	elf_ErrorCode invalid_string_error = elf_sym_str(table, ELF_LIT("invalid"), (elf_StrSlice) {0, 1});
	elf_ErrorCode invalid_function_error = elf_sym_fun(table, ELF_LIT("invalid"), 0);
	elf_ErrorCode cyclic_table_error = elf_sym_tab(table, ELF_LIT("self"), table);
	if (first_error != ELF_ERROR_NONE || duplicate_error != ELF_ERROR_INVALID_ARGUMENT || empty_name_error != ELF_ERROR_INVALID_ARGUMENT || invalid_string_error != ELF_ERROR_INVALID_ARGUMENT || invalid_function_error != ELF_ERROR_INVALID_ARGUMENT || cyclic_table_error != ELF_ERROR_INVALID_ARGUMENT || table->field_count != 1)
	{
		test_fail("symbol tables reject invalid and duplicate names");
	}
	elf_sym_destroy(table);
}

static void run_publication_tests(elf_State *state)
{
	(void)state;
	TEST_RUN("symbols.store_owned_values", test_symbol_tables_store_owned_values);
	TEST_RUN("symbols.reject_invalid_and_duplicate_names", test_symbol_tables_reject_invalid_and_duplicate_names);
}
