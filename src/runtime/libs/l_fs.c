//
// High-level filesystem operations.
//

ELF_FUNCTION(lib_fs_exists)
{
	elf_State *state = S;
	const char *path = lib_load_cstr(state, 1);
	elf_PlatformFile file = elf_platform_open_file(path, ELF_PLATFORM_OPEN_READ, ELF_PLATFORM_OPEN_EXISTING);
	b32 exists = !ELF_IS_HANDLE_INVALID(file);
	if (exists) elf_platform_close_file(file);
	push_value(state, value_from_integer(exists));
	return 1;
}

ELF_FUNCTION(lib_fs_read)
{
	elf_State *state = S;
	const char *path = lib_load_cstr(state, 1);
	elf_PlatformFile file = elf_platform_open_file(path, ELF_PLATFORM_OPEN_READ, ELF_PLATFORM_OPEN_EXISTING);
	if (ELF_IS_HANDLE_INVALID(file)) {
		push_value(state, value_nil());
		return 1;
	}

	i64 file_size = elf_platform_file_size(file);
	if (file_size < 0 || file_size > INT_MAX) {
		elf_platform_close_file(file);
		push_value(state, value_nil());
		return 1;
	}

	elf_Scratch scratch = elf_get_scratch();
	char *data = elf_arena_push(scratch.arena, (u64)file_size + 1);
	i64 size = elf_platform_read_file(file, data, file_size);
	elf_platform_close_file(file);
	if (size < 0) size = 0;
	lib_push_string(state, data, (u32)size);
	elf_end_scratch(scratch);
	return 1;
}

ELF_FUNCTION(lib_fs_write)
{
	elf_State *state = S;
	const char *path = lib_load_cstr(state, 1);
	elf_String *data = lib_load_string(state, 2);
	elf_PlatformFile file = elf_platform_open_file(path, ELF_PLATFORM_OPEN_WRITE, ELF_PLATFORM_CREATE_ALWAYS);
	if (ELF_IS_HANDLE_INVALID(file)) {
		push_value(state, value_from_integer(false));
		return 1;
	}

	i64 written = elf_platform_write_file(file, (void *)atom_data(data), atom_size(data));
	elf_platform_close_file(file);
	push_value(state, value_from_integer(written == atom_size(data)));
	return 1;
}

ELF_FUNCTION(lib_fs_stat)
{
	elf_State *state = S;
	const char *path = lib_load_cstr(state, 1);
	elf_PlatformFile file = elf_platform_open_file(path, ELF_PLATFORM_OPEN_READ, ELF_PLATFORM_OPEN_EXISTING);
	if (ELF_IS_HANDLE_INVALID(file)) {
		push_value(state, value_nil());
		return 1;
	}

	elf_PlatformFileTimes times = {0};
	elf_platform_file_times(file, &times);
	i64 size = elf_platform_file_size(file);
	elf_platform_close_file(file);

	elf_Table *result = elf_push_new_table(state);
	lib_set_integer_field(state, result, "size", size);
	lib_set_integer_field(state, result, "created", times.create.time);
	lib_set_integer_field(state, result, "accessed", times.access.time);
	lib_set_integer_field(state, result, "modified", times.write.time);
	return 1;
}

ELF_FUNCTION(lib_fs_mkdir)
{
	elf_State *state = S;
	push_value(state, value_from_integer(elf_platform_make_dir(lib_load_cstr(state, 1))));
	return 1;
}

ELF_FUNCTION(lib_fs_remove)
{
	elf_State *state = S;
	push_value(state, value_from_integer(elf_platform_delete_file(lib_load_cstr(state, 1))));
	return 1;
}

static const elf_Binding l_fs[] = {
	{"exists", lib_fs_exists},
	{"read",   lib_fs_read},
	{"write",  lib_fs_write},
	{"stat",   lib_fs_stat},
	{"mkdir",  lib_fs_mkdir},
	{"remove", lib_fs_remove},
};

static elf_Table *elf_lib_fs(elf_State *state)
{
	return new_binding_table(state, l_fs, ARRAY_COUNT(l_fs));
}
