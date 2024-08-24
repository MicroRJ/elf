/*
** See Copyright Notice In elf.h
** string.c
** String
*/





elGLOBAL elCBinding elf_slib_[] = {
	{"length",elf_slib_length},
	{"match",elf_slib_match},
	{"uppercase",elf_slib_uppercase},
	{"lowercase",elf_slib_lowercase},
	{"__add",elf_slib_append},
	{"__add1",elf_slib_append},
	{"append",elf_slib_append},
	{"append_char",elf_slib_append_char},
	{"pop",elf_slib_pop},
	{"get_hash",elf_slib_get_hash},
	{"split_by_lines",elf_slib_split_by_lines},
	{"idx",elf_slib_get_index},
	{"find",elf_slib_find},
};


elTable *elf_new_string_metatable(elState *R) {
	elTable *tab = elf_put_new_table(R);
	elf_tsetx_bindings(R,tab,elf_slib_,COUNTOF(elf_slib_));
	return tab;
}


elString *elf_new_lstring(elState *R, elInteger length) {
	elString *obj = elf_new_object(R,GC_STR,sizeof(elString)+length+1);
	if (R) obj->obj.metatable = R->metatables.string;
	obj->length = length;
	obj->hash = -1;
	obj->text[length] = 0;
	return obj;
}


elString *elf_new_string(elState *R, const char *text) {
	int length = text_length(text);
	elHashId hash = elf_hash_text(text);
	elString *string = 0;
	elTable *registry = R->M->strings;
	if (length < 64 && registry != 0) {
		elf_check_table(registry);
		elInteger slot = elf_ttry_text(registry,text,length,hash);
		ASSERT(slot != -1);
		elEntry entry = registry->entries[slot];
		if (entry.key.tag != TAG_NIL) {
			elValue target = registry->array[registry->entries[slot].i];
			string = target.x_str;
		} else {
			string = elf_new_lstring(R,length);
			elf_copy_memory(string->text,text,length);
			string->hash = hash;

			elInteger i = ARRAY_GROW(registry->array,1);
			registry->array[i] = elSTR(string);
			registry->entries[slot].key = elSTR(string);
			registry->entries[slot].index = i;
			registry->nslots ++;
		}
	} else {
		string = elf_new_lstring(R,length);
		elf_copy_memory(string->text,text,length);
		string->hash = hash;
	}
	return string;
}


elBool elf_string_eq(elString *x, elString *y) {
	if (x == y) return 1;
	/* assuming we use the same hash function */
	if (x->hash != y->hash) return 0;
	if (x->length != y->length) return 0;
	return text_eq(x->string,y->string);
}


int elf_slib_length(elState *S) {
	elf_put_integer(S,((elString*)elGETTHIS(S))->length);
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


int elf_slib_get_index(elState *R) {
	elString *str = (elString*) elGETTHIS(R);
	elf_put_integer(R,str->text[elf_get_integer(R,0)]);
	return 1;
}


int elf_slib_pop(elState *R) {
	elString *yo = (elString*) elGETTHIS(R);
	elString *el = elf_new_lstring(R,MAX(0,yo->length-1));
	elf_copy_memory(el->text,yo->text,MAX(0,yo->length-1));
	elf_put_string(R,el);
	return 1;
}


int elf_slib_append_char(elState *R) {
	elString *yo = (elString*) elGETTHIS(R);
	elString *el = elf_new_lstring(R,yo->length + elGETNARGS(R));
	elf_copy_memory(el->text,yo->text,yo->length);
	for ( int i = 0; i < elGETNARGS(R); i += 1 ) {
		el->text[yo->length + i] = elf_get_integer(R,i);
	}
	elf_put_string(R,el);
	return 1;
}


int elf_slib_append(elState *R) {
	elString *str = (elString*) elGETTHIS(R);
	char buffer[0x100] = {0};
	strcatf(buffer,"%s",str->text);
	for (int i = 0; i < elGETNARGS(R); ++ i) {
		elValue v = elGETARG(R,i);
		if (v.tag == TAG_STR) {
			strcatf(buffer,"%s",v.x_str->text);
		} else if (v.tag == TAG_NIL) {
			strcatf(buffer,"nil");
		} else if (v.tag == TAG_NUM) {
			strcatf(buffer,"%.2f",v.x_num);
		} else if (v.tag == TAG_INT) {
			strcatf(buffer,"%lli",v.x_int);
		} else NO_CODE;
	}
	elf_put_new_string(R,buffer);
	return 1;
}


int elf_slib_match(elState *R) {
	elString *s = (elString*) elGETTHIS(R);
	elString *p = elf_get_string(R,0);
	elf_put_integer(R,match_entire_text(p->string,s->string));
	return 1;
}


int elf_slib_find(elState *R) {
	elString *string = (elString*) elGETTHIS(R);
	char *pattern = elf_get_text(R,0);
	char *buffer = 0;
	char *cursor = string->text;
	elTable *list = elf_put_new_table(R);
	while (*cursor) {
		char *match = match_text_single_clause_ex(cursor,pattern);
		if (match != 0) {
			while (cursor < match) {
				ARRAY_ADD(buffer,*cursor ++);
			}
			/* todo: there's no need for the buffer! */
			ARRAY_ADD(buffer,0);
			elInteger narray = ARRAY_LENGTH(list->array);
			elf_tset(list,elINT(narray),elSTR(elf_new_string(R,buffer)));
			ARRAY(buffer).min = 0;
		} else cursor += 1;
	}
	return 1;
}


int elf_slib_split_by_lines(elState *R) {
	elString *str = (elString*) elGETTHIS(R);
	char *buffer = 0;
	char *cursor = str->text;
	elTable *list = elf_put_new_table(R);
	while (*cursor) {
		while (*cursor && *cursor != '\n' && *cursor != '\r') {
			ARRAY_ADD(buffer,*cursor ++);
		}
		if (*cursor == '\n' || *cursor == '\r') {
			cursor += 1 + (cursor[0] == '\r' && cursor[1] == '\n');
		}
		ARRAY_ADD(buffer,0);
		elf_tset(list,elINT(ARRAY_LENGTH(list->array)),elSTR(elf_new_string(R,buffer)));
		ARRAY(buffer).min = 0;
	}
	return 1;
}


int elf_slib_get_hash(elState *R) {
	elString *str = (elString*) elGETTHIS(R);
	elf_put_integer(R,str->hash);
	return 1;
}


int elf_slib_lowercase(elState *R) {
	elString *str = (elString*) elGETTHIS(R);
	elString *newstr = elf_put_new_string2(R,str->length);
	for (int i = 0; i < str->length; ++ i) {
		newstr->text[i] = chr_to_lowercase(str->text[i]);
	}
	return 1;
}


int elf_slib_uppercase(elState *R) {
	elString *str = (elString*) elGETTHIS(R);
	elString *newstr = elf_put_new_string2(R,str->length);
	for (int i = 0; i < str->length; ++ i) {
		newstr->text[i] = chr_to_uppercase(str->text[i]);
	}
	return 1;
}


