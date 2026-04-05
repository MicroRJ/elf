//
// See Copyright Notice In elf.h
//

// todo: remove!
GCStr new_empty_string(elf_State *state, u32 size)
{
	GCStr str = collector_alloc(state, GC_STRING, sizeof(*str) + size + 1);
	if (state) setmeta(str, state->metatables.string);

	str->length = size;
	str->hash = -1;
	str->text[size] = 0;
	return str;
}

GCStr new_string_from_data_size(elf_State *S, char const *text, int length) {
	ASSERT(text);
	GCStr str = collector_alloc(S, GC_STRING, sizeof(*str) + length + 1);
	if (S) setmeta(str, S->metatables.string);
	str->length = length;
	str->text[length] = 0;
	str->hash = hash_textl(text, length);
	elf_copy_memory(str->text, text, length);
	return str;
}




GCStr new_string_from_data(elf_State *S, const char *text) {
	ASSERT(text);
	int length = strlen(text);
	return new_string_from_data_size(S, text, length);
}




static inline int         strl(GCStr str) { return str->length; }
static inline const char *strt(GCStr str) { return str->text;   }
static inline Hash        strh(GCStr str) { return str->hash;   }



static inline bool streq(GCStr x, GCStr y) {
	if (x == y) return true;
	if (strh(x) != strh(y)) return false;
	if (strl(x) != strl(y)) return false;
	return text_eq(strt(x), strt(y));
}
