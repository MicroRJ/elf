/*
** See Copyright Notice In elf.h
** elf-obj.c
** Objects And Values
*/







elAPI elValue elf_tab(elTable *tab) {
	elValue v = elLITERAL(elValue){TAG_TAB};
	v.x_tab = tab;
	return v;
}


elAPI elValue elf_obj(elObject *obj) {
	elValue v = elLITERAL(elValue){TAG_OBJ};
	v.x_obj = obj;
	return v;
}


elAPI elValue elf_binding_value(elBinding c) {
	elValue v = elLITERAL(elValue){TAG_CFN};
	v.c = c;
	return v;
}


elAPI elValue elf_handle_value(elHandle h) {
	elValue v = elLITERAL(elValue){TAG_SYS};
	v.x_sys = h;
	return v;
}


elAPI elValue elf_string_value(elString *s) {
	elValue v = elLITERAL(elValue){TAG_STR};
	v.x_str = s;
	return v;
}


elAPI elValue elf_closure_value(elClosure *f) {
	elValue v = elLITERAL(elValue){TAG_CLS};
	v.x_cls = f;
	return v;
}


elAPI elValue elf_integer_value(elInteger i) {
	elValue v = (elValue){TAG_INT};
	v.x_int = i;
	return v;
}


elAPI elValue elf_number_value(elNumber n) {
	elValue v = (elValue){TAG_NUM};
	v.x_num = n;
	return v;
}


elAPI elValue elf_nil_value() {
	elValue v = (elValue){TAG_NIL};
	v.x_int = 0;
	return v;
}


elAPI elClosure *elf_new_closure(elState *S, elFileProto proto) {
	elClosure *cls = (elClosure *) elf_new_object(S,OBJ_CLS,sizeof(elClosure) + sizeof(elValue) * (proto.nlocals-1));
	cls->proto = proto;
	return cls;
}
