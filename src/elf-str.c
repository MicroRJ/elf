/*
** See Copyright Notice In elf.h
** elf-str.c
** String
*/


elTable *elf_new_string_metatable(elState *R) {
	elTable *tab = elf_add_new_table(R);
	elf_table_set_binding_field(R,tab,"length",elfstr_length_);
	elf_table_set_binding_field(R,tab,"match",elfstr_match_);
	elf_table_set_binding_field(R,tab,"gethash",elfstr_gethash_);
	elf_table_set_binding_field(R,tab,"append",elfstr_append_);
	elf_table_set_binding_field(R,tab,"touppercase",elfstr_touppercase_);
	elf_table_set_binding_field(R,tab,"tolowercase",elfstr_tolowercase_);
	elf_table_set_binding_field(R,tab,"__add",elfstr_append_);
	elf_table_set_binding_field(R,tab,"__add1",elfstr_append_);
	return tab;
}


elString *elf_new_string_of_length(elState *R, elInteger length) {
	elString *obj = elf_new_object(R,OBJ_STRING,sizeof(elString)+length+1);
	if (R) obj->obj.metatable = R->metatables.string;
	obj->length = length;
	obj->hash = -1;
	obj->c[length] = 0;
	return obj;
}


elString *elf_new_string(elState *R, char *junk) {
	int length = elf_cstrlen(junk);
	elString *obj = elf_new_string_of_length(R,length);
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


int elfstr_length_(elState *c) {
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


int elfstr_append_(elState *R) {
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
		} else elf_unreachable;
	}
	elf_add_new_string(R,buffer);
	return 1;
}


int elfstr_match_(elState *R) {
	elString *s = (elString*) elf_get_this(R);
	elString *p = elf_get_string(R,0);
	elf_add_integer(R,elf_cstrmatch(p->string,s->string));
	return 1;
}


int elfstr_gethash_(elState *R) {
	elString *str = (elString*) elf_get_this(R);
	elf_add_integer(R,str->hash);
	return 1;
}


int elfstr_tolowercase_(elState *R) {
	elString *str = (elString*) elf_get_this(R);
	elString *newstr = elf_pushnewstrlen(R,str->length);
	for (int i = 0; i < str->length; ++ i) {
		newstr->c[i] = elf_chrtolowercase(str->c[i]);
	}
	return 1;
}


int elfstr_touppercase_(elState *R) {
	elString *str = (elString*) elf_get_this(R);
	elString *newstr = elf_pushnewstrlen(R,str->length);
	for (int i = 0; i < str->length; ++ i) {
		newstr->c[i] = elf_chrtouppercase(str->c[i]);
	}
	return 1;
}


