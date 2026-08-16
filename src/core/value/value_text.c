//
// See Copyright Notice In elf.h
//

#include "value_text.h"

static void table_to_text(elf_Arena *arena, elf_Table *table)
{
	elf_arena_push_text(arena, "{");

	for (u32 i = 0; i < elf_array_length(table); ++ i)
	{
		if (i != 0) {
			elf_arena_push_text(arena, ", ");
		}

		u32 key_count = 0;
		for (u32 j = 0; j < table->nentries; ++ j)
		{
			Entry entry = table->entries[j];
			if (entry_is_dead(entry) || entry_index(entry) != i) {
				continue;
			}

			if (key_count++ != 0) {
				elf_arena_push_text(arena, ", ");
			}
			elf_print_value(arena, entry_key_value(entry));
		}

		if (key_count != 0) {
			elf_arena_push_text(arena, " = ");
		}
		elf_print_value(arena, elf_array_get(0, table, i));
	}

	elf_arena_push_text(arena, "}");
}

void elf_print_value(elf_Arena *arena, elf_Value value)
{
	switch (value.type)
	{
		case ELF_VALUE_TYPE_NIL:       elf_arena_push_text(arena, "nil"); break;
		case ELF_VALUE_TYPE_INTEGER:   elf_arena_pushf(arena, "%lli", value_as_integer(value)); break;
		case ELF_VALUE_TYPE_NUMBER:    elf_arena_pushf(arena, "%f", value.x_num); break;
		case ELF_VALUE_TYPE_STRING:    elf_arena_push_data(arena, value.x_string->data, string_size(value.x_string)); break;
		case ELF_VALUE_TYPE_CLOSURE:   elf_arena_push_text(arena, "C()"); break;
		case ELF_VALUE_TYPE_CFUNCTION: elf_arena_push_text(arena, "F()"); break;
		case ELF_VALUE_TYPE_TABLE:     table_to_text(arena, value_as_table(value)); break;
		default:                       elf_arena_push_text(arena, "(?)"); break;
	}
}

static void source_indent(elf_Arena *arena, u32 indent)
{
	elf_arena_push_nchar(arena, '\t', indent);
}

typedef struct SourceTablePath SourceTablePath;
struct SourceTablePath
{
	SourceTablePath *parent;
	elf_Table       *table;
};

static b32 unparse_value(elf_State *state, elf_Arena *arena, elf_Value value, u32 indent, SourceTablePath *path);

static b32 number_to_source(elf_Arena *arena, f64 number)
{
	if (!isfinite(number)) return false;

	char text[64];
	i32 size = snprintf(text, sizeof(text), "%.17g", number);
	ASSERT(size > 0 && size < (i32)sizeof(text));

	b32 has_fraction = false;
	for (i32 i = 0; i < size; ++ i) {
		if (text[i] == '.' || text[i] == 'e' || text[i] == 'E') has_fraction = true;
	}
	elf_arena_push_data(arena, text, size);
	if (!has_fraction) elf_arena_push_text(arena, ".0");
	return true;
}

static void string_to_source(elf_Arena *arena, elf_String *string)
{
	elf_arena_push_char(arena, '"');

	const char *data = string->data;
	for (u32 i = 0; i < string_size(string); ++ i)
	{
		switch (data[i])
		{
			case '\0': elf_arena_push_text(arena, "\\0");  break;
			case '\a': elf_arena_push_text(arena, "\\a");  break;
			case '\b': elf_arena_push_text(arena, "\\b");  break;
			case '\f': elf_arena_push_text(arena, "\\f");  break;
			case '\\': elf_arena_push_text(arena, "\\\\"); break;
			case '"':  elf_arena_push_text(arena, "\\\""); break;
			case '\n': elf_arena_push_text(arena, "\\n");  break;
			case '\r': elf_arena_push_text(arena, "\\r");  break;
			case '\t': elf_arena_push_text(arena, "\\t");  break;
			case '\v': elf_arena_push_text(arena, "\\v");  break;
			default:
			{
				u8 byte = (u8)data[i];
				if (byte < 0x20 || byte == 0x7f) elf_arena_pushf(arena, "\\x%02x", byte);
				else elf_arena_push_char(arena, data[i]);
			}
			break;
		}
	}

	elf_arena_push_char(arena, '"');
}

static b32 table_slot_keys_to_source(elf_State *state, elf_Arena *arena, elf_Table *table, u32 slot, u32 indent, SourceTablePath *path, b32 *needs_separator, b32 *emitted_key)
{
	*emitted_key = false;
	elf_Value value = elf_array_get(state, table, slot);

	for (u32 i = 0; i < table->nentries; ++ i)
	{
		Entry entry = table->entries[i];
		if (entry_is_dead(entry) || entry_index(entry) != slot) {
			continue;
		}

		elf_Value key = entry_key_value(entry);
		elf_arena_push_text(arena, *needs_separator ? ",\n" : "\n");
		source_indent(arena, indent + 1);
		if (!unparse_value(state, arena, key, indent + 1, path)) return false;
		elf_arena_push_text(arena, " = ");
		if (!unparse_value(state, arena, value, indent + 1, path)) return false;

		*needs_separator = true;
		*emitted_key = true;
	}

	return true;
}

static b32 table_to_source(elf_State *state, elf_Arena *arena, elf_Table *table, u32 indent, SourceTablePath *path)
{
	for (SourceTablePath *at = path; at; at = at->parent) {
		if (at->table == table) return false;
	}
	SourceTablePath table_path = {path, table};

	elf_arena_push_text(arena, "{");

	b32 needs_separator = false;
	for (u32 i = 0; i < elf_array_length(table); ++ i)
	{
		elf_Value value = elf_array_get(state, table, i);
		b32 emitted_key;
		if (!table_slot_keys_to_source(state, arena, table, i, indent, &table_path, &needs_separator, &emitted_key)) return false;
		if (emitted_key) {
			continue;
		}

		elf_arena_push_text(arena, needs_separator ? ",\n" : "\n");
		source_indent(arena, indent + 1);
		if (!unparse_value(state, arena, value, indent + 1, &table_path)) return false;
		needs_separator = true;
	}

	if (needs_separator)
	{
		elf_arena_push_text(arena, "\n");
		source_indent(arena, indent);
	}

	elf_arena_push_text(arena, "}");
	return true;
}

static b32 unparse_value(elf_State *state, elf_Arena *arena, elf_Value value, u32 indent, SourceTablePath *path)
{
	switch (value.type)
	{
		case ELF_VALUE_TYPE_NIL:
		{
			elf_arena_push_text(arena, "nil");
		}
		break;

		case ELF_VALUE_TYPE_INTEGER:
		{
			elf_arena_pushf(arena, "%lli", value_as_integer(value));
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

		default:
		{
			return false;
		}
	}

	return true;
}

b32 elf_unparse_value(elf_State *state, elf_Arena *arena, elf_Value value, u32 indent)
{
	return unparse_value(state, arena, value, indent, 0);
}
