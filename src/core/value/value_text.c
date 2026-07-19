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
		case ELF_VALUE_TYPE_ATOM:      elf_arena_push_data(arena, value.x_atom->data, value.x_atom->size); break;
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

static void atom_to_source(elf_Arena *arena, elf_String *atom)
{
	elf_arena_push_char(arena, '"');

	const char *data = atom->data;
	for (u32 i = 0; i < atom->size; ++ i)
	{
		switch (data[i])
		{
			case '\\': elf_arena_push_text(arena, "\\\\"); break;
			case '"':  elf_arena_push_text(arena, "\\\""); break;
			case '\n': elf_arena_push_text(arena, "\\n");  break;
			case '\r': elf_arena_push_text(arena, "\\r");  break;
			case '\t': elf_arena_push_text(arena, "\\t");  break;
			default:   elf_arena_push_char(arena, data[i]); break;
		}
	}

	elf_arena_push_char(arena, '"');
}

static b32 table_slot_keys_to_source(elf_State *state, elf_Arena *arena, elf_Table *table, u32 slot, u32 indent, b32 *needs_separator)
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

		elf_arena_push_text(arena, *needs_separator ? ",\n" : "\n");
		source_indent(arena, indent + 1);
		elf_unparse_value(state, arena, key, indent + 1);
		elf_arena_push_text(arena, " = ");
		elf_unparse_value(state, arena, value, indent + 1);

		*needs_separator = true;
		emitted_key = true;
	}

	return emitted_key;
}

static b32 table_to_source(elf_State *state, elf_Arena *arena, elf_Table *table, u32 indent)
{
	elf_arena_push_text(arena, "{");

	b32 needs_separator = false;
	for (u32 i = 0; i < elf_array_length(table); ++ i)
	{
		elf_Value value = elf_array_get(state, table, i);
		if (!value_can_source(value)) {
			continue;
		}

		b32 emitted_key = table_slot_keys_to_source(state, arena, table, i, indent, &needs_separator);
		if (emitted_key) {
			continue;
		}

		elf_arena_push_text(arena, needs_separator ? ",\n" : "\n");
		source_indent(arena, indent + 1);
		elf_unparse_value(state, arena, value, indent + 1);
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

b32 elf_unparse_value(elf_State *state, elf_Arena *arena, elf_Value value, u32 indent)
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
			elf_arena_pushf(arena, "%f", value.x_num);
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
