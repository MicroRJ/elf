typedef struct
{
	const char *path;
}
ScriptTestContext;

ELF_FUNCTION(script_test_assert)
{
	ScriptTestContext *context = elf_get_user_data(S);
	elf_Value value = load_value(S, 1);
	check_numeric(S, value);
	if (!value_to_integer(value))
	{
		print_current_runtime_source_location(S);
		fprintf(stderr, "FAIL: assertion in %s\n", context->path);
		test_failures += 1;
	}
	return 0;
}

static void run_script_test(const char *path)
{
	elf_State *state = elf_create_state();
	ScriptTestContext context = {path};
	elf_set_user_data(state, &context);

	elf_push_fun(state, script_test_assert);
	if (elf_set_global(state, "test_assert") != ELF_ERROR_NONE)
	{
		fprintf(stderr, "FAIL: could not install test_assert for %s\n", path);
		test_failures += 1;
	}
	else if (!elf_push_code_file(state, path))
	{
		fprintf(stderr, "FAIL: could not compile %s\n", path);
		test_failures += 1;
	}
	else
	{
		elf_push_nil(state);
		elf_call(state, 1, 0);
	}

	elf_destroy_state(state);
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
				run_script_test(path.data);
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
	if (!success)
	{
		test_fail("could not enumerate compiler assertion scripts");
	}
	if (!script_count)
	{
		test_fail("compiler assertion script directory is empty");
	}
}
