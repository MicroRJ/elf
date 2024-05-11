/*
** See Copyright Notice In elf.h
** elf-obj.c
** Objects And Values
*/


elf_bool elf_tagisnumeric(elf_valtag tag) {
	return (tag == TAG_NUM) || (tag == TAG_INT);
}


int elf_valisnil(elf_Value x) {
	return (x.tag == TAG_NIL) || (!elf_tagisnumeric(x.tag) && (x.p == lnil));
}


elf_bool elf_tagisobj(elf_valtag tag) {
	switch (tag) {
		case TAG_STR: case TAG_TAB:
		case TAG_OBJ: case TAG_CLS: {
			return ltrue;
		}
		default: return lfalse;
	}
}


elf_valtag elf_objtotag(elf_objty type) {
	switch(type) {
		case OBJ_CLOSURE: return TAG_CLS;
		case OBJ_TAB: return TAG_TAB;
		case OBJ_STRING: return TAG_STR;
		default: elf_unreachable;
	}
	return -1;
}


elf_api elf_Value elf_valtab(elf_Table *tab) {
	elf_Value v = LITC(elf_Value){TAG_TAB};
	v.x_tab = tab;
	return v;
}


elf_api elf_Value elf_valbid(lBinding c) {
	elf_Value v = LITC(elf_Value){TAG_BID};
	v.c = c;
	return v;
}


elf_api elf_Value elf_valsys(elf_Handle h) {
	elf_Value v = LITC(elf_Value){TAG_SYS};
	v.h = h;
	return v;
}


elf_api elf_Value elf_valstr(elf_String *s) {
	elf_Value v = LITC(elf_Value){TAG_STR};
	v.s = s;
	return v;
}


elf_api elf_Value elf_valcls(elf_Closure *f) {
	elf_Value v = LITC(elf_Value){TAG_CLS};
	v.f = f;
	return v;
}


elf_api elf_Value elf_valint(elf_int i) {
	elf_Value v = (elf_Value){TAG_INT};
	v.i = i;
	return v;
}


elf_api elf_Value elf_valnum(elf_num n) {
	elf_Value v = (elf_Value){TAG_NUM};
	v.n = n;
	return v;
}


int elf_valfpf(FILE *file, elf_Value v, elf_bool quotes) {
	switch (v.tag) {
		case TAG_NIL: return fprintf(file,"nil");
		case TAG_SYS: return fprintf(file,"h%llX",v.i);
		case TAG_INT: return fprintf(file,"%lli",v.i);
		case TAG_NUM: return fprintf(file,"%f",v.n);
		case TAG_CLS: return fprintf(file,"F()");
		case TAG_BID: return fprintf(file,"C()");
		case TAG_TAB: {
			int wrote = 0;
			elf_Table *t = v.t;
			wrote += fprintf(file,"{");
			elf_arrfori(t->v) {
				if (i != 0) wrote += fprintf(file,", ");
				wrote += elf_valfpf(file,t->v[i],ltrue);
			}
			wrote += fprintf(file,"}");
			return wrote;
		} break;
		case TAG_STR: {
			if (quotes) {
				return fprintf(file,"\"%s\"",v.s->string);
			} else {
				return fprintf(file,"%s",v.s->string);
			}
		} break;
		default: return fprintf(file,"(?)");
	}
}
