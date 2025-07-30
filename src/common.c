//
// See Copyright Notice In elf.h
//

//
// Various Utility Functions
//


static elf_i64 prof_get_time() {
	return sys_get_clock_time();
}

static elf_f64 prof_time_diff_s(elf_i64 time) {
	return (sys_get_clock_time() - time) / (elf_f64) sys_get_clock_freq();
}

static elf_f64 prof_time_diff_ms(elf_i64 time) {
	return prof_time_diff_s(time) * 1000.0;
}


//
// Hash <3 stb
//

static inline unsigned int rehash(unsigned int hash) {
	return ((hash) + ((hash) >> 6) + ((hash) >> 19));
}


static inline unsigned int hash_text(const char *text) {
	unsigned int hash;
	for (hash=2166136261u; *text; hash ^= *text++, hash *= 16777619);
	return hash;
}

static inline unsigned int hash64(elf_i64 i) {
	unsigned int hash = rehash(i);
	hash += hash << 16;
	hash ^= hash << 3;
	hash += hash >> 5;
	hash ^= hash << 2;
	hash += hash >> 15;
	hash ^= hash << 10;
	return rehash(hash);
}


static char *get_name_from_file_path(const char *s) {
	char *p, *n;

	p = (char *) s;

	for (n = p; *p != 0; p += 1) {
		if (*p == '/' || *p == '\\') {
			n = p + 1;
		}
	}
	return n;
}


static void get_source_info(char *source, char *cursor, int *line_number, char **line_start) {
	//
	// get line number and starting address
	// of the line for the given address within
	// the file q
	//

	char *q, *c;
	int n;

	q=source,c=source;

	for (n=0; q < cursor; ) {
		// skip line
		while (((*q != '\r') && (*q != '\n') && (*q != '\0')) && (q < cursor)) q ++;
		// did we reach end of file?
		if (*q == '\0') break;

		if ((*q != '\n') || (c = ++ q, n ++, 1)) {
			if ((*q == '\r') && (c = ++ q, n ++, 1)) {
				if (*q == '\n') c = ++ q;

				// todo: come back to this, why do we still increment
				// on the else branch here?
			} else q ++;
		}
	}
	if (line_number) *line_number = n + 1;
	if (line_start) *line_start = c;
}

static void *clear_memory(void *target, elf_Int length) {
	memset(target,0,length);
	return target;
}


static void *copy_memory(void *dst, void const *src, elf_Int length) {
	memcpy(dst,src,length);
	return dst;
}




elf_Bool is_eol_chr(char x) {
	return x == '\r' || x == '\n' || x == '\0';
}


elf_Bool is_digit_chr(char x) {
	return x >= '0' && x <= '9';
}


elf_Bool is_lowercase_chr(char x) {
	return x >= 'a' && x <= 'z';
}


elf_Bool is_uppercase_chr(char x) {
	return x >= 'A' && x <= 'Z';
}


elf_Bool is_letter_chr(char x) {
	return is_uppercase_chr(x) || is_lowercase_chr(x);
}


elf_Bool is_letter_or_digit_chr(char x) {
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
elf_Bool elf_cstrhasprefix(char *str, char *prefix) {
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

elf_Bool text_eql(char const *x, char const *y, int n) {
	for (int i = 0; i < n; i += 1) {
		if (x[i] != y[i]) {
			return 0;
		}
	}
	return 1;
}


elf_Bool text_eq(char const *x, char const *y) {
	if (x == y) {
		return 1;
	}
	int lx = text_length(x);
	int ly = text_length(y);
	return (lx == ly) && text_eql(x,y,lx);
}

char *copy_text2(int length, char const *text) {
	if (length <= 0) {
		length = text_length(text);
	}
	char *result = malloc(length+1);
	copy_memory(result,text,length);
	result[length]=0;
	return result;
}

static char *thread_format_v(char const *format, va_list v) {
	int length = stbsp_vsnprintf(NULL, 0, format, v);
	char *text = thread_alloc(length + 1);
	stbsp_vsnprintf(text,length+1,format,v);
	return text;
}

static char *tpf_(char const *format, ...) {
	va_list v;
	va_start(v,format);
	char *contents = thread_format_v(format,v);
	va_end(v);
	return contents;
}



/*
** Simple pattern matcher utility.
*/
elf_Bool match_entire_text_noclause(char *p, char *s);


/* Younger me wrote:
	" todo: support for ()
	  todo: this has a flaw!! "

  Now, I don't remember what the flaw is!
*/
elf_Bool match_entire_text(char *p, char *s) {
	char *b = s;
	while (!match_entire_text_noclause(p,s)) {
		while (*p != 0 && *p != '|') ++p;
		if (*p == 0) return 0;
		++ p, s = b;
	}
	return 1;
}


elf_Bool match_entire_text_noclause(char *p, char *s) {
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
