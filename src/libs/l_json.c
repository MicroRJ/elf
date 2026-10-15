//
// JSON parsing and serialization.
//

typedef struct
{
	elf_State  *state;
	char       *data;
	u64         count;
	u64      capacity;
	const char *error;
	elf_Object *ancestors[256];
}
JsonWriter;

static void json_push_error_value(elf_State *state, const char *message, const Com_Message *compiler_message)
{
	elf_Table *info = elf_table_new(state);
	push_value(state, value_from_error(info));

	elf_Value message_key = value_from_string(elf_string_from_data(state, "message"));
	elf_Value message_value = value_from_string(elf_string_from_data(state, message ? message : "JSON error"));
	elf_table_set(state, info, message_key, message_value);

	if (compiler_message)
	{
		SourceSite site = compiler_message->site;
		i64 column = site.line_index && site.line_offset <= site.offset ? 1 + (i64)(site.offset - site.line_offset) : 0;
		elf_Value line_key = value_from_string(elf_string_from_data(state, "line"));
		elf_Value column_key = value_from_string(elf_string_from_data(state, "column"));
		elf_table_set(state, info, line_key, value_from_integer(site.line_index));
		elf_table_set(state, info, column_key, value_from_integer(column));
	}
}

static b32 json_writer_reserve(JsonWriter *writer, u64 additional)
{
	if (writer->error) return false;
	if (additional > UINT64_MAX - writer->count)
	{
		writer->error = "JSON output is too large";
		return false;
	}

	u64 required = writer->count + additional;
	if (required <= writer->capacity) return true;

	u64 capacity = writer->capacity ? writer->capacity : 256;
	while (capacity < required)
	{
		if (capacity > UINT64_MAX / 2)
		{
			writer->error = "JSON output is too large";
			return false;
		}
		capacity *= 2;
	}
	if (capacity > SIZE_MAX)
	{
		writer->error = "JSON output is too large";
		return false;
	}

	char *data = elf_realloc(writer->state, writer->data,
		writer->capacity, capacity);
	if (!data)
	{
		writer->error = "could not allocate JSON output";
		return false;
	}
	writer->data = data;
	writer->capacity = capacity;
	return true;
}

static void json_writer_append(JsonWriter *writer, const char *data, u64 size)
{
	if (!json_writer_reserve(writer, size)) return;
	copy_memory(writer->data + writer->count, data, size);
	writer->count += size;
}

static void json_writer_char(JsonWriter *writer, char character)
{
	json_writer_append(writer, &character, 1);
}

static void json_writer_string(JsonWriter *writer, elf_String *string)
{
	static const char hex[] = "0123456789abcdef";
	json_writer_char(writer, '"');
	const u8 *data = (const u8 *)elf_string_data(string);
	for (u32 i = 0; i < elf_string_size(string) && !writer->error; ++i)
	{
		u8 byte = data[i];
		switch (byte)
		{
			case '"': json_writer_append(writer, "\\\"", 2); break;
			case '\\': json_writer_append(writer, "\\\\", 2); break;
			case '\b': json_writer_append(writer, "\\b", 2); break;
			case '\f': json_writer_append(writer, "\\f", 2); break;
			case '\n': json_writer_append(writer, "\\n", 2); break;
			case '\r': json_writer_append(writer, "\\r", 2); break;
			case '\t': json_writer_append(writer, "\\t", 2); break;
			default:
			{
				if (byte < 0x20)
				{
					char escape[] = {'\\', 'u', '0', '0', hex[byte >> 4], hex[byte & 15]};
					json_writer_append(writer, escape, sizeof(escape));
				}
				else
				{
					json_writer_char(writer, (char)byte);
				}
			}
			break;
		}
	}
	json_writer_char(writer, '"');
}

static void json_write_value(JsonWriter *writer, elf_Value value, u32 depth);

static b32 json_writer_enter_object(JsonWriter *writer, elf_Object *object, u32 depth)
{
	if (depth >= ARRAY_COUNT(writer->ancestors))
	{
		writer->error = "JSON value exceeds the maximum nesting depth";
		return false;
	}
	for (u32 i = 0; i < depth; ++i)
	{
		if (writer->ancestors[i] == object)
		{
			writer->error = "cannot stringify a cyclic value";
			return false;
		}
	}
	writer->ancestors[depth] = object;
	return true;
}

static void json_write_table(JsonWriter *writer, elf_Table *table, u32 depth)
{
	if (!json_writer_enter_object(writer, (elf_Object *)table, depth))
	{
		return;
	}

	json_writer_char(writer, '{');
	b32 wrote_entry = false;
	for (u32 i = 0; i < table->capacity && !writer->error; ++i)
	{
		Entry entry = table->entries[i];
		if (!elf_table_entry_is_key(entry))
		{
			continue;
		}
		elf_Value key = entry.key;
		if (!value_is_string(key))
		{
			writer->error = "JSON object keys must be strings";
			break;
		}
		if (wrote_entry)
		{
			json_writer_char(writer, ',');
		}
		wrote_entry = true;
		json_writer_string(writer, value_as_string(key));
		json_writer_char(writer, ':');
		json_write_value(writer, entry.value, depth + 1);
	}
	json_writer_char(writer, '}');
}

static void json_write_array(JsonWriter *writer, elf_Array *array, u32 depth)
{
	if (!json_writer_enter_object(writer, (elf_Object *)array, depth))
	{
		return;
	}

	json_writer_char(writer, '[');
	for (u32 i = 0; i < array->count && !writer->error; ++i)
	{
		if (i)
		{
			json_writer_char(writer, ',');
		}
		json_write_value(writer, array->items[i], depth + 1);
	}
	json_writer_char(writer, ']');
}

static void json_write_value(JsonWriter *writer, elf_Value value, u32 depth)
{
	if (writer->error) return;

	if (value_is_nil(value))
	{
		json_writer_append(writer, "null", 4);
	}
	else if (value_is_integer(value))
	{
		char number[32];
		int size = snprintf(number, sizeof(number), "%lld", (long long)value_as_integer(value));
		json_writer_append(writer, number, (u64)size);
	}
	else if (value_is_number(value))
	{
		f64 number_value = value_as_number(value);
		if (!isfinite(number_value))
		{
			writer->error = "JSON cannot represent non-finite numbers";
			return;
		}
		char number[64];
		int size = snprintf(number, sizeof(number), "%.17g", number_value);
		json_writer_append(writer, number, (u64)size);
	}
	else if (value_is_string(value))
	{
		json_writer_string(writer, value_as_string(value));
	}
	else if (value_is_table(value))
	{
		json_write_table(writer, value_as_table(value), depth);
	}
	else if (value_is_array(value))
	{
		json_write_array(writer, value_as_array(value), depth);
	}
	else
	{
		writer->error = "value cannot be represented as JSON";
	}
}

ELF_FUNCTION(l_json_parse)
{
	lib_check_arg_count(S, "json.parse", nargs, 1, 1);
	elf_String *text = lib_load_string(S, 1);
	elf_StrSlice source = {(char *)elf_string_data(text), elf_string_size(text)};
	elf_Scratch scratch = elf_begin_scratch();
	Compiler *compiler = compiler_create(scratch.arena);
	compiler_set_log_messages(compiler, false);
	compiler_add_root_source(compiler, "<json>", source);
	if (!compiler_push_json(compiler, S))
	{
		const Com_Message *compiler_message = compiler->report.first;
		const char *message = compiler_message ? compiler_message->text.data : "invalid JSON";
		json_push_error_value(S, message, compiler_message);
	}
	elf_end_scratch(scratch);
	return 1;
}

ELF_FUNCTION(l_json_stringify)
{
	lib_check_arg_count(S, "json.stringify", nargs, 1, 1);
	JsonWriter writer = {.state = S};
	json_write_value(&writer, load_value(S, 1), 0);
	if (writer.error)
	{
		json_push_error_value(S, writer.error, 0);
	}
	else
	{
		elf_push_str(S, writer.data ? writer.data : "", writer.count);
	}
	elf_free(S, writer.data, writer.capacity);
	return 1;
}

ELF_FUNCTION(l_json_load_file)
{
	lib_check_arg_count(S, "json.load_file", nargs, 1, 1);
	const char *name = lib_load_cstr(S, 1);
	dy_File_Info info;
	if (dy_get_file_info(dy_string_from_cstring(name), &info).error || info.is_directory || info.size > INT_MAX)
	{
		elf_push_nil(S);
		return 1;
	}

	dy_File file;
	if (dy_access_file(dy_string_from_cstring(name), DY_FILE_OPEN_EXISTING, DY_FILE_READ | DY_FILE_SHARE_READ, &file).error)
	{
		elf_push_nil(S);
		return 1;
	}

	u64 allocation_size = info.size + 1;
	char *data = elf_alloc(S, allocation_size);
	if (!data)
	{
		dy_close_file(file);
		elf_push_nil(S);
		return 1;
	}

	dy_u64 size = 0;
	dy_Result read = dy_read_file(file, data, info.size, &size);
	dy_close_file(file);
	if (read.error || size != info.size)
	{
		elf_free(S, data, allocation_size);
		elf_push_nil(S);
		return 1;
	}

	data[size] = 0;
	elf_StrSlice source = {data, size};
	if (elf_push_json(S, name, source) != ELF_ERROR_NONE)
	{
		elf_push_nil(S);
	}
	elf_free(S, data, allocation_size);
	return 1;
}

static const elf_Binding l_json[] = {
	{"parse",     l_json_parse},
	{"stringify", l_json_stringify},
	{"load_file", l_json_load_file},
};

static elf_Table *elf_lib_json(elf_State *state)
{
	return new_binding_table(state, l_json, ARRAY_COUNT(l_json));
}
