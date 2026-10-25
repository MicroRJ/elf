//
// See Copyright Notice In elf.h
//

#include "elf_value_text.h"

static void table_to_text(Arena *arena, elf_Table *table)
{
	arena_push_text(arena, "{");
	u32 emitted = 0;
	for (u32 i = 0; i < table->capacity; ++i)
	{
		Entry entry = table->entries[i];
		if (elf_table_entry_is_key(entry))
		{
			if (emitted++)
			{
				arena_push_text(arena, ", ");
			}
			elf_print_value(arena, entry.key);
			arena_push_text(arena, " = ");
			elf_print_value(arena, entry.value);
		}
	}
	arena_push_text(arena, "}");
}

static void array_to_text(Arena *arena, elf_Array *array)
{
	arena_push_text(arena, "[");
	for (u32 i = 0; i < array->count; ++i)
	{
		if (i != 0)
		{
			arena_push_text(arena, ", ");
		}
		elf_print_value(arena, array->items[i]);
	}
	arena_push_text(arena, "]");
}

static void error_to_text(Arena *arena, elf_GCError *error)
{
	arena_push_data(arena, elf_string_data(error->message), elf_string_size(error->message));
}

void elf_print_value(Arena *arena, elf_Value value)
{
	switch (value.type)
	{
		case ELF_VALUE_TYPE_NIL:       arena_push_text(arena, "nil"); break;
		case ELF_VALUE_TYPE_INTEGER:   arena_pushf(arena, "%lli", value_as_integer(value)); break;
		case ELF_VALUE_TYPE_NUMBER:    arena_pushf(arena, "%f", value.x_num); break;
		case ELF_VALUE_TYPE_STRING:    arena_push_data(arena, value.x_string->data, elf_string_size(value.x_string)); break;
		case ELF_VALUE_TYPE_CLOSURE:   arena_push_text(arena, "C()"); break;
		case ELF_VALUE_TYPE_CFUNCTION: arena_push_text(arena, "F()"); break;
		case ELF_VALUE_TYPE_TABLE:     table_to_text(arena, value_as_table(value)); break;
		case ELF_VALUE_TYPE_ARRAY:     array_to_text(arena, value_as_array(value)); break;
		case ELF_VALUE_TYPE_ERROR:     error_to_text(arena, value_as_error(value)); break;
		case ELF_VALUE_TYPE_BUFFER:    arena_pushf(arena, "buffer(%u)", value_as_buffer(value)->size); break;
		case ELF_VALUE_TYPE_USER_OBJECT: arena_pushf(arena, "%s()", value_name(value)); break;
		default:                       arena_push_text(arena, "(?)"); break;
	}
}

static void source_indent(Arena *arena, u32 indent)
{
	char *data = arena_push(arena, indent, 1);
	if (data)
	{
		dy_memory_fill(data, '\t', indent);
	}
}

typedef struct SourceValuePath SourceValuePath;
struct SourceValuePath
{
	SourceValuePath *parent;
	elf_Object      *object;
};

static b32 unparse_value(elf_State *state, Arena *arena, elf_Value value, u32 indent, SourceValuePath *path);

static b32 source_path_contains(SourceValuePath *path, elf_Object *object)
{
	for (SourceValuePath *at = path; at; at = at->parent)
	{
		if (at->object == object)
		{
			return true;
		}
	}
	return false;
}

static b32 number_to_source(Arena *arena, f64 number)
{
	if (!isfinite(number)) return false;

	char text[64];
	i32 size = snprintf(text, sizeof(text), "%.17g", number);
	ASSERT(size > 0 && size < (i32)sizeof(text));

	b32 has_fraction = false;
	for (i32 i = 0; i < size; ++ i) {
		if (text[i] == '.' || text[i] == 'e' || text[i] == 'E') has_fraction = true;
	}
	arena_push_data(arena, text, size);
	if (!has_fraction) arena_push_text(arena, ".0");
	return true;
}

static void string_to_source(Arena *arena, elf_String *string)
{
	arena_push_text(arena, "\"");

	const char *data = string->data;
	for (u32 i = 0; i < elf_string_size(string); ++ i)
	{
		switch (data[i])
		{
			case '\0': arena_push_text(arena, "\\0");  break;
			case '\a': arena_push_text(arena, "\\a");  break;
			case '\b': arena_push_text(arena, "\\b");  break;
			case '\f': arena_push_text(arena, "\\f");  break;
			case '\\': arena_push_text(arena, "\\\\"); break;
			case '"':  arena_push_text(arena, "\\\""); break;
			case '\n': arena_push_text(arena, "\\n");  break;
			case '\r': arena_push_text(arena, "\\r");  break;
			case '\t': arena_push_text(arena, "\\t");  break;
			case '\v': arena_push_text(arena, "\\v");  break;
			default:
			{
				u8 byte = (u8)data[i];
				if (byte < 0x20 || byte == 0x7f) arena_pushf(arena, "\\x%02x", byte);
				else arena_push_data(arena, data + i, 1);
			}
			break;
		}
	}

	arena_push_text(arena, "\"");
}

static b32 table_to_source(elf_State *state, Arena *arena, elf_Table *table, u32 indent, SourceValuePath *path)
{
	if (source_path_contains(path, (elf_Object *)table))
	{
		return false;
	}
	SourceValuePath table_path = {path, (elf_Object *)table};

	arena_push_text(arena, "{");

	b32 needs_separator = false;
	for (u32 i = 0; i < table->capacity; ++i)
	{
		Entry entry = table->entries[i];
		if (elf_table_entry_is_key(entry))
		{
			arena_push_text(arena, needs_separator ? ",\n" : "\n");
			source_indent(arena, indent + 1);
			if (!unparse_value(state, arena, entry.key, indent + 1, &table_path))
			{
				return false;
			}
			arena_push_text(arena, " = ");
			if (!unparse_value(state, arena, entry.value, indent + 1, &table_path))
			{
				return false;
			}
			needs_separator = true;
		}
	}

	if (needs_separator)
	{
		arena_push_text(arena, "\n");
		source_indent(arena, indent);
	}

	arena_push_text(arena, "}");
	return true;
}

static b32 array_to_source(elf_State *state, Arena *arena, elf_Array *array, u32 indent, SourceValuePath *path)
{
	if (source_path_contains(path, (elf_Object *)array))
	{
		return false;
	}
	SourceValuePath array_path = {path, (elf_Object *)array};

	arena_push_text(arena, "[");
	for (u32 i = 0; i < array->count; ++i)
	{
		if (i != 0)
		{
			arena_push_text(arena, ", ");
		}
		if (!unparse_value(state, arena, array->items[i], indent, &array_path))
		{
			return false;
		}
	}
	arena_push_text(arena, "]");
	return true;
}

static b32 unparse_value(elf_State *state, Arena *arena, elf_Value value, u32 indent, SourceValuePath *path)
{
	switch (value.type)
	{
		case ELF_VALUE_TYPE_NIL:
		{
			arena_push_text(arena, "nil");
		}
		break;

		case ELF_VALUE_TYPE_INTEGER:
		{
			arena_pushf(arena, "%lli", value_as_integer(value));
		}
		break;

		case ELF_VALUE_TYPE_NUMBER:
		{
			return number_to_source(arena, value.x_num);
		}

		case ELF_VALUE_TYPE_STRING:
		{
			string_to_source(arena, value_as_string(value));
		}
		break;

		case ELF_VALUE_TYPE_TABLE:
		{
			return table_to_source(state, arena, value_as_table(value), indent, path);
		}

		case ELF_VALUE_TYPE_ARRAY:
		{
			return array_to_source(state, arena, value_as_array(value), indent, path);
		}

		default:
		{
			return false;
		}
	}

	return true;
}

b32 elf_unparse_value(elf_State *state, Arena *arena, elf_Value value, u32 indent)
{
	return unparse_value(state, arena, value, indent, 0);
}
