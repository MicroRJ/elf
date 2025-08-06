//
// See Copyright Notice In elf.h
//
// Simple pattern matcher utility.
//

static char *string_match_single(char *s, char *p);


static char *string_match(char *s, char *p) {
	do {
		char *b = string_match_single(s, p);
		if (b) return b;

		// go to next alternate
		while (*p && *p != '|') ++p;
		if (*p == '|') ++ p;

	} while (*p);

	// no match
	return 0;
}

//
//
// if matched,
// 	the return value is the new string pointer
// otherwise, the return value is 0
//
//
// '?' matches any one character
// '*' matches zero or more characters
//
//
static char *string_match_single(char *s, char *p)
{
	if (*s == 0)  {
		return (*p == 0 || *p == '*') ? s : 0;
	}

	while (*p != 0 && *p != '|' && *p != ')')
	{
		ASSERT(*s != 0);
		if (*p == '?') {
			++ p, ++ s;
		}
		else if (*p == '*') {

			// skip any redundant stars
			while (p[1] == '*') ++ p;


			// if the pattern ends in '*', we'll match anything
			if (p[1] == 0 || p[1] == '|') {
				return s;
			}

			ASSERT(*s != 0);

			for (char *b = s; *b; b ++) {


				// try to skip quickly to the first literal match
				if (p[1] != '?')
				{
					while (*b != 0 && *b != p[1]) b ++;

					if (*b == 0) {
						return 0;
					}
				}

				char *x = string_match_single(b, p + 1);
				if (x) {
					return x;
				}

				ASSERT(*s != 0);
			}

			// no match
			return 0;
		}
		// match literal
		else if (*p != *s)
		{
			return 0;
		}
		else {
			++ p, ++ s;
		}
	}

	return *s != 0 ? 0 : s;
}
