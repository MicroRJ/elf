/*
** See Copyright Notice In elf.h
** elf-str.c
** String
*/


elf_Table *elf_newstrmetatab(elf_State *R) {
	elf_Table *tab = elf_pushnewtab(R);
	elf_tabmfld(R,tab,"length",elfstr_length_);
	elf_tabmfld(R,tab,"match",elfstr_match_);
	elf_tabmfld(R,tab,"gethash",elfstr_gethash_);
	elf_tabmfld(R,tab,"append",elfstr_append_);
	elf_tabmfld(R,tab,"touppercase",elfstr_touppercase_);
	elf_tabmfld(R,tab,"tolowercase",elfstr_tolowercase_);
	elf_tabmfld(R,tab,"__add",elfstr_append_);
	return tab;
}


elf_String *elf_newstrlen(elf_State *R, elf_int length) {
	elf_String *obj = elf_newobj(R,OBJ_STRING,sizeof(elf_String)+length+1);
	if (R) obj->obj.metatable = R->metatab_str;
	obj->length = length;
	obj->hash = -1;
	obj->c[length] = 0;
	return obj;
}


elf_String *elf_newstr(elf_State *R, char *junk) {
	int length = elf_cstrlen(junk);
	elf_String *obj = elf_newstrlen(R,length);
	elf_memcopy(obj->c,junk,length);
	obj->hash = elf_tabhashstr((char*)junk);
	return obj;
}


elf_bool elf_streq(elf_String *x, elf_String *y) {
	if (x == y) return ltrue;
	/* assuming we use the same hash function */
	if (x->hash != y->hash) return lfalse;

	if (x->length != y->length) return lfalse;

	return S_eq(x->string,y->string);
}


int elfstr_length_(elf_State *c) {
	elf_pushint(c,((elf_String*)c->f->obj)->length);
	return 1;
}


int elfstr_append_(elf_State *R) {
	elf_String *s = (elf_String*) elf_getthis(R);
	elf_Value v = elf_getany(R,0);
	if (v.tag == TAG_INT) {
		elf_String *r = elf_newstrlen(R,s->length+1);
		elf_pushstr(R,r);
		memcpy(r->c,s->c,s->length);
		r->c[r->length-1] = v.i;
		r->hash = elf_tabhashstr(r->c);
	} else elf_unreachable;
	return 1;
}


int elfstr_match_(elf_State *R) {
	elf_String *s = (elf_String*) elf_getthis(R);
	elf_String *p = elf_getstr(R,0);
	elf_pushint(R,elf_cstrmatch(p->string,s->string));
	return 1;
}


int elfstr_gethash_(elf_State *R) {
	elf_String *str = (elf_String*) elf_getthis(R);
	elf_pushint(R,str->hash);
	return 1;
}


int elfstr_tolowercase_(elf_State *R) {
	elf_String *str = (elf_String*) elf_getthis(R);
	elf_String *newstr = elf_pushnewstrlen(R,str->length);
	for (int i = 0; i < str->length; ++ i) {
		newstr->c[i] = elf_chrtolowercase(str->c[i]);
	}
	return 1;
}


int elfstr_touppercase_(elf_State *R) {
	elf_String *str = (elf_String*) elf_getthis(R);
	elf_String *newstr = elf_pushnewstrlen(R,str->length);
	for (int i = 0; i < str->length; ++ i) {
		newstr->c[i] = elf_chrtouppercase(str->c[i]);
	}
	return 1;
}


