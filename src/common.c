//
// See Copyright Notice In elf.h
//

//
// Various Utility Functions
//


static inline void *zero_memory(void *mem, int len) {
	memset(mem, 0, len);
	return mem;
}


static inline void *copy_memory(void *dst, void const *src, int len) {
	memcpy(dst, src, len);
	return dst;
}


static inline void *copy_text(void *buf, int zbuf, void const *src) {
	strcpy_s(buf, zbuf, src);
	return buf;
}










static elf_i64 prof_get_time() {
	return sys_get_performance_counter();
}

static elf_f64 prof_time_diff_s(elf_i64 time) {
	return (sys_get_performance_counter() - time) / (elf_f64) sys_get_performance_counter_frequency();
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


static int is_file_name_empty(char const *name) {
	while (*name == '.' || *name == '\\' || *name == '/') ++ name;
	return *name == 0;
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



bool is_eol_chr(char x) {
	return x == '\r' || x == '\n' || x == '\0';
}


bool is_digit_chr(char x) {
	return x >= '0' && x <= '9';
}


bool is_lowercase_chr(char x) {
	return x >= 'a' && x <= 'z';
}


bool is_uppercase_chr(char x) {
	return x >= 'A' && x <= 'Z';
}


bool is_letter_chr(char x) {
	return is_uppercase_chr(x) || is_lowercase_chr(x);
}


bool is_letter_or_digit_chr(char x) {
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


int text_l(char const *s) {
	int n = 0;
	if (s != 0) {
		while (*s ++ != 0) {
			n += 1;
		}
	}
	return n;
}

static int find_subtext(char *s, char *p) {
	for (int i = 0; *s; s ++, i ++) {

		char *pc, *sc;
		for (sc = s, pc = p; *pc; sc ++, pc ++) {
			if (*sc != *pc) goto retry;
		}

		// got here, no match, if subtext longer, return
		if (*sc == 0 && *pc != 0) goto esc;

		return i;
		retry: ;
	}

	esc:
	return -1;
}


#if 0
bool elf_cstrhasprefix(char *str, char *prefix) {
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

bool text_eql(char const *x, char const *y, int n) {
	for (int i = 0; i < n; i += 1) {
		if (x[i] != y[i]) {
			return 0;
		}
	}
	return 1;
}



static bool text_eq(char const *x, char const *y) {
	if (x == y) {
		return 1;
	}

	int lx = text_l(x);
	int ly = text_l(y);

	return (lx == ly) && text_eql(x,y,lx);
}



// todo: deprecated!
char *copy_text2(int length, char const *text) {
	if (length <= 0) {
		length = text_l(text);
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

static int sb_sprintf(String_Builder *sb, char *format, ...) {
	va_list vargs;
	va_start(vargs, format);
	int size = stbsp_vsnprintf(NULL, 0, format, vargs);
	char *text = sb_alloc(sb, size + 1, size);
	stbsp_vsnprintf(text, size + 1, format, vargs);
	va_end(vargs);
	return size;
}
