//
// State associated with the current host process.
//

ELF_FUNCTION(lib_os_cwd)
{
	elf_State *state = S;
	elf_Scratch scratch = elf_get_scratch();
	char *buffer = elf_arena_push_zero(scratch.arena, 32768);
	int size = sys_get_work_dir(buffer, 32768);
	if (size <= 0) push_value(state, value_nil());
	else push_value(state, lib_string_value(state, buffer));
	elf_end_scratch(scratch);
	return 1;
}

ELF_FUNCTION(lib_os_chdir)
{
	elf_State *state = S;
	push_value(state, value_from_integer(sys_set_work_dir(lib_load_cstr(state, 1))));
	return 1;
}

static const elf_Binding l_os[] = {
	{"cwd",   lib_os_cwd},
	{"chdir", lib_os_chdir},
};

static elf_Table *elf_lib_os(elf_State *state)
{
	return new_binding_table(state, l_os, ARRAY_COUNT(l_os));
}
