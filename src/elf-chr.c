/*
** See Copyright Notice In elf.h
** elf-chr.c
** String Tools
*/




elBool elf_chriseol(char x) {
	return x == '\r' || x == '\n' || x == '\0';
}


elBool elf_chrisdigit(char x) {
	return x >= '0' && x <= '9';
}


elBool elf_chrislowercase(char x) {
	return x >= 'a' && x <= 'z';
}


elBool elf_chrisuppercase(char x) {
	return x >= 'A' && x <= 'Z';
}


elBool elf_chrisletter(char x) {
	return elf_chrisuppercase(x) || elf_chrislowercase(x);
}


elBool elf_chrisalphanum(char x) {
	return elf_chrisletter(x) || elf_chrisdigit(x) || (x) == '_';
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


int elf_cstrlen(char const *s) {
	int n = 0;
	if (s != elNIL) {
		while (*s ++ != 0) {
			n += 1;
		}
	}
	return n;
}



elBool elf_cstrhasprefix(char *str, char *prefix) {
	if (prefix == 0 || *prefix == 0 || *str == 0 || *str == 0) {
		return lfalse;
	}
	do {
		if (*prefix ++ != *str ++) {
			return lfalse;
		}
	} while (*prefix);
	return ltrue;
}


elBool S_eql(char const *x, char const *y, int n) {
	for (int i = 0; i < n; i += 1) {
		if (x[i] != y[i]) {
			return lfalse;
		}
	}
	return ltrue;
}


elBool S_eq(char const *x, char const *y) {
	int lx = elf_cstrlen(x);
	int ly = elf_cstrlen(y);
	return (lx == ly) && S_eql(x,y,lx);
}


char *S_ncopy(Alloc *allocator, int length, char const *string) {
	if (length <= 0) {
		length = elf_cstrlen(string);
	}
	char *result = elf_alloc(allocator,length+1);
	elf_memcopy(result,string,length);
	result[length]=0;
	return result;
}


char *S_copy(Alloc *allocator, char const *string) {

	return S_ncopy(allocator,-1,string);
}


char *S_pfv(Alloc *cator, char const *format, va_list v) {
	int length = stbsp_vsnprintf(NULL,0,format,v);
	char *contents = elf_alloc(cator,length+1);
	stbsp_vsnprintf(contents,length+1,format,v);
	return contents;
}


char *S_tpfv(char const *format, va_list v) {
	return S_pfv(lTLOC,format,v);
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
** Pattern, elString
*/
elBool elf_cstrmatchsingle(char *p, char *s);


/* todo: support for ()
todo: this has a flaw!! */
elBool elf_cstrmatch(char *p, char *s) {
	char *b = s;
	while (!elf_cstrmatchsingle(p,s)) {
		while (*p != 0 && *p != '|') ++p;
		if (*p == 0) return lfalse;
		++ p, s = b;
	}
	return ltrue;
}


elBool elf_cstrmatchsingle(char *p, char *s) {
	while (*p != 0 && *p != '|') {
		if (*p == '?') {
			/* matches any character except terminator. */
			if (*s != 0) return lfalse;
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
			if (elf_cstrmatchsingle(p+1,s)) {
				return ltrue;
			}
			/* no match, move on to next char, remain in
			this branch and keep checking for matches. */
			++ s;
		} else
		/* otherwise, match literal fail if no match. */
		if (*p != *s) {
			return lfalse;
		} else {
			++ p, ++ s;
		}
	}
	/* did we match the whole string */
	return *s == 0;
}
