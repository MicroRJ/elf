typedef struct
{
	const char *path;
}
ScriptTestContext;

ELF_FUNCTION(script_test_assert)
{
	ScriptTestContext *context = elf_get_user_data(S);
	lib_check_arg_count(S, "test_assert", nargs, 1, 2);
	elf_Value value = load_value(S, 1);
	check_numeric(S, value);
	const char *message = nargs > 2 ? lib_load_cstr(S, 2) : 0;
	if (!value_to_integer(value))
	{
		if (message) {
			TEST_FAIL("%s", message);
		}
		else {
			TEST_FAIL("assertion failed");
		}
		print_current_runtime_source_location(S);
	}
	return 0;
}

static void run_script_test(const char *path)
{
	elf_State *state = elf_create_state();
	ScriptTestContext context = {path};
	elf_set_user_data(state, &context);

	elf_SymTable *symbols = 0;
	elf_sym_create(&symbols);
	elf_StrSlice source_path = {(char *)path, strlen(path)};
	if (elf_sym_fun(symbols, ELF_LIT("test_assert"), script_test_assert) != ELF_ERROR_NONE)
	{
		TEST_FAIL("could not install test_assert");
	}
	else if (!elf_push_code_file(state, symbols, source_path))
	{
		TEST_FAIL("could not compile script");
	}
	else
	{
		elf_push_nil(state);
		elf_call(state, 1);
	}

	elf_sym_destroy(symbols);
	elf_destroy_state(state);
}

static void test_script_discovery(b32 success, u32 script_count)
{
	if (!success)
	{
		TEST_FAIL("could not enumerate compiler assertion scripts");
	}
	if (!script_count)
	{
		TEST_FAIL("compiler assertion script directory is empty");
	}
}

static void run_script_tests(void)
{
	char path_storage[DY_PATH_CAPACITY];
	dy_Path_Builder path = {};
	dy_Directory directory = {};
	dy_Directory_Entry entry = {};
	dy_Directory_Status status = DY_DIRECTORY_END;
	b32 success = dy_path_builder_init(&path, path_storage, sizeof(path_storage), DY_LIT("test/scripts"));
	if (success)
	{
		success = !dy_find_first_file(&path, &directory, &entry, &status).error;
	}

	u32 script_count = 0;
	while (success && status == DY_DIRECTORY_ENTRY)
	{
		if (!entry.info.is_directory && dy_string_ends_with_insensitive(entry.name, DY_LIT(".elf")))
		{
			dy_Path_Mark mark = dy_path_mark(&path);
			if (!dy_path_push(&path, entry.name))
			{
				success = false;
			}
			else
			{
				TEST_RUN(path.data, run_script_test, path.data);
				script_count += 1;
			}
			dy_path_pop(&path, mark);
		}
		if (success)
		{
			success = !dy_find_next_file(&directory, &entry, &status).error;
		}
	}

	if (directory.value)
	{
		success &= !dy_close_directory(&directory).error;
	}
	TEST_RUN("scripts.discovery", test_script_discovery, success, script_count);
}
