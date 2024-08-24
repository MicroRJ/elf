/*
** See Copyright Notice In elf.h
** text.c
** Text Tools
*/




elBool is_eol_chr(char x) {
	return x == '\r' || x == '\n' || x == '\0';
}


elBool is_digit_chr(char x) {
	return x >= '0' && x <= '9';
}


elBool is_lowercase_chr(char x) {
	return x >= 'a' && x <= 'z';
}


elBool is_uppercase_chr(char x) {
	return x >= 'A' && x <= 'Z';
}


elBool is_letter_chr(char x) {
	return is_uppercase_chr(x) || is_lowercase_chr(x);
}


elBool is_letter_or_digit_chr(char x) {
	return is_letter_chr(x) || is_digit_chr(x);
}


char chr_to_lowercase(char x) {
	if (is_uppercase_chr(x)) {
	 	return x - 'A' + 'a';
	}
	return x;
}


char chr_to_uppercase(char x) {
	if (is_lowercase_chr(x)) {
	 	return x - 'a' + 'A';
	}
	return x;
}


int text_length(char const *s) {
	int n = 0;
	if (s != 0) {
		while (*s ++ != 0) {
			n += 1;
		}
	}
	return n;
}


#if 0
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
#endif

elBool text_eql(char const *x, char const *y, int n) {
	for (int i = 0; i < n; i += 1) {
		if (x[i] != y[i]) {
			return 0;
		}
	}
	return 1;
}


elBool text_eq(char const *x, char const *y) {
	if (x == y) {
		return 1;
	}
	int lx = text_length(x);
	int ly = text_length(y);
	return (lx == ly) && text_eql(x,y,lx);
}


char *copy_text2(elAllocator fn, int length, char const *text) {
	if (length <= 0) {
		length = text_length(text);
	}
	char *result = elf_alloc(fn,length+1);
	elf_copy_memory(result,text,length);
	result[length]=0;
	return result;
}


char *copy_text(elAllocator fn, char const *text) {
	return copy_text2(fn,-1,text);
}


char *xpf(elAllocator fn, char const *format, va_list v) {
	int length = stbsp_vsnprintf(NULL,0,format,v);
	char *text = elf_alloc(fn,length+1);
	stbsp_vsnprintf(text,length+1,format,v);
	return text;
}


char *tpfv(char const *format, va_list v) {
	return xpf(TLS_ALLOCATOR,format,v);
}


char *tpf_(char const *format, ...) {
	va_list v;
	va_start(v,format);
	char *contents = tpfv(format,v);
	va_end(v);
	return contents;
}


/*
** Simple pattern matcher utility.
*/
elBool match_entire_text_noclause(char *p, char *s);


/* Younger me wrote:
	" todo: support for ()
	  todo: this has a flaw!! "

  Now, I don't remember what the flaw is!
*/
elBool match_entire_text(char *p, char *s) {
	char *b = s;
	while (!match_entire_text_noclause(p,s)) {
		while (*p != 0 && *p != '|') ++p;
		if (*p == 0) return 0;
		++ p, s = b;
	}
	return 1;
}


elBool match_entire_text_noclause(char *p, char *s) {
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
			if (match_entire_text_noclause(p+1,s)) {
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


char *match_text_single_clause_ex(char *p, char *s) {
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
			char *g = match_text_single_clause_ex(p+1,s);
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
