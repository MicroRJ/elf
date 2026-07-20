//
// Optional file-backed serialization operations.
//

ELF_FUNCTION(l_serialization_load_json_file)
{
	lib_check_arg_count(S, "serialization.load_json_file", nargs, 1, 1);
	const char *name = lib_load_cstr(S, 1);
	elf_PlatformFile file = elf_platform_open_file(name, ELF_PLATFORM_OPEN_READ, ELF_PLATFORM_OPEN_EXISTING);
	if (ELF_IS_HANDLE_INVALID(file))
	{
		elf_push_nil(S);
		return 1;
	}

	i64 file_size = elf_platform_file_size(file);
	if (file_size < 0 || file_size > INT_MAX)
	{
		elf_platform_close_file(file);
		elf_push_nil(S);
		return 1;
	}

	elf_Scratch scratch = elf_begin_scratch();
	char *data = elf_arena_push(scratch.arena, (u64)file_size + 1);
	i64 size = elf_platform_read_file(file, data, file_size);
	elf_platform_close_file(file);
	if (size != file_size)
	{
		elf_end_scratch(scratch);
		elf_push_nil(S);
		return 1;
	}

	data[size] = 0;
	elf_StrSlice source = {data, (u64)size};
	elf_push_json(S, name, source);
	elf_end_scratch(scratch);
	return 1;
}

static const elf_Binding l_serialization[] = {
	{"load_json_file", l_serialization_load_json_file},
};

static elf_Table *elf_lib_serialization(elf_State *state)
{
	return new_binding_table(state, l_serialization, ARRAY_COUNT(l_serialization));
}
