//
// See Copyright Notice In elf.h
//

#include "value_text.h"

static void table_to_text(Arena *arena, elf_Table *table)
{
	arena_push_text(arena, "{");

	for (u32 i = 0; i < elf_array_len(table); ++ i)
	{
		if (i != 0) {
			arena_push_text(arena, ", ");
		}

		u32 key_count = 0;
		for (u32 j = 0; j < table->nentries; ++ j)
		{
			Entry entry = table->entries[j];
			if (entry_is_dead(entry) || entry_index(entry) != i) {
				continue;
			}

			if (key_count++ != 0) {
				arena_push_text(arena, ", ");
			}
			print_value(arena, entry_key_value(entry));
		}

		if (key_count != 0) {
			arena_push_text(arena, " = ");
		}
		print_value(arena, elf_array_get(0, table, i));
	}

	arena_push_text(arena, "}");
}

void print_value(Arena *arena, elf_Value value)
{
	switch (value.type)
	{
		case ELF_VALUE_TYPE_NIL:       arena_push_text(arena, "nil"); break;
		case ELF_VALUE_TYPE_INTEGER:   arena_pushf(arena, "%lli", value_as_integer(value)); break;
		case ELF_VALUE_TYPE_NUMBER:    arena_pushf(arena, "%f", value.x_num); break;
		case ELF_VALUE_TYPE_HANDLE:    arena_pushf(arena, "h%llX", value.x_int); break;
		case ELF_VALUE_TYPE_ATOM:      arena_push_data(arena, value.x_atom->data, value.x_atom->size); break;
		case ELF_VALUE_TYPE_CLOSURE:   arena_push_text(arena, "C()"); break;
		case ELF_VALUE_TYPE_CFUNCTION: arena_push_text(arena, "F()"); break;
		case ELF_VALUE_TYPE_TABLE:     table_to_text(arena, value_as_table(value)); break;
		default:                       arena_push_text(arena, "(?)"); break;
	}
}

static void source_indent(Arena *arena, u32 indent)
{
	arena_push_repeat(arena, '\t', indent);
}

static b32 value_can_source(elf_Value value)
{
	switch (value.type)
	{
		case ELF_VALUE_TYPE_NIL:
		case ELF_VALUE_TYPE_INTEGER:
		case ELF_VALUE_TYPE_NUMBER:
		case ELF_VALUE_TYPE_ATOM:
		case ELF_VALUE_TYPE_TABLE:
			return true;

		default:
			return false;
	}
}

static void atom_to_source(Arena *arena, elf_String *atom)
{
	arena_push_char(arena, '"');

	const char *data = atom->data;
	for (u32 i = 0; i < atom->size; ++ i)
	{
		switch (data[i])
		{
			case '\\': arena_push_text(arena, "\\\\"); break;
			case '"':  arena_push_text(arena, "\\\""); break;
			case '\n': arena_push_text(arena, "\\n");  break;
			case '\r': arena_push_text(arena, "\\r");  break;
			case '\t': arena_push_text(arena, "\\t");  break;
			default:   arena_push_char(arena, data[i]); break;
		}
	}

	arena_push_char(arena, '"');
}

static b32 table_slot_keys_to_source(elf_State *state, Arena *arena, elf_Table *table, u32 slot, u32 indent, b32 *needs_separator)
{
	b32 emitted_key = false;
	elf_Value value = elf_array_get(state, table, slot);
	if (!value_can_source(value)) {
		return false;
	}

	for (u32 i = 0; i < table->nentries; ++ i)
	{
		Entry entry = table->entries[i];
		if (entry_is_dead(entry) || entry_index(entry) != slot) {
			continue;
		}

		elf_Value key = entry_key_value(entry);
		if (!value_can_source(key)) {
			continue;
		}

		arena_push_text(arena, *needs_separator ? ",\n" : "\n");
		source_indent(arena, indent + 1);
		serialize_value(state, arena, key, indent + 1);
		arena_push_text(arena, " = ");
		serialize_value(state, arena, value, indent + 1);

		*needs_separator = true;
		emitted_key = true;
	}

	return emitted_key;
}

static b32 table_to_source(elf_State *state, Arena *arena, elf_Table *table, u32 indent)
{
	arena_push_text(arena, "{");

	b32 needs_separator = false;
	for (u32 i = 0; i < elf_array_len(table); ++ i)
	{
		elf_Value value = elf_array_get(state, table, i);
		if (!value_can_source(value)) {
			continue;
		}

		b32 emitted_key = table_slot_keys_to_source(state, arena, table, i, indent, &needs_separator);
		if (emitted_key) {
			continue;
		}

		arena_push_text(arena, needs_separator ? ",\n" : "\n");
		source_indent(arena, indent + 1);
		serialize_value(state, arena, value, indent + 1);
		needs_separator = true;
	}

	if (needs_separator)
	{
		arena_push_text(arena, "\n");
		source_indent(arena, indent);
	}

	arena_push_text(arena, "}");
	return true;
}

b32 serialize_value(elf_State *state, Arena *arena, elf_Value value, u32 indent)
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
			arena_pushf(arena, "%f", value.x_num);
		}
		break;

		case ELF_VALUE_TYPE_ATOM:
		{
			atom_to_source(arena, value_as_atom(value));
		}
		break;

		case ELF_VALUE_TYPE_TABLE:
		{
			return table_to_source(state, arena, value_as_table(value), indent);
		}

		default:
		{
			return false;
		}
	}

	return true;
}
