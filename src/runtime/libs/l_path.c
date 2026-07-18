//
// Pure path manipulation. These functions do not access the filesystem.
//

static const char *path_last_separator(const char *begin, const char *end)
{
	for (const char *cursor = end; cursor > begin; --cursor)
	{
		char c = cursor[-1];
		if (c == '\\' || c == '/') return cursor - 1;
	}
	return 0;
}

ELF_FUNCTION(lib_path_parent)
{
	elf_State *state = S;
	elf_String *path = lib_load_string(state, 1);
	i32 levels = nargs > 2 ? (i32)lib_load_integer(state, 2) : 1;
	const char *begin = elf_atom_data(path);
	const char *end = begin + elf_atom_size(path);

	while (levels-- > 0 && end > begin)
	{
		const char *separator = path_last_separator(begin, end);
		end = separator ? separator : begin;
	}

	lib_push_string(state, begin, (u32)(end - begin));
	return 1;
}

ELF_FUNCTION(lib_path_filename)
{
	elf_State *state = S;
	elf_String *path = lib_load_string(state, 1);
	const char *begin = elf_atom_data(path);
	const char *end = begin + elf_atom_size(path);
	const char *separator = path_last_separator(begin, end);
	const char *name = separator ? separator + 1 : begin;
	lib_push_string(state, name, (u32)(end - name));
	return 1;
}

ELF_FUNCTION(lib_path_stem)
{
	elf_State *state = S;
	elf_String *path = lib_load_string(state, 1);
	const char *begin = elf_atom_data(path);
	const char *end = begin + elf_atom_size(path);
	const char *separator = path_last_separator(begin, end);
	const char *name = separator ? separator + 1 : begin;
	const char *extension = end;

	for (const char *cursor = end; cursor > name; --cursor)
	{
		if (cursor[-1] == '.') {
			extension = cursor - 1;
			break;
		}
	}

	lib_push_string(state, name, (u32)(extension - name));
	return 1;
}

ELF_FUNCTION(lib_path_extension)
{
	elf_State *state = S;
	elf_String *path = lib_load_string(state, 1);
	const char *begin = elf_atom_data(path);
	const char *end = begin + elf_atom_size(path);
	const char *separator = path_last_separator(begin, end);
	const char *name = separator ? separator + 1 : begin;
	const char *extension = end;

	for (const char *cursor = end; cursor > name; --cursor)
	{
		if (cursor[-1] == '.') {
			extension = cursor;
			break;
		}
	}

	lib_push_string(state, extension, (u32)(end - extension));
	return 1;
}

ELF_FUNCTION(lib_path_join)
{
	elf_State *state = S;
	elf_String *left = lib_load_string(state, 1);
	elf_String *right = lib_load_string(state, 2);
	const char *left_data = elf_atom_data(left);
	const char *right_data = elf_atom_data(right);
	u32 left_size = elf_atom_size(left);
	u32 right_size = elf_atom_size(right);
	b32 needs_separator = left_size > 0 && right_size > 0;

	if (needs_separator) {
		char last = left_data[left_size - 1];
		char first = right_data[0];
		needs_separator = last != '/' && last != '\\' && first != '/' && first != '\\';
	}

	Scratch scratch = get_scratch();
	char *result = arena_push(scratch.arena, left_size + right_size + needs_separator);
	memcpy(result, left_data, left_size);
	u32 at = left_size;
	if (needs_separator) result[at++] = '/';
	memcpy(result + at, right_data, right_size);
	lib_push_string(state, result, at + right_size);
	end_scratch(scratch);
	return 1;
}

static const elf_Binding l_path[] = {
	{"join",      lib_path_join},
	{"parent",    lib_path_parent},
	{"filename",  lib_path_filename},
	{"stem",      lib_path_stem},
	{"extension", lib_path_extension},
};

static elf_Table *elf_lib_path(elf_State *state)
{
	return new_binding_table(state, l_path, ARRAY_COUNT(l_path));
}
