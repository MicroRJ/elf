static elf_StrSlice globals_test_name(char *name)
{
	return (elf_StrSlice){name, (u64)strlen(name)};
}

static void test_globals_own_names_and_preserve_slots(elf_State *state)
{
	u32 starting_count = state->global_count;

	char name[32];
	for (u32 i = 0; i < 256; ++i)
	{
		snprintf(name, sizeof(name), "global_%u", i);
		u32 slot = elf_globals_ensure(state, globals_test_name(name));
		if (slot != starting_count + i)
		{
			test_fail("globals assign stable sequential slots");
		}
		elf_globals_set_by_slot(state, slot, value_from_integer((i64)i * 3));
	}

	for (u32 i = 0; i < 256; ++i)
	{
		snprintf(name, sizeof(name), "global_%u", i);
		u32 slot = UINT32_MAX;
		if (!elf_globals_find(state, globals_test_name(name), &slot) || slot != starting_count + i || value_as_integer(elf_globals_get_by_slot(state, slot)) != (i64)i * 3)
		{
			test_fail("globals preserve names, slots, and values while growing");
		}
	}

	char owned_name[] = "owned_name";
	u32 owned_slot = elf_globals_ensure(state, globals_test_name(owned_name));
	owned_name[0] = 'x';
	elf_StrSlice original_name = {(char *)"owned_name", 10};
	u32 found_slot = UINT32_MAX;
	if (!elf_globals_find(state, original_name, &found_slot) || found_slot != owned_slot)
	{
		test_fail("globals own copies of their names");
	}
	if (elf_globals_ensure(state, original_name) != owned_slot || state->global_count != starting_count + 257)
	{
		test_fail("globals reuse an existing name slot");
	}
}

static void test_globals_are_gc_roots(elf_State *state)
{
	elf_Index checkpoint = elf_get_top(state);
	elf_new_table(state);
	elf_push_int(state, 42);
	if (elf_set_field(state, -2, "answer") != ELF_ERROR_NONE || elf_set_global(state, "globals_gc_probe") != ELF_ERROR_NONE)
	{
		test_fail("globals GC probe stores a table");
		elf_set_top(state, checkpoint);
		return;
	}

	force_gc_allocations(state, 32);
	elf_get_global(state, "globals_gc_probe");
	elf_Int answer = 0;
	if (!elf_get_field(state, -1, "answer") || !elf_to_int(state, -1, &answer) || answer != 42)
	{
		test_fail("globals keep stored objects reachable");
	}
	elf_set_top(state, checkpoint);
	elf_push_nil(state);
	elf_set_global(state, "globals_gc_probe");
}

static void run_globals_tests(elf_State *state)
{
	TEST_RUN("globals.own_names_and_preserve_slots", test_globals_own_names_and_preserve_slots, state);
	TEST_RUN("globals.are_gc_roots", test_globals_are_gc_roots, state);
}
