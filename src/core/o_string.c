//
// See Copyright Notice In elf.h
//


elf_rawapi
elf_String *elf_alloc_string3(elf_State *inter, char const *text, elf_i32 length) {
	elf_String *str = elf_gc_alloc(inter, GC_STR, sizeof(elf_String) + length + 1);
	if (inter) str->obj.meta = inter->metatables.string;
	str->length = length;
	str->text[length] = 0;
	str->hash = hash_text(text);
	copy_memory(str->text, text, length);
	return str;
}


elf_rawapi
elf_String *elf_alloc_string2(elf_State *S, elf_i32 length) {
	elf_String *obj = elf_gc_alloc(S, GC_STR, sizeof(elf_String)+length+1);
	//
	// todo: instead of doing this, have a table of metatables, which maps
	// the object type to a metatable pointer, so in alloc object, the metatable
	// gets assigned.
	//
	if (S) obj->obj.meta = S->metatables.string;

	obj->length = length;
	obj->hash = -1;
	obj->text[length] = 0;
	return obj;
}

elf_String *elf_alloc_string(elf_State *R, const char *text) {
	int length = strlen(text);
	elf_String *string = elf_alloc_string2(R, length);
	elf_Hash hash = hash_text(text);
	copy_memory(string->text,text,length);
	string->hash = hash;
	return string;
}


inline elf_Hash elf_get_string_hash(elf_String *string) {
	return string->hash;
}

