//
// See Copyright Notice In elf.h
//

//
// Mutable byte buffers. Reads append to the logical end; reserve changes only
// capacity, while resize changes the visible length and zero-fills growth.
//

static elf_ByteBuffer *buffer_receiver(elf_State *state)
{
	elf_Value value = load_value(state, 0);
	check_value_type(state, value, ELF_VALUE_TYPE_BUFFER);
	return value_as_buffer(value);
}

static b32 buffer_normalize_index(elf_State *state, i64 index, u32 size,
	b32 allow_end, u32 *result)
{
	if (index < 0) index += size;
	i64 upper = allow_end ? size : (i64)size - 1;
	if (index < 0 || index > upper)
	{
		report_runtime_error(state, ERROR_OUT_OF_RANGE,
			"buffer index %lld is out of bounds for length %u", index, size);
		return false;
	}
	*result = (u32)index;
	return true;
}

static u32 buffer_load_size(elf_State *state, u32 argument, const char *operation)
{
	i64 size = lib_load_integer(state, argument);
	if (size < 0 || (u64)size > UINT32_MAX)
	{
		report_runtime_error(state, ERROR_OUT_OF_RANGE,
			"%s size is out of range: %lld", operation, size);
		return 0;
	}
	return (u32)size;
}

ELF_FUNCTION(l_buffer_new)
{
	if (!lib_check_arg_count(S, "buffer", nargs, 0, 1)) {
		return 1;
	}
	u32 capacity = nargs ? buffer_load_size(S, 0, "buffer capacity") : 0;
	push_value(S, value_from_buffer(elf_buffer_new_object(S, capacity)));
	return 1;
}

ELF_FUNCTION(l_buffer_length)
{
	if (!lib_check_arg_count(S, "buffer.length", nargs - 1, 0, 0)) {
		return 1;
	}
	push_value(S, int_value(buffer_receiver(S)->size));
	return 1;
}

ELF_FUNCTION(l_buffer_capacity)
{
	if (!lib_check_arg_count(S, "buffer.capacity", nargs - 1, 0, 0)) {
		return 1;
	}
	push_value(S, int_value(buffer_receiver(S)->capacity));
	return 1;
}

ELF_FUNCTION(l_buffer_clear)
{
	if (!lib_check_arg_count(S, "buffer.clear", nargs - 1, 0, 0)) {
		return 1;
	}
	elf_ByteBuffer *buffer = buffer_receiver(S);
	if (!elf_buffer_require_writable(S, buffer)) return 0;
	buffer->size = 0;
	push_value(S, value_from_buffer(buffer));
	return 1;
}

ELF_FUNCTION(l_buffer_reserve)
{
	if (!lib_check_arg_count(S, "buffer.reserve", nargs - 1, 1, 1)) {
		return 1;
	}
	elf_ByteBuffer *buffer = buffer_receiver(S);
	u32 capacity = buffer_load_size(S, 1, "buffer reserve");
	if (!elf_buffer_reserve_object(S, buffer, capacity)) return 0;
	push_value(S, value_from_buffer(buffer));
	return 1;
}

ELF_FUNCTION(l_buffer_resize)
{
	if (!lib_check_arg_count(S, "buffer.resize", nargs - 1, 1, 1)) {
		return 1;
	}
	elf_ByteBuffer *buffer = buffer_receiver(S);
	u32 size = buffer_load_size(S, 1, "buffer resize");
	if (!elf_buffer_resize_object(S, buffer, size)) return 0;
	push_value(S, value_from_buffer(buffer));
	return 1;
}

ELF_FUNCTION(l_buffer_append)
{
	if (!lib_check_arg_count(S, "buffer.append", nargs - 1, 1, 1)) {
		return 1;
	}
	elf_ByteBuffer *buffer = buffer_receiver(S);
	elf_Value source = load_value(S, 1);
	const void *data = 0;
	u32 size = 0;
	if (value_is_string(source))
	{
		elf_String *string = value_as_string(source);
		data = elf_string_data(string);
		size = elf_string_size(string);
	}
	else if (value_is_buffer(source))
	{
		elf_ByteBuffer *source_buffer = value_as_buffer(source);
		data = source_buffer->data;
		size = source_buffer->size;
	}
	else
	{
		report_runtime_error(S, ERROR_TYPE_MISMATCH,
			"buffer append expected string or buffer, got '%s'",
			value_type_name(source.type));
		return 0;
	}
	if (!elf_buffer_append_object(S, buffer, data, size)) return 0;
	push_value(S, value_from_buffer(buffer));
	return 1;
}

ELF_FUNCTION(l_buffer_splice)
{
	if (!lib_check_arg_count(S, "buffer.splice", nargs - 1, 3, 3)) {
		return 1;
	}
	elf_ByteBuffer *buffer = buffer_receiver(S);
	u32 position = 0;
	if (!buffer_normalize_index(S, lib_load_integer(S, 1), buffer->size, true, &position)) return 0;
	u32 delete_size = buffer_load_size(S, 2, "buffer splice delete");
	elf_Value replacement = load_value(S, 3);
	const void *data = 0;
	u32 insert_size = 0;
	if (value_is_string(replacement))
	{
		elf_String *string = value_as_string(replacement);
		data = elf_string_data(string);
		insert_size = elf_string_size(string);
	}
	else if (value_is_buffer(replacement))
	{
		elf_ByteBuffer *replacement_buffer = value_as_buffer(replacement);
		data = replacement_buffer->data;
		insert_size = replacement_buffer->size;
	}
	else
	{
		report_runtime_error(S, ERROR_TYPE_MISMATCH,
			"buffer splice expected string or buffer, got '%s'",
			value_type_name(replacement.type));
		return 0;
	}
	if (!elf_buffer_splice_object(S, buffer, position, delete_size, data, insert_size)) return 0;
	push_value(S, value_from_buffer(buffer));
	return 1;
}

ELF_FUNCTION(l_buffer_byte)
{
	if (!lib_check_arg_count(S, "buffer.byte", nargs - 1, 1, 1)) {
		return 1;
	}
	elf_ByteBuffer *buffer = buffer_receiver(S);
	u32 index = 0;
	if (!buffer_normalize_index(S, lib_load_integer(S, 1), buffer->size, false, &index)) return 0;
	push_value(S, int_value(buffer->data[index]));
	return 1;
}

ELF_FUNCTION(l_buffer_set)
{
	if (!lib_check_arg_count(S, "buffer.set", nargs - 1, 2, 2)) {
		return 1;
	}
	elf_ByteBuffer *buffer = buffer_receiver(S);
	u32 index = 0;
	if (!buffer_normalize_index(S, lib_load_integer(S, 1), buffer->size, false, &index)) return 0;
	i64 byte = lib_load_integer(S, 2);
	if (byte < 0 || byte > UINT8_MAX)
	{
		report_runtime_error(S, ERROR_OUT_OF_RANGE,
			"buffer byte is out of range: %lld", byte);
		return 0;
	}
	if (!elf_buffer_require_writable(S, buffer)) return 0;
	buffer->data[index] = (u8)byte;
	push_value(S, value_from_buffer(buffer));
	return 1;
}

ELF_FUNCTION(l_buffer_slice)
{
	if (!lib_check_arg_count(S, "buffer.slice", nargs - 1, 1, 2)) {
		return 1;
	}
	elf_ByteBuffer *buffer = buffer_receiver(S);
	u32 first = 0;
	u32 end = buffer->size;
	if (!buffer_normalize_index(S, lib_load_integer(S, 1), buffer->size, true, &first)) return 0;
	if (nargs > 2 && !buffer_normalize_index(S, lib_load_integer(S, 2), buffer->size, true, &end)) return 0;
	if (end < first)
	{
		report_runtime_error(S, ERROR_OUT_OF_RANGE,
			"buffer slice end %u precedes start %u", end, first);
		return 0;
	}
	elf_ByteBuffer *result = elf_buffer_new_object(S, end - first);
	const u8 *data = buffer->data ? buffer->data + first : 0;
	if (!elf_buffer_append_object(S, result, data, end - first)) return 0;
	push_value(S, value_from_buffer(result));
	return 1;
}

ELF_FUNCTION(l_buffer_to_string)
{
	if (!lib_check_arg_count(S, "buffer.to_string", nargs - 1, 0, 0)) {
		return 1;
	}
	elf_ByteBuffer *buffer = buffer_receiver(S);
	push_value(S, str_value(elf_string_from_data_size(S,
		(const char *)(buffer->data ? buffer->data : (u8 *)""), buffer->size)));
	return 1;
}

static const elf_Binding l_buffer[] = {
	{"length",    l_buffer_length},
	{"capacity",  l_buffer_capacity},
	{"clear",     l_buffer_clear},
	{"reserve",   l_buffer_reserve},
	{"resize",    l_buffer_resize},
	{"append",    l_buffer_append},
	{"splice",    l_buffer_splice},
	{"byte",      l_buffer_byte},
	{"set",       l_buffer_set},
	{"slice",     l_buffer_slice},
	{"to_string", l_buffer_to_string},
};

static elf_Table *elf_lib_buffer(elf_State *state)
{
	return new_binding_table(state, l_buffer, ARRAY_COUNT(l_buffer));
}
