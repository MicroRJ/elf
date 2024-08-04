/*
** See Copyright Notice In elf.h
** elf-api.c
** Main API
*/







elAPI elValueTag elf_get_tag(elState *R, elRegId x) {
	return R->call->locals[x].tag;
}


elAPI elValue elf_get_value(elState *R, elRegId x) {
	return R->call->locals[x];
}


void elf_expected(elState *S, elValueTag tag, elValueTag got, elRegId x) {
	elf_throw(S,NO_BYTE,elf_tpf("expected '%s' at local %i, instead got '%s'",tag2s[tag],x,tag2s[got]));
}


elAPI elString *elf_get_string(elState *R, elRegId x) {
	elValue v = R->call->locals[x];
	if (v.tag == TAG_NIL) return 0;
	if (v.tag == TAG_STR) return v.x_str;
	elf_expected(R,TAG_STR,v.tag,x);
	return 0;
}


elAPI char *elf_get_charstring(elState *R, elRegId x) {
	elValue v = R->call->locals[x];
	if (v.tag == TAG_NIL) return 0;
	if (v.tag == TAG_STR) return v.x_str->c;
	elf_expected(R,TAG_STR,v.tag,x);
	return 0;
}


elAPI elObject *elf_get_object(elState *R, elRegId x) {
	elValue v = R->call->locals[x];
	if (v.tag == TAG_NIL) return 0;
	if (elISOBJTAG(v.tag)) return v.x_obj;
	elf_expected(R,TAG_OBJ,v.tag,x);
	return 0;
}


elAPI elTable *elf_get_table(elState *R, elRegId x) {
	elValue v = R->call->locals[x];
	if (v.tag == TAG_TAB) {
		return v.x_tab;
	} else if (v.tag != TAG_NIL) elf_expected(R,TAG_TAB,v.tag,x);
	return 0;
}


elAPI elClosure *elf_get_closure(elState *R, elRegId x) {
	return R->call->locals[x].x_cls;
}


elAPI elHandle elf_get_handle(elState *R, elRegId x) {
	elValue v = R->call->locals[x];
	if (v.tag != TAG_NIL && v.tag != TAG_SYS) {
		elf_throw(R,NO_BYTE,elf_tpf("expected system object at local %i",x));
		elNOCODE;
	}
	return v.h;
}


elAPI elInteger elf_get_integer(elState *R, int x) {
	elValue v = R->call->locals[x];
	if (v.tag == TAG_NUM) return (elInteger) v.x_num;
	if (v.tag == TAG_INT) return v.x_int;
	elf_expected(R,TAG_INT,v.tag,x);
	return 0;
}


elAPI elNumber elf_get_number(elState *R, elRegId x) {
	elValue v = R->call->locals[x];
	if (v.tag == TAG_INT) return (elNumber) v.i;
	if (v.tag == TAG_NUM) return v.x_num;
	elf_expected(R,TAG_NUM,v.tag,x);
	return 0;
}


void eld_add_nil(elState *R) {
	elGETTOP(R)->tag = TAG_NIL;
	elGETTOP(R) += 1;
}


void elf_add_integer(elState *R, elInteger i) {
	elGETTOP(R)->tag = TAG_INT;
	elGETTOP(R)->i = i;
	elGETTOP(R) += 1;
}


void elf_add_number(elState *R, elNumber n) {
	elGETTOP(R)->tag = TAG_NUM;
	elGETTOP(R)->n = n;
	elGETTOP(R) += 1;
}


void elf_pushsys(elState *R, elHandle h) {
	elGETTOP(R)->tag = TAG_SYS;
	elGETTOP(R)->h = h;
	elGETTOP(R) += 1;
}


elString *elf_add_string(elState *R, elString *str) {
	elGETTOP(R)->tag = TAG_STR;
	elGETTOP(R)->x_str = str;
	elGETTOP(R) += 1;
	return str;
}


elString *elf_add_new_string(elState *R, char *chr) {
	return elf_add_string(R,elf_new_string(R,chr));
}


elString *elf_pushnewstrlen(elState *R, elInteger len) {
	return elf_add_string(R,elf_new_lstring(R,len));
}


void elf_add_closure(elState *R, elClosure *cls) {
	elValue *T = elGETTOP(R) ++;
	T->tag	 = TAG_CLS;
	T->x_cls  = cls;
	elASSERT(cls->proto.name == 0 || elOBJCOLOR(cls->proto.name) != GC_RED);
	elASSERT(cls->proto.contents == 0 || elOBJCOLOR(cls->proto.contents) != GC_RED);
}


elObject *elf_add_object(elState *R, elObject *obj) {
	elValue *T = elGETTOP(R) ++;
	T->tag   = TAG_OBJ;
	T->x_obj = obj;
	return obj;
}


elObject *elf_add_new_object(elState *R, elInteger tell) {
	return elf_add_object(R,elf_new_object(R,GC_OBJ,tell));
}


elTable *elf_add_table(elState *R, elTable *tab) {
	elValue *T = elGETTOP(R) ++;
	T->tag   = TAG_TAB;
	T->x_tab = tab;
	return tab;
}


elTable *elf_add_new_table(elState *R) {
	return elf_add_table(R,elf_new_table(R));
}


void elf_pushbinding(elState *R, elCFunction b) {
	elValue *T = elGETTOP(R) ++;
	T->tag   = TAG_CFN;
	T->x_cfn = b;
}

