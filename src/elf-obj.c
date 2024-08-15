/*
** See Copyright Notice In elf.h
** elf-obj.c
** Objects And Values
*/





// elAPI elValue elSTR(elString *s) {
// 	elValue v = elLITERAL(elValue){TAG_STR};
// 	v.x_str = s;
// 	return v;
// }


// elAPI elValue elCLS(elClosure *f) {
// 	elValue v = elLITERAL(elValue){TAG_CLS};
// 	v.x_cls = f;
// 	return v;
// }


// elAPI elValue elINT(elInteger i) {
// 	elValue v = (elValue){TAG_INT};
// 	v.x_int = i;
// 	return v;
// }


// elAPI elValue elNUM(elNumber n) {
// 	elValue v = (elValue){TAG_NUM};
// 	v.x_num = n;
// 	return v;
// }


// elAPI elValue elNIL() {
// 	elValue v = (elValue){TAG_NIL};
// 	v.x_int = 0;
// 	return v;
// }


elAPI elClosure *elf_new_closure(elState *S, elFileProto proto) {
	elClosure *cls = (elClosure *) elf_new_object(S,GC_CLS,sizeof(elClosure) + sizeof(elValue) * (proto.nlocals-1));
	cls->proto = proto;
	return cls;
}
