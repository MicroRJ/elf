/*
** See Copyright Notice In elf.h
** string.c
** String
*/


elTable *elf_new_string_metatable(elState *R) {
	elTable *tab = elf_xtab(R);
	elf_register_bindings(R,tab,elf_libS_,COUNTOF(elf_libS_));
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

elString *elf_new_string(elState *R, char *contents) {
	int length = elf_text_length(contents);
	elHashId hash = elf_hash_text(contents);
	elString *string = 0;
	elTable *registry = R->M->strings;
	if (length < 64 && registry != 0) {
		elf_check_table(registry);
		elInteger slot = elf_ttrys(registry,contents,length,hash);
		elASSERT(slot != -1);
		elEntry entry = registry->entries[slot];
		if (entry.key.tag != TAG_NIL) {
			elValue target = registry->array[registry->entries[slot].i];
			string = target.x_str;
		} else {
			string = elf_new_lstring(R,length);
			elf_copy_memory(string->text,contents,length);
			string->hash = hash;

			elInteger i = ARRAY_GROW(registry->array,1);
			registry->array[i] = elSTR(string);
			registry->entries[slot].key = elSTR(string);
			registry->entries[slot].index = i;
			registry->nslots ++;
		}
	} else {
		string = elf_new_lstring(R,length);
		elf_copy_memory(string->text,contents,length);
		string->hash = hash;
	}
	return string;
}


elBool elf_string_eq(elString *x, elString *y) {
	if (x == y) return 1;
	/* assuming we use the same hash function */
	if (x->hash != y->hash) return 0;
	if (x->length != y->length) return 0;
	return elf_texteq(x->string,y->string);
}


int elf_libS_length(elState *S) {
	elf_pint(S,((elString*)elGETTHIS(S))->length);
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


int elf_libS_get_index(elState *R) {
	elString *str = (elString*) elGETTHIS(R);
	elf_pint(R,str->text[elf_get_integer(R,0)]);
	return 1;
}


int elf_libS_pop(elState *R) {
	elString *yo = (elString*) elGETTHIS(R);
	elString *el = elf_new_lstring(R,MAX(0,yo->length-1));
	elf_copy_memory(el->text,yo->text,MAX(0,yo->length-1));
	elf_pstr(R,el);
	return 1;
}


int elf_libS_append_char(elState *R) {
	elString *yo = (elString*) elGETTHIS(R);
	elString *el = elf_new_lstring(R,yo->length + elGETNARGS(R));
	elf_copy_memory(el->text,yo->text,yo->length);
	for ( int i = 0; i < elGETNARGS(R); i += 1 ) {
		el->text[yo->length + i] = elf_get_integer(R,i);
	}
	elf_pstr(R,el);
	return 1;
}


int elf_libS_append(elState *R) {
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
		} else elNOCODE;
	}
	elf_xstr(R,buffer);
	return 1;
}


int elf_libS_match(elState *R) {
	elString *s = (elString*) elGETTHIS(R);
	elString *p = elf_get_string(R,0);
	elf_pint(R,elf_match_entire_string(p->string,s->string));
	return 1;
}


int elf_libS_find(elState *R) {
	elString *string = (elString*) elGETTHIS(R);
	char *pattern = elf_get_text(R,0);
	char *buffer = 0;
	char *cursor = string->text;
	elTable *list = elf_xtab(R);
	while (*cursor) {
		char *match = elf_match_strings_single_clause_ex(cursor,pattern);
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


int elf_libS_split_by_lines(elState *R) {
	elString *str = (elString*) elGETTHIS(R);
	char *buffer = 0;
	char *cursor = str->text;
	elTable *list = elf_xtab(R);
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


int elf_libS_get_hash(elState *R) {
	elString *str = (elString*) elGETTHIS(R);
	elf_pint(R,str->hash);
	return 1;
}


int elf_libS_lowercase(elState *R) {
	elString *str = (elString*) elGETTHIS(R);
	elString *newstr = elf_xlstr(R,str->length);
	for (int i = 0; i < str->length; ++ i) {
		newstr->c[i] = elf_chrtolowercase(str->c[i]);
	}
	return 1;
}


int elf_libS_uppercase(elState *R) {
	elString *str = (elString*) elGETTHIS(R);
	elString *newstr = elf_xlstr(R,str->length);
	for (int i = 0; i < str->length; ++ i) {
		newstr->c[i] = elf_chrtouppercase(str->c[i]);
	}
	return 1;
}


