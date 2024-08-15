/*
** See Copyright Notice In elf.h
** elf-chr.c
** String Tools
*/




elBool elf_chriseol(char x) {
	return x == '\r' || x == '\n' || x == '\0';
}


elBool elf_is_digit_char(char x) {
	return x >= '0' && x <= '9';
}


elBool elf_chrislowercase(char x) {
	return x >= 'a' && x <= 'z';
}


elBool elf_chrisuppercase(char x) {
	return x >= 'A' && x <= 'Z';
}


elBool elf_is_letter_char(char x) {
	return elf_chrisuppercase(x) || elf_chrislowercase(x);
}


elBool elf_is_letter_or_digit_char(char x) {
	return elf_is_letter_char(x) || elf_is_digit_char(x);
}


char elf_chrtolowercase(char x) {
	if (elf_chrisuppercase(x)) {
	 	return x - 'A' + 'a';
	}
	return x;
}


char elf_chrtouppercase(char x) {
	if (elf_chrislowercase(x)) {
	 	return x - 'a' + 'A';
	}
	return x;
}


int elf_textlength(char const *s) {
	int n = 0;
	if (s != 0) {
		while (*s ++ != 0) {
			n += 1;
		}
	}
	return n;
}



elBool elf_cstrhasprefix(char *str, char *prefix) {
	if (prefix == 0 || *prefix == 0 || *str == 0 || *str == 0) {
		return 0;
	}
	do {
		if (*prefix ++ != *str ++) {
			return 0;
		}
	} while (*prefix);
	return 1;
}


elBool elf_texteql(char const *x, char const *y, int n) {
	for (int i = 0; i < n; i += 1) {
		if (x[i] != y[i]) {
			return 0;
		}
	}
	return 1;
}


elBool elf_texteq(char const *x, char const *y) {
	if (x == y) {
		return 1;
	}
	int lx = elf_textlength(x);
	int ly = elf_textlength(y);
	return (lx == ly) && elf_texteql(x,y,lx);
}


char *elf_copyltext(elAllocator *allocator, int length, char const *string) {
	if (length <= 0) {
		length = elf_textlength(string);
	}
	char *result = elf_alloc(allocator,length+1);
	elf_copy_memory(result,string,length);
	result[length]=0;
	return result;
}


char *elf_copytext(elAllocator *allocator, char const *string) {

	return elf_copyltext(allocator,-1,string);
}


char *S_pfv(elAllocator *cator, char const *format, va_list v) {
	int length = stbsp_vsnprintf(NULL,0,format,v);
	char *contents = elf_alloc(cator,length+1);
	stbsp_vsnprintf(contents,length+1,format,v);
	return contents;
}


char *S_tpfv(char const *format, va_list v) {
	return S_pfv(elTLS_ALLOCATOR,format,v);
}


char *S_tpf_(char const *format, ...) {
	va_list v;
	va_start(v,format);
	char *contents = S_tpfv(format,v);
	va_end(v);
	return contents;
}


/*
** Simple pattern matcher utility.
*/
elBool elf_match_entire_string_noclause(char *p, char *s);


/* Younger me wrote:
	" todo: support for ()
	  todo: this has a flaw!! "

  Now, I don't remember what the flaw is!
*/
elBool elf_match_entire_string(char *p, char *s) {
	char *b = s;
	while (!elf_match_entire_string_noclause(p,s)) {
		while (*p != 0 && *p != '|') ++p;
		if (*p == 0) return 0;
		++ p, s = b;
	}
	return 1;
}


elBool elf_match_entire_string_noclause(char *p, char *s) {
	while (*p != 0 && *p != '|') {
		if (*p == '?') {
			/* matches any character except terminator. */
			if (*s != 0) {
				return 0;
			}
			++ p, ++ s;
		} else
		if (*p == '*') {
			/* unlikely the user will do this. */
			while (p[1] == '*') ++ p;

			/* got to end of string, do we still
			have a pattern? If so then no match. */
			if (*s == 0) return p[1] == 0 || p[1] == '|';

			/* '*' operator causes matcher to split branches,
			we can either match the next pattern after '*' or
			delay the match by skipping this char and remaining
			in this pattern char. */
			if (elf_match_entire_string_noclause(p+1,s)) {
				return 1;
			}
			/* no match, move on to next char, remain in
			this branch and keep checking for matches. */
			++ s;
		/* otherwise, match literal, fail if no match. */
		} else if (*p != *s) {
			return 0;
		} else {
			++ p, ++ s;
		}
	}
	/* did we match the whole string */
	return *s == 0;
}


char *elf_match_strings_single_clause_ex(char *p, char *s) {
	while (*p != 0 && *p != '|') {
		if (*p == '?') {
			/* matches any character except terminator. */
			if (*s != 0) {
				return 0;
			}
			++ p, ++ s;
		} else
		if (*p == '*') {
			/* unlikely the user will do this. */
			while (p[1] == '*') ++ p;

			/* got to end of string, do we still
			have a pattern? If so then no match. */
			if (*s == 0) {
				/* todo: if we remove the match multiple clauses
				version we can't simply return here without first
				scanning the entire string for '|' */
				if (p[1] == 0 || p[1] == '|') {
					return s;
				} else {
					return 0;
				}
			}

			/* '*' operator causes matcher to split branches,
			we can either match the next pattern after '*' or
			delay the match by skipping this char and remaining
			in this pattern char. */
			char *g = elf_match_strings_single_clause_ex(p+1,s);
			if (g) return g;
			/* no match, move on to next char, remain in
			this branch and keep checking for matches. */
			++ s;
		/* otherwise, match literal, fail if no match. */
		} else if (*p != *s) {
			return 0;
		} else {
			++ p, ++ s;
		}
	}
	/* did we match the whole string */
	return s;
}
