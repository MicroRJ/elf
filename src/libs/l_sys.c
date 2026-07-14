//
// See Copyright Notice In elf.h
//

static inline f64 performance_counter_elapsed_s(i64 start)
{
	return (sys_get_performance_counter() - start) / (f64)sys_get_performance_counter_frequency();
}

static elf_Atom *load_atom_arg(elf_State *state, u32 index)
{
	elf_Value value = load_value(state, index);
	check_value_type(state, value, ELF_VALUE_TYPE_ATOM);
	return value_as_atom(value);
}

static const char *load_atom_text_arg(elf_State *state, u32 index)
{
	return elf_atom_data(load_atom_arg(state, index));
}

static i64 load_integer_arg(elf_State *state, u32 index)
{
	elf_Value value = load_value(state, index);
	check_value_type_rule(state, value, TRULE_NUMERIC);
	return value_to_integer(value);
}

static elf_Handle load_handle_arg(elf_State *state, u32 index)
{
	elf_Value value = load_value(state, index);
	check_value_type(state, value, ELF_VALUE_TYPE_HANDLE);
	return value_as_handle(value);
}

static void push_atom_data(elf_State *state, const char *data, u32 size)
{
	push_value(state, value_from_atom(elf_atom_from_data_size(state, data, size)));
}

static elf_Value atom_value_from_text(elf_State *state, const char *text)
{
	return value_from_atom(elf_atom_from_data(state, text));
}

static void table_set_integer_field(elf_State *state, elf_Table *table, const char *name, i64 value)
{
	elf_table_set(state, table, atom_value_from_text(state, name), value_from_integer(value));
}

static void push_optional_handle(elf_State *state, elf_Handle handle)
{
	if (ELF_IS_HANDLE_INVALID(handle)) {
		push_value(state, value_nil());
	}
	else {
		push_value(state, value_from_handle(handle));
	}
}

static b32 parse_file_open_mode(elf_State *state, const char *mode_text, i32 *flags, i32 *creation_mode)
{
	*flags = 0;

	for (const char *cursor = mode_text; *cursor; ++cursor)
	{
		if (*cursor == 'r') {
			*flags |= SYS_OPEN_READ;
		}
		else if (*cursor == 'w') {
			*flags |= SYS_OPEN_WRITE;
		}
		else if (*cursor == 'b') {
		}
		else {
			report_runtime_error(state, RUNTIME_ERROR_GENERIC, NO_BYTE, "unrecognized file open mode");
			return false;
		}
	}

	*creation_mode = SYS_OPEN_EXISTING;
	if (*flags & SYS_OPEN_WRITE) {
		*creation_mode = SYS_CREATE_ALWAYS;
	}

	return true;
}

static const char *path_last_separator(const char *begin, const char *end)
{
	for (const char *cursor = end; cursor > begin; --cursor)
	{
		char c = cursor[-1];
		if (c == '\\' || c == '/') {
			return cursor - 1;
		}
	}

	return 0;
}

static char *slice_path_part(char *data, i32 *size, i32 count)
{
	char *end = data + *size;

	if (count < 0)
	{
		while (count ++)
		{
			--end;
			while (end > data && end[-1] != '\\' && end[-1] != '/') {
				--end;
			}
		}

		*size -= (i32)(end - data);
		return end;
	}

	char *slice_end = data;
	while (count --)
	{
		do {
			++slice_end;
		} while (slice_end < end && *slice_end != '\\' && *slice_end != '/');
	}

	*size = (i32)(slice_end - data);
	return data;
}

static void append_visited_paths(elf_State *state, FILE_VISITOR *visitor, elf_Table *paths, i32 recurse)
{
	elf_Handle dir = sys_find_first_file(visitor);
	if (!dir) {
		return;
	}

	for (;;)
	{
		const char *name = visitor->pb.name;
		b32 is_relative_marker =
			(name[0] == '.' && name[1] == 0) ||
			(name[0] == '.' && name[1] == '.' && name[2] == 0);

		if (visitor->type != FILE_TYPE_SYMLINK && !is_relative_marker)
		{
			elf_Atom *atom = elf_atom_from_data(state, visitor->pb.path);
			elf_array_add(state, paths, value_from_atom(atom));

			if (visitor->type == FILE_TYPE_FOLDER && recurse > 0) {
				append_visited_paths(state, visitor, paths, recurse - 1);
			}
		}

		if (!sys_find_next_file(dir, visitor)) {
			break;
		}
	}
}

ELF_FUNCTION(lib_sys_get_performance_counter)
{
	elf_State *state = S;
	push_value(state, value_from_integer(sys_get_performance_counter()));
	return 1;
}

ELF_FUNCTION(lib_sys_get_performance_counter_frequency)
{
	elf_State *state = S;
	push_value(state, value_from_integer(sys_get_performance_counter_frequency()));
	return 1;
}

ELF_FUNCTION(lib_sys_get_performance_counter_elapsed_s)
{
	elf_State *state = S;
	i64 start = load_integer_arg(state, 1);
	push_value(state, value_from_number(performance_counter_elapsed_s(start)));
	return 1;
}

ELF_FUNCTION(lib_sys_get_performance_counter_elapsed_ms)
{
	elf_State *state = S;
	i64 start = load_integer_arg(state, 1);
	push_value(state, value_from_number(performance_counter_elapsed_s(start) * 1000));
	return 1;
}

ELF_FUNCTION(lib_sys_sleep)
{
	elf_State *state = S;
	sys_sleep(load_integer_arg(state, 1));
	return 0;
}

ELF_FUNCTION(lib_sys_get_parent_path)
{
	elf_State *state = S;
	elf_Atom *path = load_atom_arg(state, 1);
	i32 levels = 1;

	if (nargs >= 3) {
		levels = (i32)load_integer_arg(state, 2);
	}

	const char *begin = elf_atom_data(path);
	const char *end = begin + elf_atom_size(path);
	for (i32 i = 0; i < levels && end > begin; ++i)
	{
		const char *separator = path_last_separator(begin, end);
		end = separator ? separator : begin;
	}

	push_atom_data(state, begin, (u32)(end - begin));
	return 1;
}

ELF_FUNCTION(lib_sys_slice_path)
{
	elf_State *state = S;
	elf_Atom *path = load_atom_arg(state, 1);
	i32 count = 1;

	if (nargs > 2) {
		count = (i32)load_integer_arg(state, 2);
	}

	i32 size = (i32)elf_atom_size(path);
	char *data = slice_path_part((char *)elf_atom_data(path), &size, count);

	if (nargs > 3)
	{
		count = (i32)load_integer_arg(state, 3);
		data = slice_path_part(data, &size, count);
	}

	push_atom_data(state, data, (u32)size);
	return 1;
}

ELF_FUNCTION(lib_sys_get_file_name)
{
	elf_State *state = S;
	elf_Atom *path = load_atom_arg(state, 1);
	const char *begin = elf_atom_data(path);
	const char *end = begin + elf_atom_size(path);
	const char *extension = end;

	while (end > begin && *end != '\\' && *end != '/' && *end != '.') {
		--end;
	}

	if (*end == '.') {
		extension = end;
	}

	while (end > begin && end[-1] != '\\' && end[-1] != '/') {
		--end;
	}

	push_atom_data(state, end, (u32)(extension - end));
	return 1;
}

ELF_FUNCTION(lib_sys_get_file_extension)
{
	elf_State *state = S;
	elf_Atom *path = load_atom_arg(state, 1);
	const char *begin = elf_atom_data(path);
	const char *end = begin + elf_atom_size(path);

	while (end > begin && end[-1] != '\\' && end[-1] != '/' && end[-1] != '.') {
		--end;
	}

	push_atom_data(state, end, (u32)(elf_atom_size(path) - (end - begin)));
	return 1;
}

ELF_FUNCTION(lib_sys_create_directory)
{
	elf_State *state = S;
	const char *path = load_atom_text_arg(state, 1);
	push_value(state, value_from_integer(sys_make_dir(path)));
	return 1;
}

ELF_FUNCTION(lib_sys_delete_file)
{
	elf_State *state = S;
	const char *path = load_atom_text_arg(state, 1);
	push_value(state, value_from_integer(sys_delete_file(path)));
	return 1;
}

ELF_FUNCTION(lib_sys_get_file_times)
{
	elf_State *state = S;
	elf_Handle file = load_handle_arg(state, 1);

	FILE_TIMES times;
	sys_time_file(file, &times);

	elf_Table *table = elf_table_new(state);
	table_set_integer_field(state, table, "created", times.create.time);
	table_set_integer_field(state, table, "access", times.access.time);
	table_set_integer_field(state, table, "write", times.write.time);

	push_value(state, value_from_table(table));
	return 1;
}

ELF_FUNCTION(lib_sys_file_time_to_system_time)
{
	elf_State *state = S;
	FILE_TIME filetime = { .time = load_integer_arg(state, 1) };

	SYSTEM_TIME systemtime;
	sys_file_time_to_system_time(&filetime, &systemtime);

	elf_Table *table = elf_table_new(state);
	table_set_integer_field(state, table, "year", systemtime.year);
	table_set_integer_field(state, table, "month", systemtime.month);
	table_set_integer_field(state, table, "dayofweek", systemtime.dayofweek);
	table_set_integer_field(state, table, "day", systemtime.day);
	table_set_integer_field(state, table, "hour", systemtime.hour);
	table_set_integer_field(state, table, "minute", systemtime.minute);
	table_set_integer_field(state, table, "second", systemtime.second);
	table_set_integer_field(state, table, "milliseconds", systemtime.milliseconds);

	push_value(state, value_from_table(table));
	return 1;
}

ELF_FUNCTION(lib_sys_load_dll)
{
	elf_State *state = S;
	const char *name = load_atom_text_arg(state, 1);
	push_optional_handle(state, sys_load_dll(name));
	return 1;
}

ELF_FUNCTION(lib_sys_get_dll_fn)
{
	elf_State *state = S;
	elf_Handle dll = load_handle_arg(state, 1);
	const char *name = load_atom_text_arg(state, 2);

	elf_Function function = (elf_Function) sys_get_dll_fn(dll, name);
	if (function) {
		push_value(state, value_from_function(function));
	}
	else {
		push_value(state, value_nil());
	}

	return 1;
}

ELF_FUNCTION(lib_sys_get_file_tree)
{
	elf_State *state = S;
	load_atom_text_arg(state, 1);

	push_value(state, value_nil());
	return 1;
}

ELF_FUNCTION(lib_sys_get_path_list)
{
	elf_State *state = S;
	const char *path = load_atom_text_arg(state, 1);
	u32 recurse = 0;

	if (nargs >= 3) {
		recurse = load_integer_arg(state, 2);
	}

	elf_Table *paths = elf_table_new(state);
	Scratch scratch = get_scratch();
	FILE_VISITOR visitor = {};
	visitor.pb = path_new_stack(scratch.arena, 32768);

	path_push(&visitor.pb, path);
	append_visited_paths(state, &visitor, paths, recurse);
	end_scratch(scratch);

	push_value(state, value_from_table(paths));
	return 1;
}

ELF_FUNCTION(lib_sys_open_temp_file)
{
	elf_State *state = S;
	FILE *file = 0;
#if defined(PLATFORM_WEB)
	file = tmpfile();
#else
	tmpfile_s(&file);
#endif
	push_optional_handle(state, (elf_Handle)file);
	return 1;
}

ELF_FUNCTION(lib_sys_open_file)
{
	elf_State *state = S;
	const char *name = load_atom_text_arg(state, 1);
	const char *mode_text = load_atom_text_arg(state, 2);

	i32 flags = 0;
	i32 creation_mode = SYS_OPEN_EXISTING;
	if (!parse_file_open_mode(state, mode_text, &flags, &creation_mode)) {
		push_value(state, value_nil());
		return 1;
	}

	elf_Handle file = elf_platform_access_file(name, flags, creation_mode);
	push_optional_handle(state, file);
	return 1;
}

ELF_FUNCTION(lib_sys_close_file)
{
	elf_State *state = S;
	elf_Handle file = load_handle_arg(state, 1);

	if (!ELF_IS_HANDLE_INVALID(file)) {
		elf_platform_close_file(file);
	}

	return 0;
}

ELF_FUNCTION(lib_sys_get_file_size)
{
	elf_State *state = S;
	elf_Handle file = load_handle_arg(state, 1);

	if (ELF_IS_HANDLE_INVALID(file)) {
		push_value(state, value_nil());
	}
	else {
		push_value(state, value_from_integer(elf_platform_get_file_size(file)));
	}

	return 1;
}

ELF_FUNCTION(lib_sys_move_file_cursor)
{
	elf_State *state = S;
	elf_Handle file = load_handle_arg(state, 1);
	i32 relative_to = (i32)load_integer_arg(state, 2);
	i32 distance = (i32)load_integer_arg(state, 3);

	push_value(state, value_from_integer(sys_move_file_cursor(file, relative_to, distance)));
	return 1;
}

ELF_FUNCTION(lib_sys_read_console)
{
	elf_State *state = S;
	i32 buffer_size = (i32)load_integer_arg(state, 1);
	Scratch scratch = get_scratch();
	char *buffer = arena_push_zero(scratch.arena, buffer_size + 1);
	i32 read = sys_read_console(SYS_STD_INPUT, buffer, buffer_size);

	push_atom_data(state, buffer, read);
	end_scratch(scratch);
	return 1;
}

ELF_FUNCTION(lib_sys_read_file)
{
	elf_State *state = S;
	elf_Value file_arg = load_value(state, 1);
	elf_Handle file = ELF_HINVALID;
	b32 should_close = false;

	if (value_is_atom(file_arg))
	{
		const char *name = elf_atom_data(value_as_atom(file_arg));
		file = elf_platform_access_file(name, SYS_OPEN_READ, SYS_OPEN_EXISTING);
		should_close = true;
	}
	else if (value_is_handle(file_arg))
	{
		file = value_as_handle(file_arg);
	}
	else
	{
		check_value_type_rule(state, file_arg, TRULE_ATOM | TRULE_HANDLE);
	}

	if (ELF_IS_HANDLE_INVALID(file)) {
		push_value(state, value_nil());
		return 1;
	}

	i32 size = -1;
	if (nargs > 2) {
		size = (i32)load_integer_arg(state, 2);
	}

	if (size < 0) {
		size = (i32)elf_platform_get_file_size(file);
	}

	Scratch scratch = get_scratch();
	char *buffer = arena_push(scratch.arena, size + 1);
	i32 read = elf_platform_read_file(file, buffer, size);
	buffer[read] = 0;

	if (should_close) {
		elf_platform_close_file(file);
	}

	push_atom_data(state, buffer, read);
	end_scratch(scratch);
	return 1;
}

ELF_FUNCTION(lib_sys_write_file)
{
	elf_State *state = S;
	elf_Value file_arg = load_value(state, 1);
	elf_Handle file = ELF_HINVALID;
	b32 should_close = false;

	if (value_is_handle(file_arg))
	{
		file = value_as_handle(file_arg);
	}
	else
	{
		check_value_type(state, file_arg, ELF_VALUE_TYPE_ATOM);
		const char *name = elf_atom_data(value_as_atom(file_arg));
		file = elf_platform_access_file(name, SYS_OPEN_WRITE, SYS_CREATE_ALWAYS);
		should_close = true;
	}

	if (ELF_IS_HANDLE_INVALID(file)) {
		push_value(state, value_from_integer(false));
		return 1;
	}

	elf_Value data = load_value(state, 2);
	check_value_type(state, data, ELF_VALUE_TYPE_ATOM);

	i32 size = value_as_atom(data)->size;
	void *memory = value_as_atom(data)->data;

	sys_write_file(file, memory, size);

	if (should_close) {
		elf_platform_close_file(file);
	}

	push_value(state, value_from_integer(true));
	return 1;
}

ELF_FUNCTION(lib_sys_write_file_to_file)
{
	elf_State *state = S;
	elf_Handle dst = load_handle_arg(state, 1);
	elf_Handle src = load_handle_arg(state, 2);
	i32 size = (i32)elf_platform_get_file_size(src);

	Scratch scratch = get_scratch();
	char *buffer = arena_push(scratch.arena, size);
	elf_platform_read_file(src, buffer, size);
	sys_write_file(dst, buffer, size);
	end_scratch(scratch);

	return 0;
}

ELF_FUNCTION(lib_sys_change_work_dir)
{
	elf_State *state = S;
	const char *path = load_atom_text_arg(state, 1);
	push_value(state, value_from_integer(sys_set_work_dir(path)));
	return 1;
}

ELF_FUNCTION(lib_sys_get_work_dir)
{
	elf_State *state = S;
	char buffer[256];
	sys_get_work_dir(buffer, sizeof(buffer));
	push_value(state, atom_value_from_text(state, buffer));
	return 1;
}

ELF_FUNCTION(lib_sys_create_process)
{
	elf_State *state = S;
	const char *command = load_atom_text_arg(state, 1);
	push_optional_handle(state, sys_create_process(0, command));
	return 1;
}

ELF_FUNCTION(lib_sys_exit_this_process)
{
	elf_State *state = S;
	sys_exit_this_process((i32)load_integer_arg(state, 1));
	return 0;
}

ELF_FUNCTION(lib_sys_get_this_process_id)
{
	elf_State *state = S;
	push_value(state, value_from_integer(sys_get_this_process_id()));
	return 1;
}

static const elf_Binding l_sys[] = {
	{"load_dll",                 lib_sys_load_dll},
	{"get_dll_fn",               lib_sys_get_dll_fn},

	{"get_file_tree",            lib_sys_get_file_tree},
	{"get_path_list",            lib_sys_get_path_list},

	{"open_temp_file",           lib_sys_open_temp_file},
	{"open_file",                lib_sys_open_file},
	{"close_file",               lib_sys_close_file},
	{"get_file_size",            lib_sys_get_file_size},
	{"read_file",                lib_sys_read_file},
	{"read_console",             lib_sys_read_console},
	{"move_file_cursor",         lib_sys_move_file_cursor},
	{"write_file",               lib_sys_write_file},
	{"write_file_to_file",       lib_sys_write_file_to_file},
	{"change_work_dir",          lib_sys_change_work_dir},
	{"get_work_dir",             lib_sys_get_work_dir},

	{"get_file_times",           lib_sys_get_file_times},
	{"file_time_to_system_time", lib_sys_file_time_to_system_time},
	{"sleep",                    lib_sys_sleep},

	{"slice_path",               lib_sys_slice_path},
	{"get_file_name",            lib_sys_get_file_name},
	{"get_file_extension",       lib_sys_get_file_extension},
	{"get_parent_path",          lib_sys_get_parent_path},

	{"create_directory",         lib_sys_create_directory},
	{"delete_file",              lib_sys_delete_file},

	{"create_process",           lib_sys_create_process},
	{"get_process_id",           lib_sys_get_this_process_id},
	{"exit",                     lib_sys_exit_this_process},

	{"get_perf_counter",         lib_sys_get_performance_counter},
	{"get_perf_frequency",       lib_sys_get_performance_counter_frequency},
	{"get_perf_elapsed_s",       lib_sys_get_performance_counter_elapsed_s},
	{"get_perf_elapsed_ms",      lib_sys_get_performance_counter_elapsed_ms},
};

static elf_Table *elf_lib_sys(elf_State *state)
{
	return new_binding_table(state, l_sys, ARRAY_COUNT(l_sys));
}
