/*
** See Copyright Notice In elf.h
** elf-obj.c
** Objects And Values
*/


elBool elf_tagisnumeric(elf_tag tag) {
	return (tag == TAG_NUM) || (tag == TAG_INT);
}


int elf_valisnil(elValue x) {
	return (x.tag == TAG_NIL) || (!elf_tagisnumeric(x.tag) && (x.p == elNIL));
}


elBool elf_tagisobj(elf_tag tag) {
	switch (tag) {
		case TAG_STR: case TAG_TAB:
		case TAG_OBJ: case TAG_CLS: {
			return ltrue;
		}
		default: return lfalse;
	}
}


elf_tag elf_objtotag(elf_objty type) {
	switch(type) {
		case OBJ_CLOSURE: return TAG_CLS;
		case OBJ_TAB: return TAG_TAB;
		case OBJ_STRING: return TAG_STR;
		default: elf_unreachable;
	}
	return -1;
}


elf_api elValue elf_valtab(elTable *tab) {
	elValue v = LITC(elValue){TAG_TAB};
	v.x_tab = tab;
	return v;
}


elf_api elValue elf_valbid(elBinding c) {
	elValue v = LITC(elValue){TAG_BID};
	v.c = c;
	return v;
}


elf_api elValue elf_valsys(elHandle h) {
	elValue v = LITC(elValue){TAG_SYS};
	v.h = h;
	return v;
}


elf_api elValue elf_valstr(elString *s) {
	elValue v = LITC(elValue){TAG_STR};
	v.s = s;
	return v;
}


elf_api elValue elf_valcls(elClosure *f) {
	elValue v = LITC(elValue){TAG_CLS};
	v.f = f;
	return v;
}


elf_api elValue elf_valint(elInteger i) {
	elValue v = (elValue){TAG_INT};
	v.i = i;
	return v;
}


elf_api elValue elf_valnum(elNumber n) {
	elValue v = (elValue){TAG_NUM};
	v.n = n;
	return v;
}

