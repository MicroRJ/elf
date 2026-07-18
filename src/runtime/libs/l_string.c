//
// Immutable counted byte strings. Case conversion is ASCII-only.
//

static elf_String *string_receiver(elf_State *state)
{
	return lib_load_string(state, 0);
}

static void string_push(elf_State *state, const char *data, u32 size)
{
	push_value(state, value_from_atom(elf_atom_from_data_size(state, data, size)));
}

static void string_array_add(elf_State *state, elf_Table *array, const char *data, u32 size)
{
	elf_String *string = elf_atom_from_data_size(state, data, size);
	elf_array_add(state, array, value_from_atom(string));
}

static const char *string_find_data(const char *text, u32 text_size,
	const char *needle, u32 needle_size, u32 start)
{
	if (start > text_size) return 0;
	if (needle_size == 0) return text + start;
	if (needle_size > text_size - start) return 0;

	u32 last = text_size - needle_size;
	for (u32 i = start; i <= last; ++i) {
		if (text[i] == needle[0] && memcmp(text + i, needle, needle_size) == 0) {
			return text + i;
		}
	}
	return 0;
}

static b32 string_normalize_index(elf_State *state, i64 index, u32 size,
	b32 allow_end, u32 *result)
{
	if (index < 0) index += size;
	i64 upper = allow_end ? size : (i64)size - 1;
	if (index < 0 || index > upper)
	{
		report_runtime_error(state, RUNTIME_ERROR_GENERIC, NO_BYTE,
			"string index %lld is out of bounds for length %u", index, size);
		return false;
	}
	*result = (u32)index;
	return true;
}

static b32 string_is_space(u8 c)
{
	return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
}

static u8 string_to_lower(u8 c)
{
	return c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c;
}

static u8 string_to_upper(u8 c)
{
	return c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c;
}

ELF_FUNCTION(l_string_length)
{
	push_value(S, value_from_integer(atom_size(string_receiver(S))));
	return 1;
}

ELF_FUNCTION(l_string_byte)
{
	elf_String *string = string_receiver(S);
	u32 index = 0;
	if (!string_normalize_index(S, lib_load_integer(S, 1), atom_size(string), false, &index)) {
		return 0;
	}
	push_value(S, value_from_integer((u8)atom_data(string)[index]));
	return 1;
}

ELF_FUNCTION(l_string_slice)
{
	elf_String *string = string_receiver(S);
	u32 size = atom_size(string);
	u32 first = 0;
	u32 end = size;
	if (!string_normalize_index(S, lib_load_integer(S, 1), size, true, &first)) return 0;
	if (nargs > 2 && !string_normalize_index(S, lib_load_integer(S, 2), size, true, &end)) return 0;
	if (end < first)
	{
		report_runtime_error(S, RUNTIME_ERROR_GENERIC, NO_BYTE,
			"string slice end %u precedes start %u", end, first);
		return 0;
	}
	string_push(S, atom_data(string) + first, end - first);
	return 1;
}

ELF_FUNCTION(l_string_starts_with)
{
	elf_String *string = string_receiver(S);
	elf_String *prefix = lib_load_string(S, 1);
	u32 size = atom_size(string);
	u32 prefix_size = atom_size(prefix);
	b32 matches = prefix_size <= size &&
		memcmp(atom_data(string), atom_data(prefix), prefix_size) == 0;
	push_value(S, value_from_integer(matches));
	return 1;
}

ELF_FUNCTION(l_string_ends_with)
{
	elf_String *string = string_receiver(S);
	elf_String *suffix = lib_load_string(S, 1);
	u32 size = atom_size(string);
	u32 suffix_size = atom_size(suffix);
	b32 matches = suffix_size <= size && memcmp(
		atom_data(string) + size - suffix_size,
		atom_data(suffix), suffix_size) == 0;
	push_value(S, value_from_integer(matches));
	return 1;
}

ELF_FUNCTION(l_string_find)
{
	elf_String *string = string_receiver(S);
	elf_String *needle = lib_load_string(S, 1);
	u32 size = atom_size(string);
	u32 start = 0;
	if (nargs > 2 && !string_normalize_index(S, lib_load_integer(S, 2), size, true, &start)) {
		return 0;
	}
	const char *found = string_find_data(atom_data(string), size,
		atom_data(needle), atom_size(needle), start);
	if (found) push_value(S, value_from_integer(found - atom_data(string)));
	else push_value(S, value_nil());
	return 1;
}

ELF_FUNCTION(l_string_contains)
{
	elf_String *string = string_receiver(S);
	elf_String *needle = lib_load_string(S, 1);
	const char *found = string_find_data(atom_data(string), atom_size(string),
		atom_data(needle), atom_size(needle), 0);
	push_value(S, value_from_integer(found != 0));
	return 1;
}

ELF_FUNCTION(l_string_glob_match)
{
	elf_String *string = string_receiver(S);
	elf_String *pattern = lib_load_string(S, 1);
	b32 matches = matcher_match_sized(atom_data(string), atom_size(string),
		atom_data(pattern), atom_size(pattern));
	push_value(S, value_from_integer(matches));
	return 1;
}

ELF_FUNCTION(l_string_split)
{
	elf_String *string = string_receiver(S);
	elf_String *separator = lib_load_string(S, 1);
	const char *text = atom_data(string);
	const char *separator_data = atom_data(separator);
	u32 size = atom_size(string);
	u32 separator_size = atom_size(separator);
	if (separator_size == 0)
	{
		report_runtime_error(S, RUNTIME_ERROR_GENERIC, NO_BYTE,
			"string split separator cannot be empty");
		return 0;
	}

	elf_Table *parts = elf_push_new_table(S);
	u32 start = 0;
	for (;;)
	{
		const char *found = string_find_data(text, size, separator_data, separator_size, start);
		if (!found) break;
		u32 at = (u32)(found - text);
		string_array_add(S, parts, text + start, at - start);
		start = at + separator_size;
	}
	string_array_add(S, parts, text + start, size - start);
	return 1;
}

ELF_FUNCTION(l_string_lines)
{
	elf_String *string = string_receiver(S);
	const char *text = atom_data(string);
	u32 size = atom_size(string);
	elf_Table *lines = elf_push_new_table(S);
	u32 start = 0;
	u32 at = 0;

	while (at < size)
	{
		if (text[at] != '\r' && text[at] != '\n') {
			++at;
			continue;
		}
		string_array_add(S, lines, text + start, at - start);
		if (text[at] == '\r' && at + 1 < size && text[at + 1] == '\n') ++at;
		start = ++at;
	}
	if (start < size) string_array_add(S, lines, text + start, size - start);
	return 1;
}

ELF_FUNCTION(l_string_trim)
{
	elf_String *string = string_receiver(S);
	const char *text = atom_data(string);
	u32 first = 0;
	u32 end = atom_size(string);
	while (first < end && string_is_space((u8)text[first])) ++first;
	while (end > first && string_is_space((u8)text[end - 1])) --end;
	string_push(S, text + first, end - first);
	return 1;
}

ELF_FUNCTION(l_string_lower)
{
	elf_String *string = string_receiver(S);
	u32 size = atom_size(string);
	Scratch scratch = get_scratch();
	u8 *result = arena_push(scratch.arena, size);
	for (u32 i = 0; i < size; ++i) result[i] = string_to_lower((u8)atom_data(string)[i]);
	string_push(S, (char *)result, size);
	end_scratch(scratch);
	return 1;
}

ELF_FUNCTION(l_string_upper)
{
	elf_String *string = string_receiver(S);
	u32 size = atom_size(string);
	Scratch scratch = get_scratch();
	u8 *result = arena_push(scratch.arena, size);
	for (u32 i = 0; i < size; ++i) result[i] = string_to_upper((u8)atom_data(string)[i]);
	string_push(S, (char *)result, size);
	end_scratch(scratch);
	return 1;
}

ELF_FUNCTION(l_string_replace)
{
	elf_String *string = string_receiver(S);
	elf_String *old = lib_load_string(S, 1);
	elf_String *replacement = lib_load_string(S, 2);
	const char *text = atom_data(string);
	const char *old_data = atom_data(old);
	u32 size = atom_size(string);
	u32 old_size = atom_size(old);
	if (old_size == 0)
	{
		report_runtime_error(S, RUNTIME_ERROR_GENERIC, NO_BYTE,
			"string replacement target cannot be empty");
		return 0;
	}

	Scratch scratch = get_scratch();
	u32 start = 0;
	for (;;)
	{
		const char *found = string_find_data(text, size, old_data, old_size, start);
		if (!found) break;
		u32 at = (u32)(found - text);
		arena_push_data(scratch.arena, text + start, at - start);
		arena_push_data(scratch.arena, atom_data(replacement), atom_size(replacement));
		start = at + old_size;
	}
	arena_push_data(scratch.arena, text + start, size - start);
	string_push(S, (char *)scratch.arena->data + scratch.regress,
		(u32)(scratch.arena->in_use - scratch.regress));
	end_scratch(scratch);
	return 1;
}

ELF_FUNCTION(l_string_repeat)
{
	elf_String *string = string_receiver(S);
	i64 count = lib_load_integer(S, 1);
	u32 size = atom_size(string);
	if (count < 0 || (u64)count * size > UINT_MAX)
	{
		report_runtime_error(S, RUNTIME_ERROR_GENERIC, NO_BYTE,
			"invalid string repeat count %lld", count);
		return 0;
	}

	Scratch scratch = get_scratch();
	for (i64 i = 0; i < count; ++i) {
		arena_push_data(scratch.arena, atom_data(string), size);
	}
	string_push(S, (char *)scratch.arena->data + scratch.regress, (u32)(count * size));
	end_scratch(scratch);
	return 1;
}

ELF_FUNCTION(l_string_join)
{
	elf_String *separator = string_receiver(S);
	elf_Value parts_value = load_value(S, 1);
	check_value_type(S, parts_value, ELF_VALUE_TYPE_TABLE);
	elf_Table *parts = value_as_table(parts_value);
	u32 count = elf_array_len(parts);
	Scratch scratch = get_scratch();

	for (u32 i = 0; i < count; ++i)
	{
		elf_Value part_value = elf_array_get(S, parts, i);
		check_value_type(S, part_value, ELF_VALUE_TYPE_ATOM);
		elf_String *part = value_as_atom(part_value);
		if (i > 0) arena_push_data(scratch.arena,
			atom_data(separator), atom_size(separator));
		arena_push_data(scratch.arena, atom_data(part), atom_size(part));
	}

	string_push(S, (char *)scratch.arena->data + scratch.regress,
		(u32)(scratch.arena->in_use - scratch.regress));
	end_scratch(scratch);
	return 1;
}

static const elf_Binding l_string[] = {
	{"length",      l_string_length},
	{"size",        l_string_length},
	{"byte",        l_string_byte},
	{"slice",       l_string_slice},
	{"starts_with", l_string_starts_with},
	{"ends_with",   l_string_ends_with},
	{"contains",    l_string_contains},
	{"glob_match",  l_string_glob_match},
	{"find",        l_string_find},
	{"split",       l_string_split},
	{"lines",       l_string_lines},
	{"trim",        l_string_trim},
	{"lower",       l_string_lower},
	{"upper",       l_string_upper},
	{"replace",     l_string_replace},
	{"repeat",      l_string_repeat},
	{"join",        l_string_join},
};

static elf_Table *elf_lib_string(elf_State *state)
{
	return new_binding_table(state, l_string, ARRAY_COUNT(l_string));
}
