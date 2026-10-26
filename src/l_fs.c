//
// Optional filesystem operations.
//

#define ELF_FS_MAX_RECURSION 32

typedef struct
{
	dy_File file;
	b32 readable;
	b32 writable;
	b32 append;
}
FsFile;

static void fs_file_destroy(elf_State *state, void *data)
{
	(void)state;
	FsFile *file = data;
	if (dy_file_is_valid(file->file))
	{
		dy_file_close(file->file);
		file->file = (dy_File) {0};
	}
}

static const elf_UserType fs_file_type = {
	.name = "file",
	.destroy = fs_file_destroy,
};

static void fs_push_errorf(elf_State *state, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	Scratch scratch = scratch_begin();
	dy_String message = arena_pushfv(scratch.arena, format, args);
	va_end(args);
	arena_push_zero(scratch.arena, 1, 1);
	elf_push_error(state, message.data ? message.data : "file operation failed");
	scratch_end(scratch);
}

static void fs_push_io_error(elf_State *state, const char *operation, dy_Result result)
{
	Scratch scratch = scratch_begin();
	dy_String system_message = {0};
	dy_String message = {0};
	if (result.os_error && !dy_error_message(scratch.arena, result.os_error, &system_message).error)
	{
		message = arena_pushf(scratch.arena, "%s failed: %.*s", operation, (int)system_message.size, system_message.data);
	}
	else
	{
		message = arena_pushf(scratch.arena, "%s failed (error %u)", operation, (u32)result.error);
	}
	elf_String *error_message = elf_string_from_data(state, message.data ? message.data : "file operation failed");
	push_value(state, str_value(error_message));
	scratch_end(scratch);

	elf_Table *data = push_new_table(state);
	elf_Value code_key = str_value(elf_string_from_data(state, "code"));
	elf_table_set(state, data, code_key, int_value(result.error));
	elf_Value os_code_key = str_value(elf_string_from_data(state, "os_code"));
	elf_table_set(state, data, os_code_key, int_value(result.os_error));

	elf_GCError *error = elf_error_new(state, error_message, data);
	pop_value(state);
	pop_value(state);
	push_value(state, value_from_error(error));
}

static b32 fs_check_arg_count(elf_State *state, u32 nargs, u32 minimum, u32 maximum, const char *operation)
{
	u32 count = nargs - 1;
	b32 valid = count >= minimum && count <= maximum;
	if (!valid)
	{
		if (minimum == maximum)
		{
			fs_push_errorf(state, "%s expected %u argument(s), got %u", operation, minimum, count);
		}
		else
		{
			fs_push_errorf(state, "%s expected %u to %u args, got %u", operation, minimum, maximum, count);
		}
	}
	return valid;
}

static b32 fs_load_file_receiver(elf_State *state, FsFile **result)
{
	elf_Value value = load_value(state, 0);
	b32 valid = value_is_user(value) && value_as_user(value)->type == &fs_file_type;
	if (!valid)
	{
		fs_push_errorf(state, "file method expected a file receiver, got '%s'", value_type_name(value.type));
	}
	else
	{
		*result = (FsFile *)value_as_user(value)->data;
	}
	return valid;
}

static b32 fs_load_buffer(elf_State *state, u32 index, elf_ByteBuffer **result)
{
	elf_Value value = load_value(state, index);
	b32 valid = value_is_buffer(value);
	if (!valid)
	{
		fs_push_errorf(state, "expected a buffer, got '%s'", value_type_name(value.type));
	}
	else
	{
		*result = value_as_buffer(value);
	}
	return valid;
}

static b32 fs_load_integer(elf_State *state, u32 index, const char *operation, i64 *result)
{
	elf_Value value = load_value(state, index);
	b32 valid = value_is_numeric(value);
	if (!valid)
	{
		fs_push_errorf(state, "%s expected an integer, got '%s'", operation, value_type_name(value.type));
	}
	else
	{
		*result = value_to_integer(value);
	}
	return valid;
}

static b32 fs_load_string(elf_State *state, u32 index, const char *operation, dy_String *result)
{
	elf_Value value = load_value(state, index);
	b32 valid = value_is_string(value);
	if (!valid)
	{
		fs_push_errorf(state, "%s expected a string, got '%s'", operation, value_type_name(value.type));
	}
	else
	{
		elf_String *string = value_as_string(value);
		*result = dy_string_from_data(elf_string_data(string), elf_string_size(string));
	}
	return valid;
}

static b32 fs_require_open(elf_State *state, FsFile *file)
{
	b32 open = dy_file_is_valid(file->file);
	if (!open)
	{
		elf_push_error(state, "file is closed");
	}
	return open;
}

static b32 fs_load_size(elf_State *state, u32 index, const char *operation, u32 *result)
{
	i64 value = 0;
	if (!fs_load_integer(state, index, operation, &value))
	{
		return false;
	}
	if (value < 0 || (u64)value > UINT32_MAX)
	{
		fs_push_errorf(state, "%s size is out of range: %lld", operation, value);
		return false;
	}
	*result = (u32)value;
	return true;
}

//
// file:read(buffer, count) -> integer | error
// Appends at most count bytes to buffer. Returns the number of bytes appended;
// zero means the file is at EOF.
//
// Example:
// data := elf.buffer()
// bytes_read := file:read(data, 4096)
// if elf.is_error(bytes_read) ? { ret bytes_read }
//
ELF_FUNCTION(lib_fs_file_read)
{
	if (!fs_check_arg_count(S, nargs, 2, 2, "file.read"))
	{
		return 1;
	}
	FsFile *file = 0;
	elf_ByteBuffer *buffer = 0;
	u32 count = 0;
	if (!fs_load_file_receiver(S, &file) || !fs_load_buffer(S, 1, &buffer) || !fs_load_size(S, 2, "file read", &count))
	{
		return 1;
	}
	if (!fs_require_open(S, file))
	{
		return 1;
	}
	if (!file->readable)
	{
		elf_push_error(S, "file is not open for reading");
		return 1;
	}
	if (count > UINT32_MAX - buffer->size)
	{
		elf_push_error(S, "file read exceeds the maximum buffer size");
		return 1;
	}

	u32 offset = buffer->size;
	if (!elf_buffer_reserve_object(S, buffer, offset + count))
	{
		elf_push_error(S, "could not reserve the destination buffer for file read");
		return 1;
	}
	dy_u64 bytes_read = 0;
	dy_Result result = dy_file_read(file->file, buffer->data ? buffer->data + offset : 0, count, &bytes_read);
	ASSERT(bytes_read <= count);
	buffer->size += (u32)bytes_read;
	if (result.error)
	{
		fs_push_io_error(S, "file read", result);
	}
	else
	{
		elf_push_int(S, bytes_read);
	}
	return 1;
}

//
// file:write(buffer, offset = 0, count = buffer:length() - offset) -> integer | error
// Writes a range of bytes from buffer. Returns the number of bytes written.
// Append-mode files always write the range at the current end of the file.
//
// Example:
// data := elf.buffer():append("hello")
// bytes_written := file:write(data, 1, 3)
// if elf.is_error(bytes_written) ? { ret bytes_written }
//
ELF_FUNCTION(lib_fs_file_write)
{
	if (!fs_check_arg_count(S, nargs, 1, 3, "file.write"))
	{
		return 1;
	}
	FsFile *file = 0;
	elf_ByteBuffer *buffer = 0;
	if (!fs_load_file_receiver(S, &file) || !fs_load_buffer(S, 1, &buffer))
	{
		return 1;
	}
	u32 offset = 0;
	u32 count = buffer->size;
	if (nargs > 2)
	{
		if (!fs_load_size(S, 2, "file write offset", &offset))
		{
			return 1;
		}
		if (offset > buffer->size)
		{
			fs_push_errorf(S, "file write offset %u exceeds buffer length %u", offset, buffer->size);
			return 1;
		}
		count = buffer->size - offset;
	}
	if (nargs > 3)
	{
		if (!fs_load_size(S, 3, "file write", &count))
		{
			return 1;
		}
		if (count > buffer->size - offset)
		{
			fs_push_errorf(S, "file write range exceeds buffer length %u", buffer->size);
			return 1;
		}
	}

	if (!fs_require_open(S, file))
	{
		return 1;
	}
	if (!file->writable)
	{
		elf_push_error(S, "file is not open for writing");
		return 1;
	}
	if (file->append)
	{
		dy_u64 position = 0;
		dy_Result seek = dy_file_set_cursor(file->file, DY_SEEK_END, 0, &position);
		if (seek.error)
		{
			fs_push_io_error(S, "file append seek", seek);
			return 1;
		}
	}

	dy_u64 bytes_written = 0;
	const u8 *data = buffer->data ? buffer->data + offset : 0;
	dy_Result result = dy_file_write(file->file, data, count, &bytes_written);
	if (result.error)
	{
		fs_push_io_error(S, "file write", result);
	}
	else
	{
		elf_push_int(S, bytes_written);
	}
	return 1;
}

//
// file:seek(offset, origin) -> integer | error
// Moves the cursor relative to "begin", "current", or "end" and returns its
// resulting absolute byte position.
//
// Example:
// position := file:seek(-4, "end")
// if elf.is_error(position) ? { ret position }
//
ELF_FUNCTION(lib_fs_file_seek)
{
	if (!fs_check_arg_count(S, nargs, 2, 2, "file.seek"))
	{
		return 1;
	}
	FsFile *file = 0;
	i64 distance = 0;
	dy_String origin = {0};
	if (!fs_load_file_receiver(S, &file) || !fs_load_integer(S, 1, "file seek offset", &distance) || !fs_load_string(S, 2, "file seek origin", &origin))
	{
		return 1;
	}
	dy_Seek_Origin seek_origin;
	if (dy_string_is(origin, "begin"))
	{
		seek_origin = DY_SEEK_BEGIN;
	}
	else if (dy_string_is(origin, "current"))
	{
		seek_origin = DY_SEEK_CURRENT;
	}
	else if (dy_string_is(origin, "end"))
	{
		seek_origin = DY_SEEK_END;
	}
	else
	{
		elf_push_error(S, "invalid file seek origin; expected 'begin', 'current', or 'end'");
		return 1;
	}

	if (!fs_require_open(S, file))
	{
		return 1;
	}
	dy_u64 position = 0;
	dy_Result result = dy_file_set_cursor(file->file, seek_origin, distance, &position);
	if (result.error || position > INT64_MAX)
	{
		if (!result.error)
		{
			result.error = DY_ERROR_NOT_SUPPORTED;
		}
		fs_push_io_error(S, "file seek", result);
	}
	else
	{
		elf_push_int(S, (i64)position);
	}
	return 1;
}

//
// file:tell() -> integer | error
// Returns the cursor's current absolute byte position.
//
// Example:
// position := file:tell()
// if elf.is_error(position) ? { ret position }
//
ELF_FUNCTION(lib_fs_file_tell)
{
	if (!fs_check_arg_count(S, nargs, 0, 0, "file.tell"))
	{
		return 1;
	}
	FsFile *file = 0;
	if (!fs_load_file_receiver(S, &file))
	{
		return 1;
	}
	if (!fs_require_open(S, file))
	{
		return 1;
	}
	dy_u64 position = 0;
	dy_Result result = dy_file_set_cursor(file->file, DY_SEEK_CURRENT, 0, &position);
	if (result.error || position > INT64_MAX)
	{
		if (!result.error)
		{
			result.error = DY_ERROR_NOT_SUPPORTED;
		}
		fs_push_io_error(S, "file tell", result);
	}
	else
	{
		elf_push_int(S, (i64)position);
	}
	return 1;
}

//
// file:size() -> integer | error
// Returns the file's current size in bytes without changing its cursor.
//
// Example:
// size := file:size()
// if elf.is_error(size) ? { ret size }
//
ELF_FUNCTION(lib_fs_file_size)
{
	if (!fs_check_arg_count(S, nargs, 0, 0, "file.size"))
	{
		return 1;
	}
	FsFile *file = 0;
	if (!fs_load_file_receiver(S, &file))
	{
		return 1;
	}
	if (!fs_require_open(S, file))
	{
		return 1;
	}
	dy_u64 size = 0;
	dy_Result result = dy_file_size(file->file, &size);
	if (result.error || size > INT64_MAX)
	{
		if (!result.error)
		{
			result.error = DY_ERROR_NOT_SUPPORTED;
		}
		fs_push_io_error(S, "file size", result);
	}
	else
	{
		elf_push_int(S, (i64)size);
	}
	return 1;
}

//
// file:close() -> true | error
// Closes the handle. Closing an already closed file succeeds, and an unclosed
// handle is closed automatically when collected.
//
// Example:
// closed := file:close()
// if elf.is_error(closed) ? { ret closed }
//
ELF_FUNCTION(lib_fs_file_close)
{
	if (!fs_check_arg_count(S, nargs, 0, 0, "file.close"))
	{
		return 1;
	}
	FsFile *file = 0;
	if (!fs_load_file_receiver(S, &file))
	{
		return 1;
	}
	if (!dy_file_is_valid(file->file))
	{
		elf_push_int(S, true);
		return 1;
	}
	dy_Result result = dy_file_close(file->file);
	if (result.error)
	{
		fs_push_io_error(S, "file close", result);
	}
	else
	{
		file->file = (dy_File) {0};
		elf_push_int(S, true);
	}
	return 1;
}

//
// file:flush() -> true | error
// Flushes pending writes to the underlying storage.
//
ELF_FUNCTION(lib_fs_file_flush)
{
	if (!fs_check_arg_count(S, nargs, 0, 0, "file.flush"))
	{
		return 1;
	}
	FsFile *file = 0;
	if (!fs_load_file_receiver(S, &file))
	{
		return 1;
	}
	if (!fs_require_open(S, file))
	{
		return 1;
	}
	if (!file->writable)
	{
		elf_push_error(S, "file is not open for writing");
		return 1;
	}

	dy_Result result = dy_file_flush(file->file);
	if (result.error)
	{
		fs_push_io_error(S, "file flush", result);
	}
	else
	{
		elf_push_int(S, true);
	}
	return 1;
}

static const elf_Binding l_fs_file[] = {
	{"read",  lib_fs_file_read},
	{"write", lib_fs_file_write},
	{"seek",  lib_fs_file_seek},
	{"tell",  lib_fs_file_tell},
	{"size",  lib_fs_file_size},
	{"flush", lib_fs_file_flush},
	{"close", lib_fs_file_close},
};

//
// elf.fs.open(path, mode) -> file | error
// Opens path with one of the modes "r", "w", "a", "r+", "w+", or "a+".
// Append modes place every write at the current end of the file.
//
// Example:
// file := elf.fs.open("data.bin", "r+")
// if elf.is_error(file) ? { ret file }
//
ELF_FUNCTION(lib_fs_open)
{
	if (!fs_check_arg_count(S, nargs, 2, 2, "fs.open"))
	{
		return 1;
	}
	dy_String path = {0};
	dy_String mode = {0};
	if (!fs_load_string(S, 1, "file path", &path) || !fs_load_string(S, 2, "file mode", &mode))
	{
		return 1;
	}
	dy_File_Intent intent;
	dy_File_Access access;
	b32 readable = false;
	b32 writable = false;
	b32 append = false;

	if (dy_string_is(mode, "r"))
	{
		intent = DY_FILE_OPEN_EXISTING;
		access = DY_FILE_READ | DY_FILE_SHARE_READ;
		readable = true;
	}
	else if (dy_string_is(mode, "w"))
	{
		intent = DY_FILE_CREATE_ALWAYS;
		access = DY_FILE_WRITE;
		writable = true;
	}
	else if (dy_string_is(mode, "a"))
	{
		intent = DY_FILE_OPEN_ALWAYS;
		access = DY_FILE_WRITE;
		writable = true;
		append = true;
	}
	else if (dy_string_is(mode, "r+"))
	{
		intent = DY_FILE_OPEN_EXISTING;
		access = DY_FILE_READ | DY_FILE_WRITE;
		readable = true;
		writable = true;
	}
	else if (dy_string_is(mode, "w+"))
	{
		intent = DY_FILE_CREATE_ALWAYS;
		access = DY_FILE_READ | DY_FILE_WRITE;
		readable = true;
		writable = true;
	}
	else if (dy_string_is(mode, "a+"))
	{
		intent = DY_FILE_OPEN_ALWAYS;
		access = DY_FILE_READ | DY_FILE_WRITE;
		readable = true;
		writable = true;
		append = true;
	}
	else
	{
		elf_push_error(S, "invalid file mode; expected 'r', 'w', 'a', 'r+', 'w+', or 'a+'");
		return 1;
	}

	elf_Table *file_metatable = S->metatables.fs_file;
	if (!file_metatable)
	{
		elf_push_error(S, "file metatable is unavailable");
		return 1;
	}
	elf_UserObject *user = elf_user_object_new(S, &fs_file_type, file_metatable, sizeof(FsFile));
	FsFile *file = (FsFile *)user->data;
	file->readable = readable;
	file->writable = writable;
	file->append = append;
	push_value(S, value_from_user(user));

	dy_Result result = dy_file_open(path, intent, access, &file->file);
	if (result.error)
	{
		fs_push_io_error(S, "file open", result);
	}
	return 1;
}

static b32 fs_get_file_info(dy_String path, dy_File_Info *info)
{
	return !dy_file_info(path, info).error &&
		!info->is_directory && !info->is_symbolic_link;
}

static u32 fs_push_path_kind(elf_State *state, u32 nargs, b32 files, b32 directories)
{
	lib_check_arg_count(state, "fs path query", nargs, 1, 1);
	dy_File_Info info = {0};
	b32 found = !dy_file_info(lib_load_string_view(state, 1), &info).error;
	elf_push_int(state, found && ((files && !info.is_directory) || (directories && info.is_directory)));
	return 1;
}

ELF_FUNCTION(lib_fs_exists)
{
	return fs_push_path_kind(S, nargs, true, true);
}

ELF_FUNCTION(lib_fs_is_file)
{
	return fs_push_path_kind(S, nargs, true, false);
}

ELF_FUNCTION(lib_fs_is_directory)
{
	return fs_push_path_kind(S, nargs, false, true);
}

ELF_FUNCTION(lib_fs_file_exists)
{
	lib_check_arg_count(S, "fs.file_exists", nargs, 1, 1);
	dy_File_Info info = {0};
	elf_push_int(S, fs_get_file_info(lib_load_string_view(S, 1), &info));
	return 1;
}

ELF_FUNCTION(lib_fs_get_file_info)
{
	lib_check_arg_count(S, "fs.get_file_info", nargs, 1, 1);
	dy_File_Info info = {0};
	if (!fs_get_file_info(lib_load_string_view(S, 1), &info) || info.size > INT64_MAX) {
		elf_push_nil(S);
		return 1;
	}
	elf_new_table(S);
	elf_i32 result = elf_abs_index(S, -1);
	lib_set_integer_field(S, result, "size_bytes", (i64)info.size);
	lib_set_integer_field(S, result, "created_unix_ms", info.created_unix_ms);
	lib_set_integer_field(S, result, "accessed_unix_ms", info.accessed_unix_ms);
	lib_set_integer_field(S, result, "modified_unix_ms", info.modified_unix_ms);
	return 1;
}

ELF_FUNCTION(lib_fs_create_directory)
{
	lib_check_arg_count(S, "fs.create_directory", nargs, 1, 1);
	elf_push_int(S, !dy_create_directory(lib_load_string_view(S, 1)).error);
	return 1;
}

ELF_FUNCTION(lib_fs_create_directories)
{
	lib_check_arg_count(S, "fs.create_directories", nargs, 1, 1);
	elf_push_int(S, !dy_create_directories(lib_load_string_view(S, 1)).error);
	return 1;
}

static u32 fs_transfer_file(elf_State *state, u32 nargs, b32 move)
{
	lib_check_arg_count(state, move ? "fs.move_file" : "fs.copy_file", nargs, 2, 3);
	dy_String source = lib_load_string_view(state, 1);
	dy_String destination = lib_load_string_view(state, 2);
	b32 overwrite = nargs == 4 ? (b32)lib_load_integer(state, 3) : true;
	dy_Result transfer = move ? dy_file_move(source, destination, overwrite) : dy_file_copy(source, destination, overwrite);
	b32 success = !transfer.error;
	elf_push_int(state, success);
	return 1;
}

ELF_FUNCTION(lib_fs_copy_file)
{
	return fs_transfer_file(S, nargs, false);
}

ELF_FUNCTION(lib_fs_move_file)
{
	return fs_transfer_file(S, nargs, true);
}

ELF_FUNCTION(lib_fs_remove_file)
{
	lib_check_arg_count(S, "fs.remove_file", nargs, 1, 1);
	elf_push_int(S, !dy_file_remove(lib_load_string_view(S, 1)).error);
	return 1;
}

ELF_FUNCTION(lib_fs_remove_directory)
{
	lib_check_arg_count(S, "fs.remove_directory", nargs, 1, 1);
	elf_push_int(S, !dy_remove_directory(lib_load_string_view(S, 1)).error);
	return 1;
}

ELF_FUNCTION(lib_fs_remove_tree)
{
	lib_check_arg_count(S, "fs.remove_tree", nargs, 1, 1);
	elf_push_int(S, !dy_remove_tree(lib_load_string_view(S, 1)).error);
	return 1;
}

ELF_FUNCTION(lib_fs_get_working_directory)
{
	lib_check_arg_count(S, "fs.get_working_directory", nargs, 0, 0);
	Scratch scratch = scratch_begin();
	dy_String directory;
	if (dy_get_current_directory(scratch.arena, &directory).error || directory.size > INT_MAX) {
		scratch_end(scratch);
		elf_push_nil(S);
		return 1;
	}
	elf_push_str(S, directory.data, (int)directory.size);
	scratch_end(scratch);
	return 1;
}

ELF_FUNCTION(lib_fs_set_working_directory)
{
	lib_check_arg_count(S, "fs.set_working_directory", nargs, 1, 1);
	elf_push_int(S, !dy_set_current_directory(lib_load_string_view(S, 1)).error);
	return 1;
}

static b32 fs_collect_paths(elf_State *state, dy_Path_Builder *path, u32 recursion_level,
	elf_i32 output, b32 call_each, u32 *count)
{
	dy_Directory directory;
	dy_Directory_Entry entry;
	dy_Directory_Status status;
	if (dy_find_first_file(path, &directory, &entry, &status).error) return false;
	while (status == DY_DIRECTORY_ENTRY)
	{
		dy_Path_Mark mark = dy_path_mark(path);
		if (!dy_path_push(path, entry.name))
		{
			if (dy_find_next_file(&directory, &entry, &status).error) {
				dy_close_directory(&directory);
				return false;
			}
			continue;
		}
		if (call_each) {
			elf_push_value(state, output);
			elf_push_nil(state);
			elf_push_str(state, path->data, (int)path->size);
			u32 returned_count = elf_call(state, 2);
			ASSERT(elf_pop(state, returned_count) == ELF_ERROR_NONE);
			++*count;
		}
		else {
			elf_push_str(state, path->data, (int)path->size);
			elf_append(state, output);
		}
		if (recursion_level && entry.info.is_directory && !entry.info.is_symbolic_link)
		{
			fs_collect_paths(state, path, recursion_level - 1, output, call_each, count);
		}
		dy_path_pop(path, mark);
		if (dy_find_next_file(&directory, &entry, &status).error) {
			dy_close_directory(&directory);
			return false;
		}
	}
	return !dy_close_directory(&directory).error && status == DY_DIRECTORY_END;
}

ELF_FUNCTION(lib_fs_get_paths)
{
	if (nargs < 1 || nargs > 3) {
		elf_push_nil(S);
		return 1;
	}
	dy_String root = DY_LIT(".");
	if (nargs >= 2)
	{
		elf_Value root_value = load_value(S, 1);
		if (!value_is_string(root_value))
		{
			elf_push_nil(S);
			return 1;
		}
		elf_String *root_string = value_as_string(root_value);
		root = dy_string_from_data(elf_string_data(root_string), elf_string_size(root_string));
	}
	if (root.size == 0 || root.size >= DY_PATH_CAPACITY) {
		elf_push_nil(S);
		return 1;
	}
	elf_Int recursion_level = 0;
	if ((nargs == 3 && !elf_to_int(S, 2, &recursion_level)) || recursion_level < 0 || recursion_level > ELF_FS_MAX_RECURSION) {
		elf_push_nil(S);
		return 1;
	}
	elf_new_array(S);
	elf_i32 result = elf_abs_index(S, -1);
	char storage[DY_PATH_CAPACITY];
	dy_Path_Builder path;
	if (!dy_path_builder_init(&path, storage, sizeof(storage), root)) {
		elf_pop(S, 1);
		elf_push_nil(S);
		return 1;
	}
	u32 count = 0;
	b32 success = fs_collect_paths(S, &path, (u32)recursion_level, result, false, &count);
	if (!success) {
		elf_pop(S, 1);
		elf_push_nil(S);
	}
	return 1;
}

ELF_FUNCTION(lib_fs_for_each_path)
{
	if (nargs < 2 || nargs > 4) {
		elf_push_nil(S);
		return 1;
	}
	dy_String root = DY_LIT(".");
	elf_Int recursion_level = 0;
	elf_i32 callback = 1;
	if (nargs >= 3) {
		elf_Value root_value = load_value(S, 1);
		if (!value_is_string(root_value)) {
			elf_push_nil(S);
			return 1;
		}
		elf_String *root_string = value_as_string(root_value);
		root = dy_string_from_data(elf_string_data(root_string), elf_string_size(root_string));
		callback = 2;
	}
	if (nargs == 4) {
		if (!elf_to_int(S, 2, &recursion_level)) {
			elf_push_nil(S);
			return 1;
		}
		callback = 3;
	}
	if (root.size == 0 || root.size >= DY_PATH_CAPACITY || recursion_level < 0 || recursion_level > ELF_FS_MAX_RECURSION || !elf_is_callable(S, callback)) {
		elf_push_nil(S);
		return 1;
	}
	callback = elf_abs_index(S, callback);
	char storage[DY_PATH_CAPACITY];
	dy_Path_Builder path;
	if (!dy_path_builder_init(&path, storage, sizeof(storage), root)) {
		elf_push_nil(S);
		return 1;
	}
	u32 count = 0;
	b32 success = fs_collect_paths(S, &path, (u32)recursion_level, callback, true, &count);
	if (success) elf_push_int(S, count);
	else elf_push_nil(S);
	return 1;
}

static const elf_Binding l_fs[] = {
	{"open",                  lib_fs_open},
	{"exists",                lib_fs_exists},
	{"is_file",               lib_fs_is_file},
	{"is_directory",          lib_fs_is_directory},
	{"file_exists",           lib_fs_file_exists},
	{"get_paths",             lib_fs_get_paths},
	{"for_each_path",         lib_fs_for_each_path},
	{"get_file_info",         lib_fs_get_file_info},
	{"create_directory",      lib_fs_create_directory},
	{"create_directories",    lib_fs_create_directories},
	{"copy_file",             lib_fs_copy_file},
	{"move_file",             lib_fs_move_file},
	{"remove_file",           lib_fs_remove_file},
	{"remove_directory",      lib_fs_remove_directory},
	{"remove_tree",           lib_fs_remove_tree},
	{"get_working_directory", lib_fs_get_working_directory},
	{"set_working_directory", lib_fs_set_working_directory},
};

static elf_SymTable *elf_lib_fs(elf_State *state)
{
	state->metatables.fs_file = new_binding_table(state, l_fs_file, ARRAY_COUNT(l_fs_file));
	elf_pop(state, 1);
	return new_symbol_table(l_fs, ARRAY_COUNT(l_fs));
}
