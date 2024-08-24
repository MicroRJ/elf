/*
** See Copyright Notice In elf.h
** elf-api.c
** Main API
*/



void elf_expected(elState *S, ValueTag tag, ValueTag got, elRegId x) {
	elf_fail(S,NO_BYTE,elf_tpf("expected '%s' at local %i, instead got '%s'",tag2s[tag],x,tag2s[got]));
}


elAPI elString *elf_get_string(elState *R, elRegId x) {
	elValue v = elGETARG(R,x);
	if (v.tag == TAG_NIL) return 0;
	if (v.tag == TAG_STR) return v.x_str;
	elf_expected(R,TAG_STR,v.tag,x);
	return 0;
}


elAPI char *elf_get_text(elState *R, elRegId x) {
	elValue v = elGETARG(R,x);
	if (v.tag == TAG_NIL) return 0;
	if (v.tag == TAG_STR) return v.x_str->text;
	elf_expected(R,TAG_STR,v.tag,x);
	return 0;
}


elAPI elObject *elf_get_object(elState *R, elRegId x) {
	elValue v = elGETARG(R,x);
	if (v.tag == TAG_NIL) return 0;
	if (elISOBJTAG(v.tag)) return v.x_obj;
	elf_expected(R,TAG_OBJ,v.tag,x);
	return 0;
}


elAPI elTable *elf_get_table(elState *R, elRegId x) {
	elValue v = elGETARG(R,x);
	if (v.tag == TAG_TAB) {
		return v.x_tab;
	} else if (v.tag != TAG_NIL) elf_expected(R,TAG_TAB,v.tag,x);
	return 0;
}


elAPI elClosure *elf_get_closure(elState *R, elRegId x) {
	return elGETARG(R,x).x_cls;
}


elAPI elHandle elf_get_handle(elState *R, elRegId x) {
	elValue v = elGETARG(R,x);
	if (v.tag != TAG_NIL && v.tag != TAG_SYS) {
		elf_fail(R,NO_BYTE,elf_tpf("expected system object at local %i",x));
		NO_CODE;
	}
	return v.x_sys;
}


elAPI elInteger elf_get_integer(elState *R, int x) {
	elValue v = elGETARG(R,x);
	if (v.tag == TAG_NUM) return (elInteger) v.x_num;
	if (v.tag == TAG_INT) return v.x_int;
	elf_expected(R,TAG_INT,v.tag,x);
	return 0;
}


elNumber elf_get_number(elState *R, elRegId x) {
	elValue v = elGETARG(R,x);
	if (v.tag == TAG_INT) return (elNumber) v.x_int;
	if (v.tag == TAG_NUM) return v.x_num;
	elf_expected(R,TAG_NUM,v.tag,x);
	return 0;
}


elString *elf_put_new_string(elState *R, const char *text) {
	elString *string = elf_new_string(R,text);
	elf_put_string(R,string);
	return string;
}


elString *elf_put_new_string2(elState *R, elInteger len) {
	elString *string = elf_new_lstring(R,len);
	elf_put_string(R,string);
	return string;
}



elObject *elf_put_new_object(elState *R, elInteger tell) {
	elObject *obj = elf_new_object(R,GC_OBJ,tell);
	elf_put_object(R,obj);
	return obj;
}


elTable *elf_put_new_table(elState *R) {
	elTable *tab = elf_new_table(R);
	elf_put_table(R,tab);
	return tab;
}

