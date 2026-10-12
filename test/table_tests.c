static elf_Value test_key_string(elf_State *state, const char *text)
{
	elf_Value value = {};
	value = value_from_string(elf_string_from_data(state, text));
	return value;
}

static elf_Value test_key_int(elf_Int integer)
{
	elf_Value value = {};
	value = value_from_integer(integer);
	return value;
}

static elf_Value test_value_int(elf_Int integer)
{
	return test_key_int(integer);
}

static void expect_nil(elf_Value value, const char *label)
{
	if (value.type != ELF_VALUE_TYPE_NIL) {
		test_fail(label);
	}
}

static void expect_int(elf_Value value, elf_Int expected, const char *label)
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
	elf_Table *table = push_new_table(state);

	elf_table_set(state, table, test_key_string(state, "answer"), test_value_int(42));
	expect_int(elf_table_get_or_nil(state, table, test_key_string(state, "answer")), 42, "field get by equal string key");

	elf_table_set(state, table, test_key_string(state, "answer"), test_value_int(43));
	expect_index(table->count, 1, "field overwrite keeps one mapping");
	expect_int(elf_table_get_or_nil(state, table, test_key_string(state, "answer")), 43, "field overwrite elf_Value");

	expect_nil(elf_table_get_or_nil(state, table, test_key_string(state, "missing")), "missing field returns nil");
}

static void test_table_integer_keys(elf_State *state)
{
	elf_Table *table = push_new_table(state);

	for (u32 i = 0; i < 64; ++i) {
		elf_table_set(state, table, test_key_int(i), test_value_int(i * 10));
	}

	for (u32 i = 0; i < 64; ++i) {
		expect_int(elf_table_get_or_nil(state, table, test_key_int(i)), i * 10, "integer key lookup");
	}
}

static void test_table_resize_stress(elf_State *state)
{
	elf_Table *table = push_new_table(state);
	u32 initial_capacity = table->capacity;
	elf_Value keys[512] = {};

	for (u32 i = 0; i < 512; ++i) {
		char name[64];
		snprintf(name, sizeof(name), "key.%u", i);
		keys[i] = test_key_string(state, name);
		elf_table_set(state, table, keys[i], test_value_int(i + 1000));
	}

	if (table->capacity <= initial_capacity) {
		test_fail("resize increased entry capacity");
	}

	for (u32 i = 0; i < 512; ++i) {
		expect_int(elf_table_get_or_nil(state, table, keys[i]), i + 1000, "resize preserves field");
	}
}

static void test_table_delete_and_reuse_tombstones(elf_State *state)
{
	elf_Table *table = push_new_table(state);
	for (u32 i = 0; i < 128; ++i)
	{
		elf_table_set(state, table, test_key_int(i), test_value_int(i));
	}
	for (u32 i = 0; i < 128; i += 2)
	{
		elf_Value removed = value_nil();
		if (!elf_table_delete(state, table, test_key_int(i), &removed))
		{
			test_fail("table deletes existing key");
		}
		expect_int(removed, i, "table delete returns removed value");
	}
	u32 capacity = table->capacity;
	for (u32 i = 128; i < 192; ++i)
	{
		elf_table_set(state, table, test_key_int(i), test_value_int(i));
	}
	expect_index(table->count, 128, "table count tracks live mappings");
	if (table->capacity != capacity)
	{
		test_fail("table reuses tombstones before growing");
	}
	for (u32 i = 1; i < 192; i += 2)
	{
		expect_int(elf_table_get_or_nil(state, table, test_key_int(i)), i, "table preserves entries across tombstone reuse");
	}
	for (u32 i = 128; i < 192; ++i)
	{
		expect_int(elf_table_get_or_nil(state, table, test_key_int(i)), i, "table finds entries inserted over tombstones");
	}
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
	elf_StrSlice elf_name = {(char *)"elf", 3};
	elf_Value elf_value = elf_globals_get_by_name(&state->globals, elf_name);
	expect_table_value(elf_value, "global elf is a table");

	elf_Table *elf_table = value_as_table(elf_value);
	expect_function_value(elf_table_get_or_nil(state, elf_table, test_key_string(state, "print")), "elf.print is a field function");
	expect_function_value(elf_table_get_or_nil(state, elf_table, test_key_string(state, "println")), "elf.println is a field function");
	expect_function_value(elf_table_get_or_nil(state, elf_table, test_key_string(state, "to_string")), "elf.to_string is a field function");
	expect_function_value(elf_table_get_or_nil(state, elf_table, test_key_string(state, "error")), "elf.error is a field function");
	expect_function_value(elf_table_get_or_nil(state, elf_table, test_key_string(state, "is_error")), "elf.is_error is a field function");
	elf_StrSlice flat_print_name = {(char *)"elf.print", 9};
	expect_nil(elf_globals_get_by_name(&state->globals, flat_print_name), "flattened elf.print global is absent");
	expect_nil(elf_table_get_or_nil(state, elf_table, test_key_string(state, "load_expr")), "broken elf.load_expr binding is absent");
	expect_nil(elf_table_get_or_nil(state, elf_table, test_key_string(state, "unparse")), "broken elf.unparse binding is absent");
	expect_nil(elf_table_get_or_nil(state, elf_table, test_key_string(state, "format")), "obsolete elf.format binding is absent");
	expect_nil(elf_table_get_or_nil(state, elf_table, test_key_string(state, "get_obj_pointer")), "raw object pointer binding is absent");

	expect_nil(elf_table_get_or_nil(state, elf_table, test_key_string(state, "debug")), "redundant elf.debug table is absent");

	elf_Value gc_value = elf_table_get_or_nil(state, elf_table, test_key_string(state, "gc"));
	expect_table_value(gc_value, "elf.gc is a table");
	expect_function_value(elf_table_get_or_nil(state, value_as_table(gc_value), test_key_string(state, "counters")), "elf.gc.counters is a field function");

	elf_Value vm_value = elf_table_get_or_nil(state, elf_table, test_key_string(state, "vm"));
	expect_table_value(vm_value, "elf.vm is a table");
	expect_function_value(elf_table_get_or_nil(state, value_as_table(vm_value), test_key_string(state, "counters")), "elf.vm.counters is a field function");

	elf_Value json_value = elf_table_get_or_nil(state, elf_table, test_key_string(state, "json"));
	expect_table_value(json_value, "elf.json is a table");
	elf_Table *json_table = value_as_table(json_value);
	expect_function_value(elf_table_get_or_nil(state, json_table, test_key_string(state, "parse")), "elf.json.parse is a field function");
	expect_function_value(elf_table_get_or_nil(state, json_table, test_key_string(state, "stringify")), "elf.json.stringify is a field function");

	elf_Value serialization_value = elf_table_get_or_nil(state, elf_table, test_key_string(state, "serialization"));
	expect_table_value(serialization_value, "elf.serialization is a table");
	expect_function_value(elf_table_get_or_nil(state, value_as_table(serialization_value), test_key_string(state, "load_json_file")), "elf.serialization.load_json_file is a field function");

	elf_Value math_value = elf_table_get_or_nil(state, elf_table, test_key_string(state, "math"));
	expect_table_value(math_value, "elf.math is a table");
	expect_function_value(elf_table_get_or_nil(state, value_as_table(math_value), test_key_string(state, "sqrt")), "elf.math.sqrt is a field function");

	elf_Value path_value = elf_table_get_or_nil(state, elf_table, test_key_string(state, "path"));
	expect_table_value(path_value, "elf.path is a table");
	expect_function_value(elf_table_get_or_nil(state, value_as_table(path_value), test_key_string(state, "join")), "elf.path.join is a field function");

	elf_Value fs_value = elf_table_get_or_nil(state, elf_table, test_key_string(state, "fs"));
	expect_table_value(fs_value, "elf.fs is a table");
	elf_Table *fs_table = value_as_table(fs_value);
	expect_function_value(elf_table_get_or_nil(state, fs_table, test_key_string(state, "exists")), "elf.fs.exists is a field function");
	expect_function_value(elf_table_get_or_nil(state, fs_table, test_key_string(state, "is_file")), "elf.fs.is_file is a field function");
	expect_function_value(elf_table_get_or_nil(state, fs_table, test_key_string(state, "is_directory")), "elf.fs.is_directory is a field function");
	expect_function_value(elf_table_get_or_nil(state, fs_table, test_key_string(state, "file_exists")), "elf.fs.file_exists is a field function");
	expect_function_value(elf_table_get_or_nil(state, fs_table, test_key_string(state, "read_text_file")), "elf.fs.read_text_file is a field function");
	expect_function_value(elf_table_get_or_nil(state, fs_table, test_key_string(state, "write_text_file")), "elf.fs.write_text_file is a field function");
	expect_function_value(elf_table_get_or_nil(state, fs_table, test_key_string(state, "get_file_info")), "elf.fs.get_file_info is a field function");
	expect_function_value(elf_table_get_or_nil(state, fs_table, test_key_string(state, "create_directory")), "elf.fs.create_directory is a field function");
	expect_function_value(elf_table_get_or_nil(state, fs_table, test_key_string(state, "create_directories")), "elf.fs.create_directories is a field function");
	expect_function_value(elf_table_get_or_nil(state, fs_table, test_key_string(state, "copy_file")), "elf.fs.copy_file is a field function");
	expect_function_value(elf_table_get_or_nil(state, fs_table, test_key_string(state, "move_file")), "elf.fs.move_file is a field function");
	expect_function_value(elf_table_get_or_nil(state, fs_table, test_key_string(state, "remove_file")), "elf.fs.remove_file is a field function");
	expect_function_value(elf_table_get_or_nil(state, fs_table, test_key_string(state, "remove_directory")), "elf.fs.remove_directory is a field function");
	expect_function_value(elf_table_get_or_nil(state, fs_table, test_key_string(state, "remove_tree")), "elf.fs.remove_tree is a field function");
	expect_function_value(elf_table_get_or_nil(state, fs_table, test_key_string(state, "get_working_directory")), "elf.fs.get_working_directory is a field function");
	expect_function_value(elf_table_get_or_nil(state, fs_table, test_key_string(state, "set_working_directory")), "elf.fs.set_working_directory is a field function");
	expect_nil(elf_table_get_or_nil(state, fs_table, test_key_string(state, "read")), "old elf.fs.read binding is absent");

	elf_Value env_value = elf_table_get_or_nil(state, elf_table, test_key_string(state, "env"));
	expect_table_value(env_value, "elf.env is a table");
	elf_Table *env_table = value_as_table(env_value);
	expect_function_value(elf_table_get_or_nil(state, env_table, test_key_string(state, "get")), "elf.env.get is a field function");
	expect_function_value(elf_table_get_or_nil(state, env_table, test_key_string(state, "has")), "elf.env.has is a field function");
	expect_function_value(elf_table_get_or_nil(state, env_table, test_key_string(state, "set")), "elf.env.set is a field function");
	expect_function_value(elf_table_get_or_nil(state, env_table, test_key_string(state, "unset")), "elf.env.unset is a field function");

	elf_Value process_value = elf_table_get_or_nil(state, elf_table, test_key_string(state, "process"));
	expect_table_value(process_value, "elf.process is a table");
	elf_Table *process_table = value_as_table(process_value);
	expect_function_value(elf_table_get_or_nil(state, process_table, test_key_string(state, "start")), "elf.process.start is a field function");
	expect_table_value(elf_table_get_or_nil(state, process_table, test_key_string(state, "Process")), "elf.process.Process is a method table");

	elf_Value io_value = elf_table_get_or_nil(state, elf_table, test_key_string(state, "io"));
	expect_table_value(io_value, "elf.io is a table");
	expect_function_value(elf_table_get_or_nil(state, value_as_table(io_value), test_key_string(state, "pipe")), "elf.io.pipe is a field function");

	expect_nil(elf_table_get_or_nil(state, elf_table, test_key_string(state, "os")), "empty elf.os library is absent");

	elf_Value time_value = elf_table_get_or_nil(state, elf_table, test_key_string(state, "time"));
	expect_table_value(time_value, "elf.time is a table");
	expect_function_value(elf_table_get_or_nil(state, value_as_table(time_value), test_key_string(state, "elapsed")), "elf.time.elapsed is a field function");
	expect_function_value(elf_table_get_or_nil(state, value_as_table(time_value), test_key_string(state, "unix_ms")), "elf.time.unix_ms is a field function");

	elf_Value random_value = elf_table_get_or_nil(state, elf_table, test_key_string(state, "random"));
	expect_table_value(random_value, "elf.random is a table");
	expect_function_value(elf_table_get_or_nil(state, value_as_table(random_value), test_key_string(state, "random")), "elf.random.random is a field function");

	expect_function_value(elf_table_get_or_nil(state, state->metatables.table, test_key_string(state, "length")), "table.length is a method");
	expect_function_value(elf_table_get_or_nil(state, state->metatables.table, test_key_string(state, "size")), "table.size is a method");
	expect_function_value(elf_table_get_or_nil(state, state->metatables.table, test_key_string(state, "has")), "table.has is a method");
	expect_function_value(elf_table_get_or_nil(state, state->metatables.table, test_key_string(state, "delete")), "table.delete is a method");
	expect_function_value(elf_table_get_or_nil(state, state->metatables.table, test_key_string(state, "clear")), "table.clear is a method");
	expect_function_value(elf_table_get_or_nil(state, state->metatables.table, test_key_string(state, "keys")), "table.keys is a method");
	expect_nil(elf_table_get_or_nil(state, state->metatables.table, test_key_string(state, "map_equal")), "table.map_equal is absent");
	expect_nil(elf_table_get_or_nil(state, state->metatables.table, test_key_string(state, "haskey")), "table.haskey is absent");
	expect_nil(elf_table_get_or_nil(state, state->metatables.table, test_key_string(state, "values")), "table.values is absent");
	expect_nil(elf_table_get_or_nil(state, state->metatables.table, test_key_string(state, "pairs")), "table.pairs is absent");
	expect_nil(elf_table_get_or_nil(state, state->metatables.table, test_key_string(state, "clone")), "table.clone is absent");
	expect_nil(elf_table_get_or_nil(state, state->metatables.table, test_key_string(state, "merge")), "table.merge is absent");

	expect_nil(elf_table_get_or_nil(state, elf_table, test_key_string(state, "sockets")), "deleted elf.sockets library is absent");
}

static void run_table_tests(elf_State *state)
{
	test_table_field_set_get(state);
	test_table_integer_keys(state);
	test_table_resize_stress(state);
	test_table_delete_and_reuse_tombstones(state);
	test_runtime_elf_global_is_table(state);
}
