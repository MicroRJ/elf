//
// High-level filesystem operations.
//

ELF_FUNCTION(lib_fs_exists)
{
	elf_State *state = S;
	const char *path = lib_load_cstr(state, 1);
	elf_Handle file = elf_platform_access_file(path, SYS_OPEN_READ, SYS_OPEN_EXISTING);
	b32 exists = !ELF_IS_HANDLE_INVALID(file);
	if (exists) elf_platform_close_file(file);
	push_value(state, value_from_integer(exists));
	return 1;
}

ELF_FUNCTION(lib_fs_read)
{
	elf_State *state = S;
	const char *path = lib_load_cstr(state, 1);
	elf_Handle file = elf_platform_access_file(path, SYS_OPEN_READ, SYS_OPEN_EXISTING);
	if (ELF_IS_HANDLE_INVALID(file)) {
		push_value(state, value_nil());
		return 1;
	}

	i64 file_size = elf_platform_get_file_size(file);
	if (file_size < 0 || file_size > INT_MAX) {
		elf_platform_close_file(file);
		push_value(state, value_nil());
		return 1;
	}

	Scratch scratch = get_scratch();
	char *data = arena_push(scratch.arena, (u64)file_size + 1);
	i64 size = elf_platform_read_file(file, data, file_size);
	elf_platform_close_file(file);
	if (size < 0) size = 0;
	lib_push_string(state, data, (u32)size);
	end_scratch(scratch);
	return 1;
}

ELF_FUNCTION(lib_fs_write)
{
	elf_State *state = S;
	const char *path = lib_load_cstr(state, 1);
	elf_String *data = lib_load_string(state, 2);
	elf_Handle file = elf_platform_access_file(path, SYS_OPEN_WRITE, SYS_CREATE_ALWAYS);
	if (ELF_IS_HANDLE_INVALID(file)) {
		push_value(state, value_from_integer(false));
		return 1;
	}

	i64 written = sys_write_file(file, (void *)elf_atom_data(data), elf_atom_size(data));
	elf_platform_close_file(file);
	push_value(state, value_from_integer(written == elf_atom_size(data)));
	return 1;
}

ELF_FUNCTION(lib_fs_stat)
{
	elf_State *state = S;
	const char *path = lib_load_cstr(state, 1);
	elf_Handle file = elf_platform_access_file(path, SYS_OPEN_READ, SYS_OPEN_EXISTING);
	if (ELF_IS_HANDLE_INVALID(file)) {
		push_value(state, value_nil());
		return 1;
	}

	FILE_TIMES times = {0};
	sys_time_file(file, &times);
	i64 size = elf_platform_get_file_size(file);
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
	push_value(state, value_from_integer(sys_make_dir(lib_load_cstr(state, 1))));
	return 1;
}

ELF_FUNCTION(lib_fs_remove)
{
	elf_State *state = S;
	push_value(state, value_from_integer(sys_delete_file(lib_load_cstr(state, 1))));
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
