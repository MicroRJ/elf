//
// See Copyright Notice In elf.h
//

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
	int length;
	elf_HashInt hash;
	elf_String *string;
	elf_Table *registry;

	length=text_length(text);
	hash=hash_text(text);
	string=0;
	registry=R->strings;


	if (length < 64 && registry != 0) {
		elf_table_resize_maybe(registry);
		elf_Int slot=elf_table_try_text(registry,text,length,hash);
		ASSERT(slot != -1);
		elf_Table_Entry entry=registry->slots[slot];
		if (entry.key.tag != elf_tag_nil) {
			elf_Value target=registry->array[registry->slots[slot].idx];
			string=target.x_str;
		} else {
			string = elf_alloc_string2(R,length);
			copy_memory(string->text,text,length);
			string->hash = hash;

			elf_Int i = ARRAY_GROW(registry->array,1);
			registry->array[i]=VALUE_STRING(string);
			registry->slots[slot].key=VALUE_STRING(string);
			registry->slots[slot].idx=i;
			registry->nslots ++;
		}
	} else {
		string = elf_alloc_string2(R,length);
		copy_memory(string->text,text,length);
		string->hash = hash;
	}
	return string;
}


elf_String *elf_new_string(elf_State *S, const char *text) {
	elf_String *string = elf_alloc_string(S, text);
	elf_push_string(S, string);
	return string;
}


elf_String *elf_new_string2(elf_State *R, elf_i32 length) {
	elf_String *string = elf_alloc_string2(R,length);
	elf_push_string(R,string);
	return string;
}

elf_Value elf_string(elf_State *R, char *text) {
	elf_String *string = elf_new_string(R, text);
	return VALUE_STRING(string);
}

inline elf_HashInt elf_get_string_hash(elf_String *string) {
	return string->hash;
}
