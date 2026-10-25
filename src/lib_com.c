//
// See Copyright Notice In elf.h
//

typedef enum
{
	COMPILER_PARSE_JSON,
	COMPILER_PARSE_JSONL,
}
CompilerParseType;

static Ast compiler_parse_json_source(Arena *arena, elf_StrSlice name, elf_StrSlice source, CompilerParseType type)
{
	if (!arena || !name.data || name.size > UINT32_MAX || !source.data || source.size > UINT32_MAX)
	{
		return ERROR_AST;
	}

	Compiler *compiler = compiler_create(arena);
	if (!compiler)
	{
		return ERROR_AST;
	}

	Atom *source_name = atom_from_data_size(compiler->atoms, name.data, (u32)name.size);
	char *source_data = arena_push_zero(arena, source.size + 1, 1);
	if (!source_name || !source_data)
	{
		return ERROR_AST;
	}
	copy_memory(source_data, source.data, source.size);

	Com_Source *compiler_source = compiler_alloc_source(compiler, source_name, source_name, 0, (dy_String) {.data = source_data, .size = source.size});
	if (!compiler_source)
	{
		return ERROR_AST;
	}

	Parser parser = {0};
	if (!parser_init(&parser))
	{
		return ERROR_AST;
	}
	Ast ast = type == COMPILER_PARSE_JSON ? parser_parse_json(&parser, compiler_source) : parser_parse_jsonl(&parser, compiler_source);
	parser_destroy(&parser);
	return compiler->error_count ? ERROR_AST : ast;
}

static b32 compiler_push_json_ast(elf_State *state, Ast ast)
{
	elf_Index stack_checkpoint = elf_get_top(state);
	b32 result = true;
	switch (ast->kind)
	{
		case AST_NIL_LITERAL:
		{
			elf_push_nil(state);
		}
		break;

		case AST_INTEGER_LITERAL:
		{
			elf_push_int(state, ast->integer_value);
		}
		break;

		case AST_NUMBER_LITERAL:
		{
			elf_push_num(state, ast->number_value);
		}
		break;

		case AST_STRING_LITERAL:
		{
			elf_push_str(state, atom_data(ast->atom), atom_size(ast->atom));
		}
		break;

		case AST_TABLE:
		{
			elf_new_table(state);
			elf_Index table = elf_abs_index(state, -1);
			for (u32 i = 0; result && i < ast->table.count; ++i)
			{
				Ast entry = ast->table.items[i];
				Ast key = entry && entry->kind == AST_TABLE_ENTRY ? entry->table_entry.key : 0;
				if (!key || key->kind != AST_STRING_LITERAL)
				{
					result = false;
				}
				else
				{
					result = compiler_push_json_ast(state, entry->table_entry.value);
					if (result)
					{
						elf_StrSlice field = {(char *)atom_data(key->atom), atom_size(key->atom)};
						result = elf_set_field_slice(state, table, field) == ELF_ERROR_NONE;
					}
				}
			}
		}
		break;

		case AST_ARRAY:
		{
			elf_new_array(state);
			elf_Index array = elf_abs_index(state, -1);
			for (u32 i = 0; result && i < ast->array.count; ++i)
			{
				result = compiler_push_json_ast(state, ast->array.items[i]);
				if (result)
				{
					result = elf_append(state, array) == ELF_ERROR_NONE;
				}
			}
		}
		break;

		default:
		{
			result = false;
		}
		break;
	}
	if (!result)
	{
		elf_set_top(state, stack_checkpoint);
	}
	ASSERT(!result || elf_get_top(state) == stack_checkpoint + 1);
	return result;
}

static b32 compiler_push_json(elf_State *state, elf_StrSlice name, elf_StrSlice source, b32 jsonl)
{
	if (!state)
	{
		return false;
	}

	Scratch scratch = scratch_begin();
	CompilerParseType type = jsonl ? COMPILER_PARSE_JSONL : COMPILER_PARSE_JSON;
	Ast ast = compiler_parse_json_source(scratch.arena, name, source, type);
	b32 result = !ast_is_error(ast) && compiler_push_json_ast(state, ast);
	scratch_end(scratch);
	return result;
}


ELF_FUNCTION(l_core_load_file)
{
	lib_check_arg_count(S, "load_file", nargs, 1, INT_MAX);
	dy_String name = lib_load_string_view(S, 1);
	if (!elf_push_code_file(S, 0, (elf_StrSlice) {name.data, name.size})) return 0;

	elf_push_value(S, 0);
	for (i64 index = 2; index < nargs; ++index)
	{
		elf_push_value(S, (elf_i32)index);
	}
	return elf_call(S, nargs - 1);
}

// JSON parsing and serialization.

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

static b32 lib_push_json_source(elf_State *state, dy_String name, dy_String source, b32 jsonl)
{
	elf_StrSlice source_name = {name.data, name.size};
	elf_StrSlice source_text = {source.data, source.size};
	return compiler_push_json(state, source_name, source_text, jsonl);
}

static b32 lib_push_json(elf_State *state, dy_String name, dy_String source)
{
	return lib_push_json_source(state, name, source, false);
}

static b32 lib_push_jsonl(elf_State *state, dy_String name, dy_String source)
{
	return lib_push_json_source(state, name, source, true);
}

static void json_push_error_value(elf_State *state, const char *message)
{
	elf_push_error(state, message ? message : "JSON error");
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
	dy_String source = lib_load_string_view(S, 1);
	if (!lib_push_json(S, DY_LIT("json.parse"), source)) {
		json_push_error_value(S, "invalid JSON");
	}
	return 1;
}

ELF_FUNCTION(l_json_stringify)
{
	lib_check_arg_count(S, "json.stringify", nargs, 1, 1);
	JsonWriter writer = {.state = S};
	json_write_value(&writer, load_value(S, 1), 0);
	if (writer.error)
	{
		json_push_error_value(S, writer.error);
	}
	else
	{
		elf_push_str(S, writer.data ? writer.data : "", writer.count);
	}
	elf_free(S, writer.data, writer.capacity);
	return 1;
}

static b32 json_read_file(elf_State *state, dy_String name, dy_String *source, u64 *allocation_size)
{
	dy_File_Info info;
	if (dy_file_info(name, &info).error || info.is_directory || info.size > INT_MAX)
	{
		return false;
	}

	dy_File file;
	if (dy_file_open(name, DY_FILE_OPEN_EXISTING, DY_FILE_READ | DY_FILE_SHARE_READ, &file).error)
	{
		return false;
	}

	*allocation_size = info.size + 1;
	char *data = elf_alloc(state, *allocation_size);
	if (!data)
	{
		dy_file_close(file);
		return false;
	}

	dy_u64 size = 0;
	dy_Result read = dy_file_read(file, data, info.size, &size);
	dy_file_close(file);
	if (read.error || size != info.size)
	{
		elf_free(state, data, *allocation_size);
		return false;
	}

	data[size] = 0;
	*source = dy_string_from_data(data, size);
	return true;
}

static void json_push_loaded_file(elf_State *state, dy_String name, b32 jsonl)
{
	dy_String source = {};
	u64 allocation_size = 0;
	if (!json_read_file(state, name, &source, &allocation_size))
	{
		elf_push_nil(state);
		return;
	}

	b32 loaded = jsonl ? lib_push_jsonl(state, name, source) : lib_push_json(state, name, source);
	if (!loaded)
	{
		elf_push_nil(state);
	}
	elf_free(state, source.data, allocation_size);
}

ELF_FUNCTION(l_json_load_file)
{
	lib_check_arg_count(S, "json.load_file", nargs, 1, 1);
	json_push_loaded_file(S, lib_load_string_view(S, 1), false);
	return 1;
}

ELF_FUNCTION(l_json_load_jsonl)
{
	lib_check_arg_count(S, "json.load_jsonl", nargs, 1, 1);
	json_push_loaded_file(S, lib_load_string_view(S, 1), true);
	return 1;
}

static const elf_Binding l_json[] = {
	{"parse",      l_json_parse},
	{"stringify",  l_json_stringify},
	{"load_file",  l_json_load_file},
	{"load_jsonl", l_json_load_jsonl},
};

static elf_SymTable *elf_lib_json(void)
{
	return new_symbol_table(l_json, ARRAY_COUNT(l_json));
}

void elf_open_compiler_libraries(elf_State *state, elf_SymTable *elf_symbols)
{
	ASSERT(state);
	ASSERT(elf_symbols);
	elf_sym_tab(elf_symbols, ELF_LIT("json"), elf_lib_json());
	elf_sym_fun(elf_symbols, ELF_LIT("load_file"), l_core_load_file);
}
