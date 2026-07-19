//
// See Copyright Notice In elf.h
//

static const char *matcher_match_one(const char *text, const char *pattern);

static b32 matcher_match_range(const char *text, u32 text_size,
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
			if (matcher_match_range(text + i, text_size - i, pattern + 1, pattern_size - 1)) return true;
		}
		return false;
	}

	if (text_size == 0) return false;
	if (pattern[0] != '?' && pattern[0] != text[0]) return false;
	return matcher_match_range(text + 1, text_size - 1, pattern + 1, pattern_size - 1);
}

b32 elf_glob_match_sized(const char *text, u32 text_size, const char *pattern, u32 pattern_size)
{
	u32 start = 0;
	for (u32 i = 0; i <= pattern_size; ++i)
	{
		if (i < pattern_size && pattern[i] != '|') continue;
		if (matcher_match_range(text, text_size, pattern + start, i - start)) return true;
		start = i + 1;
	}
	return false;
}

static const char *matcher_text_end(const char *text)
{
	while (*text) {
		++text;
	}
	return text;
}

const char *elf_glob_match(const char *text, const char *pattern)
{
	do {
		const char *tail = matcher_match_one(text, pattern);
		if (tail) {
			return tail;
		}

		while (*pattern && *pattern != '|') {
			++pattern;
		}
		if (*pattern == '|') {
			++pattern;
		}
	} while (*pattern);

	return 0;
}

static const char *matcher_match_one(const char *text, const char *pattern)
{
	if (*text == 0) {
		return (*pattern == 0 || *pattern == '*' || *pattern == '|') ? text : 0;
	}

	while (*pattern && *pattern != '|')
	{
		if (*pattern == '?')
		{
			++pattern;
			++text;
		}
		else if (*pattern == '*')
		{
			while (pattern[1] == '*') {
				++pattern;
			}

			if (pattern[1] == 0 || pattern[1] == '|') {
				return matcher_text_end(text);
			}

			for (const char *cursor = text; *cursor; ++cursor)
			{
				if (pattern[1] != '?')
				{
					while (*cursor && *cursor != pattern[1]) {
						++cursor;
					}

					if (*cursor == 0) {
						return 0;
					}
				}

				const char *tail = matcher_match_one(cursor, pattern + 1);
				if (tail) {
					return tail;
				}
			}

			return 0;
		}
		else if (*pattern != *text)
		{
			return 0;
		}
		else
		{
			++pattern;
			++text;
		}
	}

	return *text ? 0 : text;
}
