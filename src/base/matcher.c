//
// See Copyright Notice In elf.h
//

static const char *matcher_match_one(const char *text, const char *pattern);

static const char *matcher_text_end(const char *text)
{
	while (*text) {
		++text;
	}
	return text;
}

const char *matcher_match(const char *text, const char *pattern)
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
