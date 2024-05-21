/*
** See Copyright Notice In elf.h
** elf-str.c
** String
*/


elTable *elf_newstrmetatab(elState *R) {
	elTable *tab = elf_pushnewtab(R);
	elf_tabmfld(R,tab,"length",elfstr_length_);
	elf_tabmfld(R,tab,"match",elfstr_match_);
	elf_tabmfld(R,tab,"gethash",elfstr_gethash_);
	elf_tabmfld(R,tab,"append",elfstr_append_);
	elf_tabmfld(R,tab,"touppercase",elfstr_touppercase_);
	elf_tabmfld(R,tab,"tolowercase",elfstr_tolowercase_);
	elf_tabmfld(R,tab,"__add",elfstr_append_);
	return tab;
}


elString *elf_newstrlen(elState *R, elInteger length) {
	elString *obj = elf_newobj(R,OBJ_STRING,sizeof(elString)+length+1);
	if (R) obj->obj.metatable = R->metatab_str;
	obj->length = length;
	obj->hash = -1;
	obj->c[length] = 0;
	return obj;
}


elString *elf_newstr(elState *R, char *junk) {
	int length = elf_cstrlen(junk);
	elString *obj = elf_newstrlen(R,length);
	elf_memcopy(obj->c,junk,length);
	obj->hash = elf_tabhashstr((char*)junk);
	return obj;
}


elBool elf_streq(elString *x, elString *y) {
	if (x == y) return ltrue;
	/* assuming we use the same hash function */
	if (x->hash != y->hash) return lfalse;

	if (x->length != y->length) return lfalse;

	return S_eq(x->string,y->string);
}


int elfstr_length_(elState *c) {
	elf_pushint(c,((elString*)c->f->obj)->length);
	return 1;
}


int elfstr_append_(elState *R) {
	elString *s = (elString*) elf_getthis(R);
	elValue v = elf_getany(R,0);
	if (v.tag == TAG_INT) {
		elString *r = elf_newstrlen(R,s->length+1);
		elf_pushstr(R,r);
		memcpy(r->c,s->c,s->length);
		r->c[r->length-1] = v.i;
		r->hash = elf_tabhashstr(r->c);
	} else elf_unreachable;
	return 1;
}


int elfstr_match_(elState *R) {
	elString *s = (elString*) elf_getthis(R);
	elString *p = elf_getstr(R,0);
	elf_pushint(R,elf_cstrmatch(p->string,s->string));
	return 1;
}


int elfstr_gethash_(elState *R) {
	elString *str = (elString*) elf_getthis(R);
	elf_pushint(R,str->hash);
	return 1;
}


int elfstr_tolowercase_(elState *R) {
	elString *str = (elString*) elf_getthis(R);
	elString *newstr = elf_pushnewstrlen(R,str->length);
	for (int i = 0; i < str->length; ++ i) {
		newstr->c[i] = elf_chrtolowercase(str->c[i]);
	}
	return 1;
}


int elfstr_touppercase_(elState *R) {
	elString *str = (elString*) elf_getthis(R);
	elString *newstr = elf_pushnewstrlen(R,str->length);
	for (int i = 0; i < str->length; ++ i) {
		newstr->c[i] = elf_chrtouppercase(str->c[i]);
	}
	return 1;
}


