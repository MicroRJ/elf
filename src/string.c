/*
** See Copyright Notice In elf.h
** string.c
** String
*/


static int string_lib_length(elf_State *S);
static int string_lib_match(elf_State *S);
static int string_lib_pop(elf_State *S);
static int string_lib_append(elf_State *S);
static int string_lib_append_char(elf_State *S);
static int string_lib_get_hash(elf_State *S);
static int string_lib_uppercase(elf_State *S);
static int string_lib_lowercase(elf_State *S);
static int string_lib_split_by_lines(elf_State *S);
static int string_lib_get_index(elf_State *S);
static int string_lib_find(elf_State *S);
static int string_lib_split(elf_State *S);


elf_Table *elf_new_string_lib(elf_State *R) {
	elf_CBinding lib[] = {
		{"length",string_lib_length},
		{"match",string_lib_match},
		{"uppercase",string_lib_uppercase},
		{"lowercase",string_lib_lowercase},
		{"__add",string_lib_append},
		{"__add1",string_lib_append},
		{"append",string_lib_append},
		{"append_char",string_lib_append_char},
		{"pop",string_lib_pop},
		{"get_hash",string_lib_get_hash},
		{"split_by_lines",string_lib_split_by_lines},
		{"idx",string_lib_get_index},
		{"find",string_lib_find},
	};


	elf_Table *tab;

	tab=elf_new_table(R);
	elf_tsetx_bindings(R,tab,lib,COUNTOF(lib));
	return tab;
}


elf_String *elf_alloc_string2(elf_State *R, elf_Int length) {
	elf_String *obj = elf_alloc_object(R,GC_STR,sizeof(elf_String)+length+1);
	if (R) obj->obj.meta = R->metatables.string;
	obj->length = length;
	obj->hash = -1;
	obj->text[length] = 0;
	return obj;
}


elf_String *elf_alloc_string(elf_State *R, const char *text) {
	int length;
	elf_Hash hash;
	elf_String *string;
	elf_Table *registry;

	length=text_length(text);
	hash=elf_hash_text(text);
	string=0;
	registry=R->M->strings;


	if (length < 64 && registry != 0) {
		elf_check_table(registry);
		elf_Int slot=elf_ttryx(registry,text,length,hash);
		ASSERT(slot != -1);
		elf_Entry entry=registry->slots[slot];
		if (entry.key.tag != elf_TAG_NIL) {
			elf_Value target=registry->array[registry->slots[slot].idx];
			string=target.x_str;
		} else {
			string = elf_alloc_string2(R,length);
			copy_memory(string->text,text,length);
			string->hash = hash;

			elf_Int i = ARRAY_GROW(registry->array,1);
			registry->array[i]=VSTR(string);
			registry->slots[slot].key=VSTR(string);
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


elf_Bool elf_get_strings_eq(elf_String *x, elf_String *y) {
	if (x == y) return 1;
	/* assuming we use the same hash function */
	if (x->hash != y->hash) return 0;
	if (x->length != y->length) return 0;
	return text_eq(x->text,y->text);
}


int string_lib_length(elf_State *S) {
	elf_add_int(S,((elf_String*)elf_get_this(S))->length);
	return 1;
}


#define BUFFER 0x10000


/* todo: this is so unsafe is crazy */
void strcatf(char *buffer, char *fmt, ...) {
	char *cursor = buffer;
	while (*cursor != 0) ++ cursor;
	va_list v;
	va_start(v,fmt);
	stbsp_vsnprintf(cursor,BUFFER-(cursor-buffer),fmt,v);
	va_end(v);
}


int string_lib_get_index(elf_State *R) {
	elf_String *str = (elf_String*) elf_get_this(R);
	elf_add_int(R,str->text[elf_get_int(R,0)]);
	return 1;
}


int string_lib_pop(elf_State *R) {
	elf_String *yo = (elf_String*) elf_get_this(R);
	elf_String *el = elf_alloc_string2(R,MAX(0,yo->length-1));
	copy_memory(el->text,yo->text,MAX(0,yo->length-1));
	elf_add_str(R,el);
	return 1;
}


int string_lib_append_char(elf_State *R) {
	elf_String *yo = (elf_String*) elf_get_this(R);
	elf_String *el = elf_alloc_string2(R,yo->length + elf_get_num_args(R));
	copy_memory(el->text,yo->text,yo->length);
	for ( int i = 0; i < elf_get_num_args(R); i += 1 ) {
		el->text[yo->length + i] = elf_get_int(R,i);
	}
	elf_add_str(R,el);
	return 1;
}


int string_lib_append(elf_State *R) {
	elf_String *str = (elf_String*) elf_get_this(R);
	char buffer[0x100] = {0};
	strcatf(buffer,"%s",str->text);
	for (int i = 0; i < elf_get_num_args(R); ++ i) {
		elf_Value v = elf_get_arg(R,i);
		if (v.tag == elf_TAG_STR) {
			strcatf(buffer,"%s",v.x_str->text);
		} else if (v.tag == elf_TAG_NIL) {
			strcatf(buffer,"nil");
		} else if (v.tag == elf_TAG_NUM) {
			strcatf(buffer,"%.2f",v.x_num);
		} else if (v.tag == elf_TAG_INT) {
			strcatf(buffer,"%lli",v.x_int);
		} else NO_CODE;
	}
	elf_new_string(R,buffer);
	return 1;
}


int string_lib_match(elf_State *R) {
	elf_String *s = (elf_String*) elf_get_this(R);
	elf_String *p = elf_get_str(R,0);
	elf_add_int(R,match_entire_text(p->text,s->text));
	return 1;
}


int string_lib_find(elf_State *R) {
	elf_String *string = (elf_String*) elf_get_this(R);
	char *pattern = elf_get_txt(R,0);
	char *buffer = 0;
	char *cursor = string->text;
	elf_Table *list = elf_new_table(R);
	while (*cursor) {
		char *match = match_text_single_clause_ex(cursor,pattern);
		if (match != 0) {
			while (cursor < match) {
				ARRAY_ADD(buffer,*cursor ++);
			}
			/* todo: there's no need for the buffer! */
			ARRAY_ADD(buffer,0);
			elf_Int narray = ARRAY_LENGTH(list->array);
			elf_tset(list,VINT(narray),VSTR(elf_alloc_string(R,buffer)));
			ARRAY(buffer).min = 0;
		} else cursor += 1;
	}
	return 1;
}


int string_lib_split_by_lines(elf_State *R) {
	elf_String *str = (elf_String*) elf_get_this(R);
	char *buffer = 0;
	char *cursor = str->text;
	elf_Table *list = elf_new_table(R);
	while (*cursor) {
		while (*cursor && *cursor != '\n' && *cursor != '\r') {
			ARRAY_ADD(buffer,*cursor ++);
		}
		if (*cursor == '\n' || *cursor == '\r') {
			cursor += 1 + (cursor[0] == '\r' && cursor[1] == '\n');
		}
		ARRAY_ADD(buffer,0);
		elf_tset(list,VINT(ARRAY_LENGTH(list->array)),VSTR(elf_alloc_string(R,buffer)));
		ARRAY(buffer).min = 0;
	}
	return 1;
}


int string_lib_get_hash(elf_State *R) {
	elf_String *str = (elf_String*) elf_get_this(R);
	elf_add_int(R,str->hash);
	return 1;
}


int string_lib_lowercase(elf_State *R) {
	elf_String *str = (elf_String*) elf_get_this(R);
	elf_String *newstr = elf_new_string2(R,str->length);
	for (int i = 0; i < str->length; ++ i) {
		newstr->text[i] = chr_to_lowercase(str->text[i]);
	}
	return 1;
}


int string_lib_uppercase(elf_State *R) {
	elf_String *str = (elf_String*) elf_get_this(R);
	elf_String *newstr = elf_new_string2(R,str->length);
	for (int i = 0; i < str->length; ++ i) {
		newstr->text[i] = chr_to_uppercase(str->text[i]);
	}
	return 1;
}


