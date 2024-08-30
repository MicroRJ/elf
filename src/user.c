/*
** See Copyright Notice In elf.h
** user.c
*/


static void _expected(elf_Shell *S, elValueTag tag, elValueTag got, elf_StackId x) {
	elf_fail(S,NO_BYTE,elf_tpf("expected '%s' at local %i, instead got '%s'",tag2s[tag],x,tag2s[got]));
}


int elf_get_num_args(elf_Shell *S) {
	return S->frame->nargs-1;
}


elf_Value elf_get_arg(elf_Shell *S, int x) {
	return S->frame->locals[x+1];
}


elValueTag elf_get_tag(elf_Shell *S, int x) {
	return S->frame->locals[x+1].tag;
}


elf_Object *elf_get_this(elf_Shell *S) {
	return S->frame->locals[0].x_obj;
}


void elf_put_nil(elf_Shell *S) {
	PUSHV(S,elNIL());
}

void elf_put_closure(elf_Shell *S, elf_Closure *x) {
	PUSHV(S,elCLS(x));
}

void elf_put_object(elf_Shell *S, elf_Object *x) {
	PUSHV(S,elOBJ(x));
}

void elf_put_cfunction(elf_Shell *S, elf_CFunction x) {
	PUSHV(S,elCFN(x));
}

void elf_new_table(elf_Shell *S, elf_Table *x) {
	PUSHV(S,elTAB(x));
}

void elf_put_integer(elf_Shell *S, elf_Int x) {
	PUSHV(S,elINT(x));
}

void elf_put_number(elf_Shell *S, elf_Num x) {
	PUSHV(S,elNUM(x));
}

void elf_put_string(elf_Shell *S, elf_String *x) {
	PUSHV(S,elSTR(x));
}

void elf_put_handle(elf_Shell *S, elf_Handle x) {
	PUSHV(S,elSYS(x));
}

elf_String *elf_get_string(elf_Shell *R, elf_StackId x) {
	elf_Value v=elf_get_arg(R,x);
	if (v.tag==TAG_NIL) return 0;
	if (v.tag==TAG_STR) return v.x_str;
	_expected(R,TAG_STR,v.tag,x);
	return 0;
}


elAPI char *elf_get_text(elf_Shell *R, elf_StackId x) {
	elf_Value v=elf_get_arg(R,x);
	if (v.tag==TAG_NIL) return 0;
	if (v.tag==TAG_STR) return v.x_str->text;
	_expected(R,TAG_STR,v.tag,x);
	return 0;
}


elAPI elf_Object *elf_get_object(elf_Shell *R, elf_StackId x) {
	elf_Value v=elf_get_arg(R,x);
	if (v.tag==TAG_NIL) return 0;
	if (IS_TOBJ(v.tag)) return v.x_obj;
	_expected(R,TAG_OBJ,v.tag,x);
	return 0;
}


elAPI elf_Table *elf_get_table(elf_Shell *R, elf_StackId x) {
	elf_Value v = elf_get_arg(R,x);
	if (v.tag == TAG_TAB) {
		return v.x_tab;
	} else if (v.tag != TAG_NIL) _expected(R,TAG_TAB,v.tag,x);
	return 0;
}


elAPI elf_Closure *elf_get_closure(elf_Shell *R, elf_StackId x) {
	return elf_get_arg(R,x).x_cls;
}


elAPI elf_Handle elf_get_handle(elf_Shell *R, elf_StackId x) {
	elf_Value v = elf_get_arg(R,x);
	if (v.tag != TAG_NIL && v.tag != TAG_SYS) {
		elf_fail(R,NO_BYTE,elf_tpf("expected system object at local %i",x));
		NO_CODE;
	}
	return v.x_sys;
}


elAPI elf_Int elf_get_integer(elf_Shell *R, int x) {
	elf_Value v = elf_get_arg(R,x);
	if (v.tag == TAG_NUM) return (elf_Int) v.x_num;
	if (v.tag == TAG_INT) return v.x_int;
	_expected(R,TAG_INT,v.tag,x);
	return 0;
}


elf_Num elf_get_number(elf_Shell *R, elf_StackId x) {
	elf_Value v = elf_get_arg(R,x);
	if (v.tag == TAG_INT) return (elf_Num) v.x_int;
	if (v.tag == TAG_NUM) return v.x_num;
	_expected(R,TAG_NUM,v.tag,x);
	return 0;
}


elf_String *elf_new_string(elf_Shell *R, const char *text) {
	elf_String *string=elf_alloc_string(R,text);
	elf_put_string(R,string);
	return string;
}


elf_String *elf_new_string2(elf_Shell *R, elf_Int length) {
	elf_String *string=elf_alloc_string2(R,length);
	elf_put_string(R,string);
	return string;
}



elf_Object *elf_put_new_object(elf_Shell *R, elf_Int tell) {
	elf_Object *obj = elf_alloc_object(R,GC_OBJ,tell);
	elf_put_object(R,obj);
	return obj;
}


elf_Table *elf_put_new_table(elf_Shell *R) {
	elf_Table *tab = elf_alloc_table(R);
	elf_new_table(R,tab);
	return tab;
}

