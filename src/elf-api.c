/*
** See Copyright Notice In elf.h
** elf-api.c
** Main API
*/


elAPI int elf_get_num_args(elState *R) {
	return R->call->nx;
}


elAPI elObject *elf_get_this(elState *R) {
	return R->call->obj;
}


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
	if (v.tag == TAG_NIL) return elNIL;
	if (v.tag == TAG_STR) return v.x_str;
	elf_expected(R,TAG_STR,v.tag,x);
	return elNIL;
}


elAPI char *elf_get_cstring(elState *R, elRegId x) {
	elValue v = R->call->locals[x];
	if (v.tag == TAG_NIL) return elNIL;
	if (v.tag == TAG_STR) return v.x_str->c;
	elf_expected(R,TAG_STR,v.tag,x);
	return elNIL;
}


elAPI elObject *elf_get_object(elState *R, elRegId x) {
	elValue v = R->call->locals[x];
	if (v.tag == TAG_NIL) return elNIL;
	if (elf_isobj(v.tag)) return v.x_obj;
	elf_expected(R,TAG_OBJ,v.tag,x);
	return elNIL;
}


elAPI elTable *elf_get_table(elState *R, elRegId x) {
	elValue v = R->call->locals[x];
	if (v.tag == TAG_TAB) {
		return v.x_tab;
	} else if (v.tag != TAG_NIL) elf_expected(R,TAG_TAB,v.tag,x);
	return elNIL;
}


elAPI void elf_checkcl(elState *R, elRegId x) {
	elValue v = R->call->locals[x];
	if (v.tag != TAG_NIL && v.tag != TAG_CLS) {
		elf_throw(R,NO_BYTE,elf_tpf("expected closure at local %i",x));
		elNOCODE;
	}
}


elAPI elClosure *elf_get_closure(elState *R, elRegId x) {
	elf_checkcl(R,x);
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


elRegId elf_local_alloc(elState *R, int n) {
	elRegId id = R->stack_top - R->stack;
	if (id + n <= R->stack_length) {
		R->stack_top += n;
	} else elf_throw(R,NO_BYTE,"stack overflow");
	return id;
}


elRegId elf_add_value(elState *R, elValue v) {
	*R->top = v;
	return elf_local_alloc(R,1);
}

#define _INC_TOP do {\
	elRegId __i = R->top ++ - R->stk;\
	elASSERT(__i < R->stklen);\
} while(0)


void eld_add_nil(elState *R) {
	R->top->tag = TAG_NIL;
	_INC_TOP;
}


void elf_add_integer(elState *R, elInteger i) {
	R->top->tag = TAG_INT;
	R->top->i = i;
	_INC_TOP;
}


void elf_add_number(elState *R, elNumber n) {
	R->top->tag = TAG_NUM;
	R->top->n = n;
	_INC_TOP;
}


void elf_pushsys(elState *R, elHandle h) {
	R->top->tag = TAG_SYS;
	R->top->h = h;
	_INC_TOP;
}


elString *elf_add_string(elState *R, elString *str) {
	R->top->tag = TAG_STR;
	R->top->x_str = str;
	_INC_TOP;
	return str;
}


elString *elf_add_new_string(elState *R, char *chr) {
	return elf_add_string(R,elf_new_string(R,chr));
}


elString *elf_pushnewstrlen(elState *R, elInteger len) {
	return elf_add_string(R,elf_new_lstring(R,len));
}


elValue *elf_get_stack_top(elState *R) {
	return R->top;
}


void elf_set_stack_top(elState *R, elValue *top) {
	R->top = top;
}


elRegId elf_add_closure(elState *R, elClosure *cl) {
	R->top->tag = TAG_CLS;
	R->top->f   = cl;
	elRegId id = R->top - R->call->locals;
	_INC_TOP;
	return id;
}


elObject *elf_add_object(elState *R, elObject *obj) {
	R->top->tag = TAG_OBJ;
	R->top->x_obj = obj;
	_INC_TOP;
	return obj;
}


elObject *elf_add_new_object(elState *R, elInteger tell) {
	return elf_add_object(R,elf_new_object(R,OBJ_CUSTOM,tell));
}


elTable *elf_add_table(elState *R, elTable *tab) {
	elASSERT(tab->obj.color != GC_RED);
	R->top->tag = TAG_TAB;
	R->top->x_tab = tab;
	_INC_TOP;
	return tab;
}


elTable *elf_add_new_table(elState *R) {
	return elf_add_table(R,elf_new_table(R));
}


elRegId elf_pushbinding(elState *R, elBinding b) {
	R->top->tag = TAG_BID;
	R->top->c = b;
	elRegId id = R->top - R->call->locals;
	_INC_TOP;
	return id;
}

