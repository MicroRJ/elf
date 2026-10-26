//
// See Copyright Notice In elf.h
//

//
// Immutable counted byte strings. Case conversion is ASCII-only.
//

static elf_String *string_receiver(elf_State *state)
{
	return lib_load_string(state, 0);
}

static void string_push(elf_State *state, const char *data, u32 size)
{
	push_value(state, str_value(elf_string_from_data_size(state, data, size)));
}

static void string_array_add(elf_State *state, elf_Array *array, const char *data, u32 size)
{
	elf_String *string = elf_string_from_data_size(state, data, size);
	elf_array_add(state, array, str_value(string));
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

static b32 string_glob_match_range(const char *text, u32 text_size,
	const char *pattern, u32 pattern_size)
{
	if (pattern_size == 0) return text_size == 0;

	if (pattern[0] == '*')
	{
		while (pattern_size > 1 && pattern[1] == '*') {
			++pattern;
			--pattern_size;
		}
		if (pattern_size == 1) return true;
		for (u32 i = 0; i <= text_size; ++i) {
			if (string_glob_match_range(text + i, text_size - i,
				pattern + 1, pattern_size - 1)) return true;
		}
		return false;
	}

	if (text_size == 0) return false;
	if (pattern[0] != '?' && pattern[0] != text[0]) return false;
	return string_glob_match_range(text + 1, text_size - 1,
		pattern + 1, pattern_size - 1);
}

static b32 string_glob_match_data(const char *text, u32 text_size,
	const char *pattern, u32 pattern_size)
{
	u32 start = 0;
	for (u32 i = 0; i <= pattern_size; ++i)
	{
		if (i < pattern_size && pattern[i] != '|') continue;
		if (string_glob_match_range(text, text_size,
			pattern + start, i - start)) return true;
		start = i + 1;
	}
	return false;
}

static b32 string_normalize_index(elf_State *state, i64 index, u32 size,
	b32 allow_end, u32 *result)
{
	if (index < 0) index += size;
	i64 upper = allow_end ? size : (i64)size - 1;
	if (index < 0 || index > upper)
	{
		report_runtime_error(state, ERROR_OUT_OF_RANGE,
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
	push_value(S, int_value(elf_string_size(string_receiver(S))));
	return 1;
}

ELF_FUNCTION(l_string_byte)
{
	if (nargs < 2)
	{
		elf_push_error(S, "string.byte requires an index");
		return 1;
	}

	elf_StrSlice source = {0};
	if (!elf_to_str(S, 0, &source))
	{
		elf_push_error(S, "string.byte requires a string receiver");
		return 1;
	}

	elf_Int index = 0;
	if (!elf_to_int(S, 1, &index))
	{
		elf_push_error(S, "string.byte index must be an integer");
		return 1;
	}

	if (index < 0)
	{
		index += (elf_Int)source.size;
	}

	if (index < 0 || (elf_Size)index >= source.size)
	{
		elf_push_error(S, "string.byte index is out of bounds");
		return 1;
	}

	elf_push_int(S, (u8)source.data[index]);
	return 1;
}

ELF_FUNCTION(l_string_slice)
{
	if (nargs < 2)
	{
		elf_push_error(S, "string.slice requires a start index");
		return 1;
	}

	elf_StrSlice source = {0};
	if (!elf_to_str(S, 0, &source))
	{
		elf_push_error(S, "string.slice requires a string receiver");
		return 1;
	}

	elf_Int bounds[2] = {0, (elf_Int)source.size};
	for (int argument = 1; argument < nargs && argument < 3; ++argument)
	{
		elf_Int index = 0;
		if (!elf_to_int(S, argument, &index))
		{
			elf_push_error(S, "string.slice index must be an integer");
			return 1;
		}

		if (index < 0)
		{
			index += (elf_Int)source.size;
		}

		if (index < 0 || (elf_Size)index > source.size)
		{
			elf_push_error(S, "string.slice index is out of bounds");
			return 1;
		}

		bounds[argument - 1] = index;
	}

	elf_Int first = bounds[0];
	elf_Int end = bounds[1];
	if (end < first)
	{
		elf_push_error(S, "string.slice end precedes start");
		return 1;
	}

	elf_push_str(S, source.data + first, (elf_Size)(end - first));
	return 1;
}

ELF_FUNCTION(l_string_splice)
{
	lib_check_arg_count(S, "string.splice", nargs, 3, 3);
	elf_String *string = string_receiver(S);
	elf_String *replacement = lib_load_string(S, 3);
	u32 size = elf_string_size(string);
	u32 position = 0;
	if (!string_normalize_index(S, lib_load_integer(S, 1), size, true, &position)) {
		return 0;
	}
	i64 requested = lib_load_integer(S, 2);
	if (requested < 0 || (u64)requested > size - position)
	{
		report_runtime_error(S, ERROR_OUT_OF_RANGE, "string splice delete count %lld is out of bounds", requested);
		return 0;
	}
	u32 delete_size = (u32)requested;
	u32 replacement_size = elf_string_size(replacement);
	u32 size_after_delete = size - delete_size;
	if (replacement_size > ELF_STRING_MAX_SIZE - size_after_delete)
	{
		report_runtime_error(S, ERROR_OUT_OF_RANGE, "string size exceeds the supported range");
		return 0;
	}

	Scratch scratch = scratch_begin();
	arena_push_data(scratch.arena, elf_string_data(string), position);
	arena_push_data(scratch.arena, elf_string_data(replacement), replacement_size);
	u32 suffix = position + delete_size;
	arena_push_data(scratch.arena, elf_string_data(string) + suffix, size - suffix);
	u32 result_size = size_after_delete + replacement_size;
	string_push(S, (char *)scratch.arena->data + scratch.restore_used, result_size);
	scratch_end(scratch);
	return 1;
}

ELF_FUNCTION(l_string_compare)
{
	elf_String *left = string_receiver(S);
	elf_String *right = lib_load_string(S, 1);
	u32 left_size = elf_string_size(left);
	u32 right_size = elf_string_size(right);
	u32 size = MIN(left_size, right_size);
	int order = memcmp(elf_string_data(left), elf_string_data(right), size);
	if (order == 0) order = (left_size > right_size) - (left_size < right_size);
	push_value(S, int_value((order > 0) - (order < 0)));
	return 1;
}

ELF_FUNCTION(l_string_starts_with)
{
	elf_String *string = string_receiver(S);
	elf_String *prefix = lib_load_string(S, 1);
	u32 size = elf_string_size(string);
	u32 prefix_size = elf_string_size(prefix);
	b32 matches = prefix_size <= size &&
		memcmp(elf_string_data(string), elf_string_data(prefix), prefix_size) == 0;
	push_value(S, int_value(matches));
	return 1;
}

ELF_FUNCTION(l_string_ends_with)
{
	elf_String *string = string_receiver(S);
	elf_String *suffix = lib_load_string(S, 1);
	u32 size = elf_string_size(string);
	u32 suffix_size = elf_string_size(suffix);
	b32 matches = suffix_size <= size && memcmp(
		elf_string_data(string) + size - suffix_size,
		elf_string_data(suffix), suffix_size) == 0;
	push_value(S, int_value(matches));
	return 1;
}

ELF_FUNCTION(l_string_find)
{
	if (nargs < 2)
	{
		elf_push_error(S, "string.find requires a search string");
		return 1;
	}

	elf_StrSlice source = {0};
	if (!elf_to_str(S, 0, &source))
	{
		elf_push_error(S, "string.find requires a string receiver");
		return 1;
	}

	elf_StrSlice needle = {0};
	if (!elf_to_str(S, 1, &needle))
	{
		elf_push_error(S, "string.find search argument must be a string");
		return 1;
	}

	elf_Int start = 0;
	if (nargs > 2 && !elf_to_int(S, 2, &start))
	{
		elf_push_error(S, "string.find start index must be an integer");
		return 1;
	}

	if (start < 0)
	{
		start += (elf_Int)source.size;
	}

	if (start < 0 || (elf_Size)start > source.size)
	{
		elf_push_error(S, "string.find start index is out of bounds");
		return 1;
	}

	const char *found = string_find_data(source.data, (u32)source.size, needle.data, (u32)needle.size, (u32)start);
	if (found)
	{
		elf_push_int(S, found - source.data);
	}
	else
	{
		elf_push_nil(S);
	}
	return 1;
}

ELF_FUNCTION(l_string_contains)
{
	elf_String *string = string_receiver(S);
	elf_String *needle = lib_load_string(S, 1);
	const char *found = string_find_data(elf_string_data(string), elf_string_size(string),
		elf_string_data(needle), elf_string_size(needle), 0);
	push_value(S, int_value(found != 0));
	return 1;
}

ELF_FUNCTION(l_string_glob_match)
{
	elf_String *string = string_receiver(S);
	elf_String *pattern = lib_load_string(S, 1);
	b32 matches = string_glob_match_data(elf_string_data(string), elf_string_size(string),
		elf_string_data(pattern), elf_string_size(pattern));
	push_value(S, int_value(matches));
	return 1;
}

ELF_FUNCTION(l_string_split)
{
	if (nargs < 2)
	{
		elf_push_error(S, "string.split requires a separator");
		return 1;
	}

	elf_StrSlice source = {0};
	if (!elf_to_str(S, 0, &source))
	{
		elf_push_error(S, "string.split requires a string receiver");
		return 1;
	}

	elf_StrSlice separator = {0};
	if (!elf_to_str(S, 1, &separator))
	{
		elf_push_error(S, "string.split separator must be a string");
		return 1;
	}

	if (separator.size == 0)
	{
		elf_push_error(S, "string.split separator cannot be empty");
		return 1;
	}

	elf_Array *parts = push_new_array(S);
	u32 start = 0;
	for (;;)
	{
		const char *found = string_find_data(source.data, (u32)source.size, separator.data, (u32)separator.size, start);
		if (!found)
		{
			break;
		}
		u32 at = (u32)(found - source.data);
		string_array_add(S, parts, source.data + start, at - start);
		start = at + (u32)separator.size;
	}
	string_array_add(S, parts, source.data + start, (u32)source.size - start);
	return 1;
}

ELF_FUNCTION(l_string_lines)
{
	elf_String *string = string_receiver(S);
	const char *text = elf_string_data(string);
	u32 size = elf_string_size(string);
	elf_Array *lines = push_new_array(S);
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
	const char *text = elf_string_data(string);
	u32 first = 0;
	u32 end = elf_string_size(string);
	while (first < end && string_is_space((u8)text[first])) ++first;
	while (end > first && string_is_space((u8)text[end - 1])) --end;
	string_push(S, text + first, end - first);
	return 1;
}

ELF_FUNCTION(l_string_lower)
{
	elf_String *string = string_receiver(S);
	u32 size = elf_string_size(string);
	Scratch scratch = scratch_begin();
	u8 *result = arena_push(scratch.arena, size, 1);
	for (u32 i = 0; i < size; ++i) result[i] = string_to_lower((u8)elf_string_data(string)[i]);
	string_push(S, (char *)result, size);
	scratch_end(scratch);
	return 1;
}

ELF_FUNCTION(l_string_upper)
{
	elf_String *string = string_receiver(S);
	u32 size = elf_string_size(string);
	Scratch scratch = scratch_begin();
	u8 *result = arena_push(scratch.arena, size, 1);
	for (u32 i = 0; i < size; ++i) result[i] = string_to_upper((u8)elf_string_data(string)[i]);
	string_push(S, (char *)result, size);
	scratch_end(scratch);
	return 1;
}

ELF_FUNCTION(l_string_replace)
{
	elf_String *string = string_receiver(S);
	elf_String *old = lib_load_string(S, 1);
	elf_String *replacement = lib_load_string(S, 2);
	const char *text = elf_string_data(string);
	const char *old_data = elf_string_data(old);
	u32 size = elf_string_size(string);
	u32 old_size = elf_string_size(old);
	if (old_size == 0)
	{
		report_runtime_error(S, ERROR_INVALID_ARGUMENT,
			"string replacement target cannot be empty");
		return 0;
	}

	Scratch scratch = scratch_begin();
	u32 start = 0;
	for (;;)
	{
		const char *found = string_find_data(text, size, old_data, old_size, start);
		if (!found) break;
		u32 at = (u32)(found - text);
		arena_push_data(scratch.arena, text + start, at - start);
		arena_push_data(scratch.arena, elf_string_data(replacement), elf_string_size(replacement));
		start = at + old_size;
	}
	arena_push_data(scratch.arena, text + start, size - start);
	string_push(S, (char *)scratch.arena->data + scratch.restore_used,
		(u32)(scratch.arena->used - scratch.restore_used));
	scratch_end(scratch);
	return 1;
}

ELF_FUNCTION(l_string_repeat)
{
	elf_String *string = string_receiver(S);
	i64 count = lib_load_integer(S, 1);
	u32 size = elf_string_size(string);
	if (count < 0 || (u64)count * size > UINT_MAX)
	{
		report_runtime_error(S, ERROR_INVALID_ARGUMENT,
			"invalid string repeat count %lld", count);
		return 0;
	}

	Scratch scratch = scratch_begin();
	for (i64 i = 0; i < count; ++i) {
		arena_push_data(scratch.arena, elf_string_data(string), size);
	}
	string_push(S, (char *)scratch.arena->data + scratch.restore_used, (u32)(count * size));
	scratch_end(scratch);
	return 1;
}

ELF_FUNCTION(l_string_join)
{
	elf_String *separator = string_receiver(S);
	elf_Value parts_value = load_value(S, 1);
	check_value_type(S, parts_value, ELF_VALUE_TYPE_ARRAY);
	elf_Array *parts = value_as_array(parts_value);
	u32 count = elf_array_length(parts);
	Scratch scratch = scratch_begin();

	for (u32 i = 0; i < count; ++i)
	{
		elf_Value part_value = elf_array_get(S, parts, i);
		check_value_type(S, part_value, ELF_VALUE_TYPE_STRING);
		elf_String *part = value_as_string(part_value);
		if (i > 0) arena_push_data(scratch.arena,
			elf_string_data(separator), elf_string_size(separator));
		arena_push_data(scratch.arena, elf_string_data(part), elf_string_size(part));
	}

	string_push(S, (char *)scratch.arena->data + scratch.restore_used,
		(u32)(scratch.arena->used - scratch.restore_used));
	scratch_end(scratch);
	return 1;
}

static const elf_Binding l_string[] = {
	{"length",      l_string_length},
	{"size",        l_string_length},
	{"byte",        l_string_byte},
	{"slice",       l_string_slice},
	{"splice",      l_string_splice},
	{"compare",     l_string_compare},
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
