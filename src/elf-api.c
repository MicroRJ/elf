/*
** See Copyright Notice In elf.h
** elf-api.c
** Main API
*/


elf_api elObject *elf_getthis(elState *R) {
	return R->call->obj;
}


elf_api elf_tag elf_gettag(elState *R, elf_localid x) {
	return R->call->locals[x].tag;
}


elf_api elValue elf_getany(elState *R, elf_localid x) {
	return R->call->locals[x];
}


void elf_expected(elState *S, elf_tag tag, elf_tag got, elf_localid x) {
	elf_throw(S,NO_BYTE,elf_tpf("expected '%s' at local %i, instead got '%s'",tag2s[tag],x,tag2s[got]));
}


elf_api elString *elf_getstr(elState *R, elf_localid x) {
	elValue v = R->call->locals[x];
	if (v.tag == TAG_NIL) return elNIL;
	if (v.tag == TAG_STR) return v.x_str;
	elf_expected(R,TAG_STR,v.tag,x);
	return elNIL;
}


elf_api char *elf_getcstr(elState *R, elf_localid x) {
	elValue v = R->call->locals[x];
	if (v.tag == TAG_NIL) return elNIL;
	if (v.tag == TAG_STR) return v.x_str->c;
	elf_expected(R,TAG_STR,v.tag,x);
	return elNIL;
}


elf_api elObject *elf_getobj(elState *R, elf_localid x) {
	elValue v = R->call->locals[x];
	if (v.tag == TAG_NIL) return elNIL;
	if (elf_tagisobj(v.tag)) return v.x_obj;
	elf_expected(R,TAG_OBJ,v.tag,x);
	return elNIL;
}


elf_api elTable *elf_gettab(elState *R, elf_localid x) {
	elValue v = R->call->locals[x];
	if (v.tag != TAG_NIL && v.tag != TAG_TAB) {
		elf_throw(R,NO_BYTE,elf_tpf("expected table at local %i",x));
		elf_unreachable;
	}
	return v.x_tab;
}


elf_api void elf_checkcl(elState *R, elf_localid x) {
	elValue v = R->call->locals[x];
	if (v.tag != TAG_NIL && v.tag != TAG_CLS) {
		elf_throw(R,NO_BYTE,elf_tpf("expected closure at local %i",x));
		elf_unreachable;
	}
}


elf_api elClosure *elf_getcls(elState *R, elf_localid x) {
	elf_checkcl(R,x);
	return R->call->locals[x].f;
}


elf_api elHandle elf_getsys(elState *R, elf_localid x) {
	elValue v = R->call->locals[x];
	if (v.tag != TAG_NIL && v.tag != TAG_SYS) {
		elf_throw(R,NO_BYTE,elf_tpf("expected system object at local %i",x));
		elf_unreachable;
	}
	return v.h;
}


elf_api elInteger elf_getint(elState *R, int x) {
	elValue v = R->call->locals[x];
	if (v.tag == TAG_NUM) return (elInteger) v.x_num;
	if (v.tag == TAG_INT) return v.x_int;
	elf_expected(R,TAG_INT,v.tag,x);
	return 0;
}


elf_api elNumber elf_getnum(elState *R, elf_localid x) {
	elValue v = R->call->locals[x];
	if (v.tag == TAG_INT) return (elNumber) v.i;
	if (v.tag == TAG_NUM) return v.x_num;
	elf_expected(R,TAG_NUM,v.tag,x);
	return 0;
}


elf_localid elf_pushmany(elState *R, int n) {
	elf_localid stkptr = R->top - R->stk;
	if (stkptr <= R->stklen) {
		R->top += n;
	} else elf_unreachable;
	return stkptr;
}


elf_localid elf_pushany(elState *R, elValue v) {
	*R->top = v;
	return elf_pushmany(R,1);
}

#define _INC_TOP do {\
	elf_localid __i = R->top ++ - R->stk;\
	elf_ensure(__i < R->stklen);\
} while(0)


void elf_pushnil(elState *R) {
	R->top->tag = TAG_NIL;
	_INC_TOP;
}


void elf_pushint(elState *R, elInteger i) {
	R->top->tag = TAG_INT;
	R->top->i = i;
	_INC_TOP;
}


void elf_pushnum(elState *R, elNumber n) {
	R->top->tag = TAG_NUM;
	R->top->n = n;
	_INC_TOP;
}


void elf_pushsys(elState *R, elHandle h) {
	R->top->tag = TAG_SYS;
	R->top->h = h;
	_INC_TOP;
}


elString *elf_pushstr(elState *R, elString *str) {
	R->top->tag = TAG_STR;
	R->top->x_str = str;
	_INC_TOP;
	return str;
}


elString *elf_pushnewstr(elState *R, char *chr) {
	return elf_pushstr(R,elf_newstr(R,chr));
}


elString *elf_pushnewstrlen(elState *R, elInteger len) {
	return elf_pushstr(R,elf_newstrlen(R,len));
}


elValue *elf_gettop(elState *R) {
	return R->top;
}


void elf_settop(elState *R, elValue *top) {
	R->top = top;
}


elf_localid elf_pushcls(elState *R, elClosure *cl) {
	R->top->tag = TAG_CLS;
	R->top->f   = cl;
	elf_localid id = R->top - R->call->locals;
	_INC_TOP;
	return id;
}


elObject *elf_pushobj(elState *R, elObject *obj) {
	R->top->tag = TAG_OBJ;
	R->top->x_obj = obj;
	_INC_TOP;
	return obj;
}


elObject *elf_pushnewobj(elState *R, elInteger tell) {
	return elf_pushobj(R,elf_newobj(R,OBJ_CUSTOM,tell));
}


elTable *elf_pushtab(elState *R, elTable *tab) {
	R->top->tag = TAG_TAB;
	R->top->x_tab = tab;
	_INC_TOP;
	return tab;
}


elTable *elf_pushnewtab(elState *R) {
	return elf_pushtab(R,elf_newtab(R));
}


elf_localid elf_pushbinding(elState *R, elBinding b) {
	R->top->tag = TAG_BID;
	R->top->c = b;
	elf_localid id = R->top - R->call->locals;
	_INC_TOP;
	return id;
}


elf_localid elf_pushnewcls(elState *R, elProto fn) {
	elClosure *cl = elf_newcls(R,fn);
	R->top -= fn.zcache;
	int i;
	for (i=0; i<fn.zcache; ++i) {
		cl->caches[i] = R->top[i];
	}
	return elf_pushcls(R,cl);
}
