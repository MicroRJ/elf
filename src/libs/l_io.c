//
// Streams and pipes backed by dy-core.
//

typedef enum Lib_IO_Status
{
	LIB_IO_SUCCESS,
	LIB_IO_WOULD_BLOCK,
	LIB_IO_END_OF_STREAM,
	LIB_IO_FAILURE,
}
Lib_IO_Status;

typedef struct Lib_Stream
{
	dy_Stream *stream;
}
Lib_Stream;

static void lib_stream_destroy(elf_State *state, void *data)
{
	(void)state;
	Lib_Stream *stream = data;
	if (stream->stream)
	{
		dy_stream_close(stream->stream);
	}
	stream->stream = 0;
}

static const elf_UserType lib_stream_type = {
	.name = "stream",
	.destroy = lib_stream_destroy,
};

static Lib_IO_Status lib_io_status(dy_Error error)
{
	Lib_IO_Status result = LIB_IO_FAILURE;
	switch (error)
	{
		case DY_ERROR_NONE:          result = LIB_IO_SUCCESS;       break;
		case DY_ERROR_WOULD_BLOCK:   result = LIB_IO_WOULD_BLOCK;   break;
		case DY_ERROR_END_OF_STREAM: result = LIB_IO_END_OF_STREAM; break;
		default:                                                         break;
	}
	return result;
}

static Lib_Stream *lib_stream_receiver(elf_State *state)
{
	Lib_Stream *stream = elf_to_user_object(state, 0, &lib_stream_type);
	if (!stream)
	{
		elf_error(state, "expected stream receiver");
	}
	return stream;
}

static Lib_Stream *lib_push_stream(elf_State *state, elf_i32 metatable, dy_Stream *stream)
{
	Lib_Stream *result = 0;
	elf_ErrorCode error = elf_push_user_object(state, &lib_stream_type, metatable, sizeof(*result), (void **)&result);
	if (error != ELF_ERROR_NONE)
	{
		dy_stream_close(stream);
		elf_error(state, "could not create stream object");
	}
	result->stream = stream;
	return result;
}

static elf_i32 lib_stream_metatable(elf_State *state)
{
	elf_get_global(state, "elf");
	b32 has_io = elf_get_field(state, -1, "io");
	b32 has_metatable = has_io && elf_get_field(state, -1, "Stream");
	if (!has_metatable)
	{
		elf_error(state, "stream metatable is unavailable");
	}
	return elf_abs_index(state, -1);
}

static elf_Size lib_stream_load_size(elf_State *state, u32 index, const char *operation)
{
	i64 value = lib_load_integer(state, index);
	if (value < 0 || (u64)value > UINT32_MAX)
	{
		char message[160];
		snprintf(message, sizeof(message), "%s size is out of range", operation);
		elf_error(state, message);
	}
	return (elf_Size)value;
}

ELF_FUNCTION(l_stream_read)
{
	lib_check_arg_count(S, "stream.read", nargs, 2, 3);
	Lib_Stream *stream = lib_stream_receiver(S);
	elf_Value buffer_value = load_value(S, 1);
	check_value_type(S, buffer_value, ELF_VALUE_TYPE_BUFFER);
	elf_ByteBuffer *buffer = value_as_buffer(buffer_value);
	if (buffer->obj.status & ELF_OBJECT_READONLY)
	{
		elf_error(S, "stream.read cannot modify a read-only buffer");
	}
	elf_Size request = lib_stream_load_size(S, 2, "stream read");
	u32 flags = nargs > 3 ? (u32)lib_load_integer(S, 3) : 0;
	if (flags & ~DY_STREAM_READ_AVAILABLE)
	{
		elf_error(S, "invalid stream read flags");
	}
	if (request > UINT32_MAX - buffer->size)
	{
		elf_error(S, "stream read exceeds buffer size range");
	}

	u32 original_size = buffer->size;
	dy_u64 bytes_read = 0;
	dy_Result result = { .error = DY_ERROR_INVALID_ARGUMENT };
	if (stream->stream)
	{
		if (!elf_buffer_resize_object(S, buffer, original_size + (u32)request))
		{
			elf_error(S, "could not grow stream read buffer");
		}
		void *destination = request ? buffer->data + original_size : 0;
		result = dy_stream_read(stream->stream, destination, request, flags, &bytes_read);
		u64 committed = result.error == DY_ERROR_NONE ? (u64)original_size + bytes_read : original_size;
		if (committed > UINT32_MAX || !elf_buffer_resize_object(S, buffer, (u32)committed))
		{
			elf_error(S, "could not commit stream read buffer");
		}
	}

	elf_push_int(S, lib_io_status(result.error));
	elf_push_int(S, (elf_Int)bytes_read);
	return 2;
}

ELF_FUNCTION(l_stream_write)
{
	lib_check_arg_count(S, "stream.write", nargs, 1, 3);
	Lib_Stream *stream = lib_stream_receiver(S);
	const void *data = 0;
	elf_Size size = 0;
	elf_Value source = load_value(S, 1);
	elf_StrSlice string = {0};
	if (value_is_buffer(source))
	{
		elf_ByteBuffer *buffer = value_as_buffer(source);
		data = buffer->data;
		size = buffer->size;
	}
	else if (elf_to_str(S, 1, &string))
	{
		data = string.data;
		size = string.size;
	}
	else
	{
		elf_error(S, "stream.write expected a buffer or string");
	}

	elf_Size offset = nargs > 2 ? lib_stream_load_size(S, 2, "stream write offset") : 0;
	if (offset > size)
	{
		elf_error(S, "stream write offset is out of range");
	}
	elf_Size request = nargs > 3 ? lib_stream_load_size(S, 3, "stream write") : size - offset;
	if (request > size - offset)
	{
		elf_error(S, "stream write range is out of bounds");
	}

	dy_u64 bytes_written = 0;
	dy_Result result = { .error = DY_ERROR_INVALID_ARGUMENT };
	if (stream->stream)
	{
		const elf_u8 *bytes = data;
		result = dy_stream_write(stream->stream, bytes ? bytes + offset : 0, request, &bytes_written);
	}
	elf_push_int(S, lib_io_status(result.error));
	elf_push_int(S, (elf_Int)bytes_written);
	return 2;
}

ELF_FUNCTION(l_stream_close)
{
	lib_check_arg_count(S, "stream.close", nargs, 0, 0);
	Lib_Stream *stream = lib_stream_receiver(S);
	if (stream->stream)
	{
		dy_stream_close(stream->stream);
	}
	stream->stream = 0;
	elf_push_int(S, LIB_IO_SUCCESS);
	return 1;
}

ELF_FUNCTION(l_stream_is_closed)
{
	lib_check_arg_count(S, "stream.is_closed", nargs, 0, 0);
	elf_push_int(S, lib_stream_receiver(S)->stream == 0);
	return 1;
}

ELF_FUNCTION(l_io_pipe)
{
	lib_check_arg_count(S, "io.pipe", nargs, 0, 0);
	elf_i32 metatable = lib_stream_metatable(S);
	dy_Pipe pipe = {0};
	dy_Result result = dy_pipe_create(&pipe);
	elf_push_int(S, lib_io_status(result.error));
	int return_count = 1;
	if (result.error == DY_ERROR_NONE)
	{
		lib_push_stream(S, metatable, pipe.reader);
		pipe.reader = 0;
		lib_push_stream(S, metatable, pipe.writer);
		pipe.writer = 0;
		return_count = 3;
	}
	dy_pipe_close(&pipe);
	return return_count;
}

static int lib_push_standard_stream(elf_State *state, dy_Standard_Stream standard)
{
	elf_i32 metatable = lib_stream_metatable(state);
	dy_Stream *stream = 0;
	dy_Result result = dy_standard_stream_open(standard, &stream);
	elf_push_int(state, lib_io_status(result.error));
	int return_count = 1;
	if (result.error == DY_ERROR_NONE)
	{
		lib_push_stream(state, metatable, stream);
		return_count = 2;
	}
	return return_count;
}

ELF_FUNCTION(l_io_stdin)
{
	lib_check_arg_count(S, "io.stdin", nargs, 0, 0);
	return lib_push_standard_stream(S, DY_STANDARD_INPUT);
}

ELF_FUNCTION(l_io_stdout)
{
	lib_check_arg_count(S, "io.stdout", nargs, 0, 0);
	return lib_push_standard_stream(S, DY_STANDARD_OUTPUT);
}

ELF_FUNCTION(l_io_stderr)
{
	lib_check_arg_count(S, "io.stderr", nargs, 0, 0);
	return lib_push_standard_stream(S, DY_STANDARD_ERROR);
}

static const elf_Binding l_stream[] = {
	{"read",      l_stream_read},
	{"write",     l_stream_write},
	{"close",     l_stream_close},
	{"is_closed", l_stream_is_closed},
};

static const elf_Binding l_io[] = {
	{"pipe",   l_io_pipe},
	{"stdin",  l_io_stdin},
	{"stdout", l_io_stdout},
	{"stderr", l_io_stderr},
};

static void elf_lib_io(elf_State *state)
{
	new_binding_table(state, l_io, ARRAY_COUNT(l_io));
	elf_i32 io = elf_abs_index(state, -1);
	new_binding_table(state, l_stream, ARRAY_COUNT(l_stream));
	elf_set_field(state, io, "Stream");
	lib_set_integer_field(state, io, "SUCCESS", LIB_IO_SUCCESS);
	lib_set_integer_field(state, io, "WOULD_BLOCK", LIB_IO_WOULD_BLOCK);
	lib_set_integer_field(state, io, "END_OF_STREAM", LIB_IO_END_OF_STREAM);
	lib_set_integer_field(state, io, "FAILURE", LIB_IO_FAILURE);
	lib_set_integer_field(state, io, "READ_AVAILABLE", DY_STREAM_READ_AVAILABLE);
}
