/*
** See Copyright Notice In elf.h
** elf-str.c
** String
*/


elTable *elf_new_string_metatable(elState *R) {
	elTable *tab = elf_add_new_table(R);
	elf_table_set_binding_field(R,tab,"length",elf_string_lib_length);
	elf_table_set_binding_field(R,tab,"match",elf_string_lib_match);
	elf_table_set_binding_field(R,tab,"uppercase",elf_string_lib_uppercase);
	elf_table_set_binding_field(R,tab,"lowercase",elf_string_lib_lowercase);
	elf_table_set_binding_field(R,tab,"__add",elf_string_lib_append);
	elf_table_set_binding_field(R,tab,"__add1",elf_string_lib_append);
	elf_table_set_binding_field(R,tab,"append",elf_string_lib_append);
	elf_table_set_binding_field(R,tab,"append_char",elf_string_lib_append_char);
	elf_table_set_binding_field(R,tab,"pop",elf_string_lib_pop);
	elf_table_set_binding_field(R,tab,"get_hash",elf_string_lib_get_hash);
	return tab;
}


elString *elf_new_lstring(elState *R, elInteger length) {
	elString *obj = elf_new_object(R,OBJ_STRING,sizeof(elString)+length+1);
	if (R) obj->obj.metatable = R->metatables.string;
	obj->length = length;
	obj->hash = -1;
	obj->contents[length] = 0;
	return obj;
}


elString *elf_new_string(elState *R, char *junk) {
	int length = elf_cstrlen(junk);
	elString *obj = elf_new_lstring(R,length);
	elf_memcopy(obj->c,junk,length);
	obj->hash = elf_tabhashstr((char*)junk);
	return obj;
}


elBool elf_streq(elString *x, elString *y) {
	if (x == y) return elTrue;
	/* assuming we use the same hash function */
	if (x->hash != y->hash) return false;

	if (x->length != y->length) return false;

	return S_eq(x->string,y->string);
}


int elf_string_lib_length(elState *c) {
	elf_add_integer(c,((elString*)c->f->obj)->length);
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


int elf_string_lib_pop(elState *R) {
	elString *yo = (elString*) elf_get_this(R);
	elString *el = elf_new_lstring(R,MAX(0,yo->length-1));
	elf_memcopy(el->contents,yo->contents,MAX(0,yo->length-1));
	elf_add_string(R,el);
	return 1;
}


int elf_string_lib_append_char(elState *R) {
	elString *yo = (elString*) elf_get_this(R);
	elString *el = elf_new_lstring(R,yo->length + elf_get_num_args(R));
	elf_memcopy(el->contents,yo->contents,yo->length);
	for ( int i = 0; i < elf_get_num_args(R); i += 1 ) {
		el->contents[yo->length + i] = elf_get_integer(R,i);
	}
	elf_add_string(R,el);
	return 1;
}


int elf_string_lib_append(elState *R) {
	elString *str = (elString*) elf_get_this(R);
	char buffer[0x100] = {0};
	strcatf(buffer,"%s",str->c);
	for (int i = 0; i < R->call->nx; ++ i) {
		elValue v = elf_get_value(R,i);
		if (v.tag == TAG_STR) {
			strcatf(buffer,"%s",v.x_str->c);
		} else if (v.tag == TAG_NIL) {
			strcatf(buffer,"nil");
		} else if (v.tag == TAG_NUM) {
			strcatf(buffer,"%.2f",v.x_num);
		} else if (v.tag == TAG_INT) {
			strcatf(buffer,"%lli",v.x_int);
		} else elNOCODE;
	}
	elf_add_new_string(R,buffer);
	return 1;
}


int elf_string_lib_match(elState *R) {
	elString *s = (elString*) elf_get_this(R);
	elString *p = elf_get_string(R,0);
	elf_add_integer(R,elf_match_strings(p->string,s->string));
	return 1;
}


int elf_string_lib_get_hash(elState *R) {
	elString *str = (elString*) elf_get_this(R);
	elf_add_integer(R,str->hash);
	return 1;
}


int elf_string_lib_lowercase(elState *R) {
	elString *str = (elString*) elf_get_this(R);
	elString *newstr = elf_pushnewstrlen(R,str->length);
	for (int i = 0; i < str->length; ++ i) {
		newstr->c[i] = elf_chrtolowercase(str->c[i]);
	}
	return 1;
}


int elf_string_lib_uppercase(elState *R) {
	elString *str = (elString*) elf_get_this(R);
	elString *newstr = elf_pushnewstrlen(R,str->length);
	for (int i = 0; i < str->length; ++ i) {
		newstr->c[i] = elf_chrtouppercase(str->c[i]);
	}
	return 1;
}


