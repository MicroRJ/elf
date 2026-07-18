static elf_Value test_key_atom(elf_State *state, const char *text)
{
	elf_Value value = {};
	value = value_from_atom(elf_atom_from_data(state, text));
	return value;
}

static elf_Value test_key_int(elf_Integer integer)
{
	elf_Value value = {};
	value = value_from_integer(integer);
	return value;
}

static elf_Value test_value_int(elf_Integer integer)
{
	return test_key_int(integer);
}

static void expect_nil(elf_Value value, const char *label)
{
	if (value.type != ELF_VALUE_TYPE_NIL) {
		test_fail(label);
	}
}

static void expect_int(elf_Value value, elf_Integer expected, const char *label)
{
	if (value.type != ELF_VALUE_TYPE_INTEGER || value.x_int != expected) {
		fprintf(stderr, "FAIL: %s expected int %lld, got tag %d elf_Value %lld\n",
			label, expected, value.type, value.x_int);
		test_failures += 1;
	}
}

static void expect_index(u32 value, u32 expected, const char *label)
{
	if (value != expected) {
		fprintf(stderr, "FAIL: %s expected u32 %u, got %u\n",
			label, expected, value);
		test_failures += 1;
	}
}

static void test_table_field_set_get(elf_State *state)
{
	elf_Table *table = elf_push_new_table(state);

	u32 first = elf_table_set(state, table, test_key_atom(state, "answer"), test_value_int(42));
	expect_int(elf_table_get_or_nil(state, table, test_key_atom(state, "answer")), 42, "field get by equal atom key");

	u32 second = elf_table_set(state, table, test_key_atom(state, "answer"), test_value_int(43));
	expect_index(second, first, "field overwrite keeps storage slot");
	expect_int(elf_table_get_or_nil(state, table, test_key_atom(state, "answer")), 43, "field overwrite elf_Value");

	expect_nil(elf_table_get_or_nil(state, table, test_key_atom(state, "missing")), "missing field returns nil");
}

static void test_table_integer_keys(elf_State *state)
{
	elf_Table *table = elf_push_new_table(state);

	for (u32 i = 0; i < 64; ++i) {
		elf_table_set(state, table, test_key_int(i), test_value_int(i * 10));
	}

	for (u32 i = 0; i < 64; ++i) {
		expect_int(elf_table_get_or_nil(state, table, test_key_int(i)), i * 10, "integer key lookup");
	}
}

static void test_table_resize_stress(elf_State *state)
{
	elf_Table *table = elf_push_new_table(state);
	u32 initial_entries = table->nentries;
	elf_Value keys[512] = {};

	for (u32 i = 0; i < 512; ++i) {
		char name[64];
		snprintf(name, sizeof(name), "key.%u", i);
		keys[i] = test_key_atom(state, name);
		elf_table_set(state, table, keys[i], test_value_int(i + 1000));
	}

	if (table->nentries <= initial_entries) {
		test_fail("resize increased entry capacity");
	}

	for (u32 i = 0; i < 512; ++i) {
		expect_int(elf_table_get_or_nil(state, table, keys[i]), i + 1000, "resize preserves field");
	}
}

static void test_table_array_operations(elf_State *state)
{
	elf_Table *table = elf_push_new_table(state);

	u32 a = elf_array_add(state, table, test_value_int(7));
	u32 b = elf_array_add(state, table, test_value_int(8));
	u32 c = elf_array_add(state, table, test_value_int(9));

	expect_index(a, 0, "first array add u32");
	expect_index(b, 1, "second array add u32");
	expect_index(c, 2, "third array add u32");
	expect_index(elf_array_len(table), 3, "array length after adds");

	expect_int(elf_array_get(state, table, 0), 7, "array get 0");
	expect_int(elf_array_get(state, table, 1), 8, "array get 1");
	expect_int(elf_array_get(state, table, 2), 9, "array get 2");

	elf_array_set(state, table, 1, test_value_int(80));
	expect_int(elf_array_get(state, table, 1), 80, "array set");
}

static void test_table_field_and_array_share_storage(elf_State *state)
{
	elf_Table *table = elf_push_new_table(state);

	u32 field_slot = elf_table_set(state, table, test_key_atom(state, "field"), test_value_int(1));
	u32 array_slot = elf_array_add(state, table, test_value_int(2));

	expect_index(field_slot, 0, "field storage starts at slot 0");
	expect_index(array_slot, 1, "array append follows field storage");
	expect_index(elf_array_len(table), 2, "array length reflects shared storage");
	expect_int(elf_array_get(state, table, field_slot), 1, "field elf_Value lives in storage elf_Value *");
	expect_int(elf_array_get(state, table, array_slot), 2, "array elf_Value lives in storage elf_Value *");
}

static void test_table_bind_to_index(elf_State *state)
{
	elf_Table *table = elf_push_new_table(state);

	u32 slot = elf_array_add(state, table, test_value_int(123));
	u32 bound = elf_table_bind_to_index(state, table, test_key_atom(state, "bound"), slot);

	expect_index(bound, slot, "bind returns requested slot");
	expect_int(elf_table_get_or_nil(state, table, test_key_atom(state, "bound")), 123, "bound key resolves to slot elf_Value");

	elf_array_set(state, table, slot, test_value_int(321));
	expect_int(elf_table_get_or_nil(state, table, test_key_atom(state, "bound")), 321, "bound key tracks slot mutation");
}

static void expect_table_value(elf_Value value, const char *label)
{
	if (value.type != ELF_VALUE_TYPE_TABLE) {
		test_fail(label);
	}
}

static void expect_function_value(elf_Value value, const char *label)
{
	if (value.type != ELF_VALUE_TYPE_CFUNCTION) {
		test_fail(label);
	}
}

static void test_runtime_elf_global_is_table(elf_State *state)
{
	elf_Value elf_value = elf_table_get_or_nil(state, state->globals, test_key_atom(state, "elf"));
	expect_table_value(elf_value, "global elf is a table");

	elf_Table *elf_table = value_as_table(elf_value);
	expect_function_value(elf_table_get_or_nil(state, elf_table, test_key_atom(state, "print")), "elf.print is a field function");
	expect_nil(elf_table_get_or_nil(state, state->globals, test_key_atom(state, "elf.print")), "flattened elf.print global is absent");

	elf_Value math_value = elf_table_get_or_nil(state, elf_table, test_key_atom(state, "math"));
	expect_table_value(math_value, "elf.math is a table");
	expect_function_value(elf_table_get_or_nil(state, value_as_table(math_value), test_key_atom(state, "sqrt")), "elf.math.sqrt is a field function");

	elf_Value path_value = elf_table_get_or_nil(state, elf_table, test_key_atom(state, "path"));
	expect_table_value(path_value, "elf.path is a table");
	expect_function_value(elf_table_get_or_nil(state, value_as_table(path_value), test_key_atom(state, "join")), "elf.path.join is a field function");

	elf_Value fs_value = elf_table_get_or_nil(state, elf_table, test_key_atom(state, "fs"));
	expect_table_value(fs_value, "elf.fs is a table");
	expect_function_value(elf_table_get_or_nil(state, value_as_table(fs_value), test_key_atom(state, "read")), "elf.fs.read is a field function");

	elf_Value process_value = elf_table_get_or_nil(state, elf_table, test_key_atom(state, "process"));
	expect_table_value(process_value, "elf.process is a table");
	expect_function_value(elf_table_get_or_nil(state, value_as_table(process_value), test_key_atom(state, "run")), "elf.process.run is a field function");

	elf_Value os_value = elf_table_get_or_nil(state, elf_table, test_key_atom(state, "os"));
	expect_table_value(os_value, "elf.os is a table");
	expect_function_value(elf_table_get_or_nil(state, value_as_table(os_value), test_key_atom(state, "cwd")), "elf.os.cwd is a field function");

	elf_Value time_value = elf_table_get_or_nil(state, elf_table, test_key_atom(state, "time"));
	expect_table_value(time_value, "elf.time is a table");
	expect_function_value(elf_table_get_or_nil(state, value_as_table(time_value), test_key_atom(state, "elapsed")), "elf.time.elapsed is a field function");

	elf_Value random_value = elf_table_get_or_nil(state, elf_table, test_key_atom(state, "random"));
	expect_table_value(random_value, "elf.random is a table");
	expect_function_value(elf_table_get_or_nil(state, value_as_table(random_value), test_key_atom(state, "random")), "elf.random.random is a field function");

	expect_nil(elf_table_get_or_nil(state, elf_table, test_key_atom(state, "sockets")), "deleted elf.sockets library is absent");
}

static void run_table_tests(elf_State *state)
{
	test_table_field_set_get(state);
	test_table_integer_keys(state);
	test_table_resize_stress(state);
	test_table_array_operations(state);
	test_table_field_and_array_share_storage(state);
	test_table_bind_to_index(state);
	test_runtime_elf_global_is_table(state);
}
