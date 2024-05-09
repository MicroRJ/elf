/*
** See Copyright Notice In elf.h
** elf-api.c
** Main user API
*/



elf_api elf_Object *elf_getthis(elf_State *R) {
	return R->call->obj;
}


elf_api elf_Value elf_getany(elf_State *R, elf_localid x) {
	return R->call->locals[x];
}


elf_api elf_String *elf_getstr(elf_State *R, elf_localid x) {
	elf_Value v = R->call->locals[x];
	if (v.tag != TAG_NIL && v.tag != TAG_STR) {
		elf_throw(R,NO_BYTE,elf_tpf("expected string at local %i",x));
		LNOBRANCH;
	}
	return v.s;
}


elf_api char *elf_getcstr(elf_State *R, elf_localid x) {
	elf_Value v = R->call->locals[x];
	if (v.tag == TAG_NIL) return lnil;
	if (v.tag == TAG_STR) return v.x_str->c;
	elf_throw(R,NO_BYTE,elf_tpf("expected string at local %i",x));
	LNOBRANCH;
	return lnil;
}


elf_api elf_Object *elf_getobj(elf_State *R, elf_localid x) {
	elf_Value v = R->call->locals[x];
	if (v.tag != TAG_NIL && !elf_tagisobj(v.tag)) {
		elf_throw(R,NO_BYTE,elf_tpf("expected object at local %i",x));
		LNOBRANCH;
	}
	return v.x_obj;
}


elf_api elf_Table *elf_gettab(elf_State *R, elf_localid x) {
	elf_Value v = R->call->locals[x];
	if (v.tag != TAG_NIL && v.tag != TAG_TAB) {
		elf_throw(R,NO_BYTE,elf_tpf("expected table at local %i",x));
		LNOBRANCH;
	}
	return v.x_tab;
}


elf_api void elf_checkcl(elf_State *R, elf_localid x) {
	elf_Value v = R->call->locals[x];
	if (v.tag != TAG_NIL && v.tag != TAG_CLS) {
		elf_throw(R,NO_BYTE,elf_tpf("expected closure at local %i",x));
		LNOBRANCH;
	}
}


elf_api elf_Closure *elf_getcls(elf_State *R, elf_localid x) {
	elf_checkcl(R,x);
	return R->call->locals[x].f;
}


elf_api elf_Handle elf_getsys(elf_State *R, elf_localid x) {
	elf_Value v = R->call->locals[x];
	if (v.tag != TAG_NIL && v.tag != TAG_SYS) {
		elf_throw(R,NO_BYTE,elf_tpf("expected system object at local %i",x));
		LNOBRANCH;
	}
	return v.h;
}


elf_api elf_String *elf_checkstr(elf_State *R, elf_localid x) {
	elf_ensure(R->call->locals[x].tag == TAG_STR);
	return R->call->locals[x].s;
}


elf_api elf_int elf_getint(elf_State *R, int x) {
	elf_Value v = R->call->locals[x];
	if (v.tag == TAG_NUM) {
		return (elf_int) v.n;
	}
	if (v.tag != TAG_INT) {
		elf_throw(R,NO_BYTE,elf_tpf("expected integer at local %i",x));
	}
	return v.i;
}


elf_api elf_num elf_getnum(elf_State *R, elf_localid x) {
	elf_Value v = R->call->locals[x];
	if (v.tag == TAG_INT) return (elf_num) v.i;
	if (v.tag != TAG_NUM) {
		elf_throw(R,NO_BYTE,elf_tpf("expected number at local %i",x));
	}
	return v.n;
}


elf_localid elf_pushmany(elf_State *R, int n) {
	elf_localid stkptr = R->top - R->stk;
	if (stkptr <= R->stklen) {
		R->top += n;
	} else LNOBRANCH;
	return stkptr;
}


elf_localid elf_pushany(elf_State *R, elf_Value v) {
	*R->top = v;
	return elf_pushmany(R,1);
}

#define _INC_TOP do {\
	elf_localid __i = R->top ++ - R->stk;\
	elf_ensure(__i < R->stklen);\
} while(0)


void elf_pushnil(elf_State *R) {
	R->top->tag = TAG_NIL;
	_INC_TOP;
}


void elf_pushint(elf_State *R, elf_int i) {
	R->top->tag = TAG_INT;
	R->top->i = i;
	_INC_TOP;
}


void elf_pushnum(elf_State *R, elf_num n) {
	R->top->tag = TAG_NUM;
	R->top->n = n;
	_INC_TOP;
}


void elf_pushsys(elf_State *R, elf_Handle h) {
	R->top->tag = TAG_SYS;
	R->top->h = h;
	_INC_TOP;
}


elf_String *elf_pushstr(elf_State *R, elf_String *str) {
	R->top->tag = TAG_STR;
	R->top->x_str = str;
	_INC_TOP;
	return str;
}


elf_String *elf_pushnewstr(elf_State *R, char *chr) {
	return elf_pushstr(R,elf_newstr(R,chr));
}


elf_String *elf_pushnewstrlen(elf_State *R, elf_int len) {
	return elf_pushstr(R,elf_newstrlen(R,len));
}


elf_Value *elf_gettop(elf_State *R) {
	return R->top;
}


void elf_settop(elf_State *R, elf_Value *top) {
	R->top = top;
}


elf_localid elf_pushcls(elf_State *R, elf_Closure *cl) {
	R->top->tag = TAG_CLS;
	R->top->f   = cl;
	elf_localid id = R->top - R->call->locals;
	_INC_TOP;
	return id;
}


elf_Object *elf_pushobj(elf_State *R, elf_Object *obj) {
	R->top->tag = TAG_OBJ;
	R->top->x_obj = obj;
	_INC_TOP;
	return obj;
}


elf_Object *elf_pushnewobj(elf_State *R, elf_int tell) {
	return elf_pushobj(R,elf_newobj(R,OBJ_CUSTOM,tell));
}


elf_Table *elf_pushtab(elf_State *R, elf_Table *tab) {
	R->top->tag = TAG_TAB;
	R->top->x_tab = tab;
	_INC_TOP;
	return tab;
}


elf_Table *elf_pushnewtab(elf_State *R) {
	return elf_pushtab(R,elf_newtab(R));
}


elf_localid elf_pushbinding(elf_State *R, lBinding b) {
	R->top->tag = TAG_BID;
	R->top->c = b;
	elf_localid id = R->top - R->call->locals;
	_INC_TOP;
	return id;
}


elf_localid elf_pushnewcls(elf_State *R, elf_Proto fn) {
	elf_Closure *cl = elf_newcls(R,fn);
	R->top -= fn.ncaches;
	int i;
	for (i=0; i<fn.ncaches; ++i) {
		cl->caches[i] = R->top[i];
	}
	return elf_pushcls(R,cl);
}
