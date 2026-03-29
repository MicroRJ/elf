//
// See Copyright Notice In elf.h
//




// todo: remove!
Str _new_empty_str(elf_State *S, int length) {
	Str str = gcalloc(S, GC_STR, sizeof(*str)+length+1);
	if (S) setmeta(str, S->metatables.string);

	str->length = length;
	str->hash = -1;
	str->text[length] = 0;
	return str;
}




Str _string_newl(elf_State *S, char const *text, int length) {
	ASSERT(text);
	Str str = gcalloc(S, GC_STR, sizeof(*str) + length + 1);
	if (S) setmeta(str, S->metatables.string);
	str->length = length;
	str->text[length] = 0;
	str->hash = hash_textl(text, length);
	elf_copy_memory(str->text, text, length);
	return str;
}




Str _string_new(elf_State *S, const char *text) {
	ASSERT(text);
	int length = strlen(text);
	return _string_newl(S, text, length);
}




static inline int         strl(Str str) { return str->length; }
static inline const char *strt(Str str) { return str->text;   }
static inline Hash        strh(Str str) { return str->hash;   }



static inline bool streq(Str x, Str y) {
	if (x == y) return true;
	if (strh(x) != strh(y)) return false;
	if (strl(x) != strl(y)) return false;
	return text_eq(strt(x), strt(y));
}
