//
// See Copyright Notice In elf.h
//

//
// todo: why does this file even exist! merge with internal.c
//

#define TOP(S) (S->stack_ptr)

static inline
elf_stkid incstackptr(elf_State *S) {
	elf_stkid index = S->stack_ptr ++ - S->stack;
	ASSERT(index < S->stack_max);
	return index;
}

static inline
elf_String *stkstr(elf_State *inter, int index) {
	elf_Value *v = inter->stack_ptr + index;
	if (v->tag != elf_tag_Nil && v->tag != elf_tag_String) {
		elf_error(inter, NO_BYTE, "expected string");
	}
	return v->x_str;
}

elf_pubapi
int elf_gettop(elf_State *inter) {
	return inter->stack_ptr - inter->stack;
}

/* todo: remove the asexpr thing? */
elf_pubapi
int elf_loadcode(elf_State *inter, bool asexpr) {
	elf_String *name = stkstr(inter, -2);
	elf_String *contents = stkstr(inter, -1);
	if (!name || !contents) {
		elf_lerror("'%s': could not load code", name->text);
		elf_pushnil(inter);
		return false;
	}

	elf_Proto proto = elf_compile(inter, name, contents, asexpr);

	// pops the contents at the same time
	elf_Closure *closure = elf_alloc_closure(inter, proto);
	vSetClosure(&inter->stack_ptr[-1], closure);
	return true;
}



elf_pubapi
int elf_pushmetatab(elf_State *inter, int objstk) {
	elf_Value v = inter->stack[objstk];
	if (!isobj(v)) {
		elf_errorf(inter, -1, "expected object at %i", objstk);
	}
	vsettab(inter->stack_ptr, vgetobj(v)->meta);
	return incstackptr(inter);
}


elf_pubapi
void elf_setmetatab(elf_State *inter, int objstk, int tabstk) {
	elf_Value obj = inter->stack[objstk];
	elf_Value tab = inter->stack[tabstk];
	if (!isobj(obj)) {
		elf_errorf(inter, -1, "expected object at %i", objstk);
	}
	if (!istab(tab)) {
		elf_errorf(inter, -1, "expected table at %i", tabstk);
	}
	vgetobj(obj)->meta = vgettab(tab);
}







elf_pubapi
elf_Tag elf_gettag(elf_State *inter, int x) {
	return inter->stack[x].tag;
}


elf_pubapi
elf_Number elf_tonum(elf_State *inter, int x) {
	elf_Value v = inter->stack[x];
	if (isint(v)) return vgetint(v); // cast
	if (isnum(v)) return vgetnum(v);
	_check_arg_tag(inter, elf_tag_Num, v.tag,x);
	return 0;
}


elf_pubapi
elf_Integer elf_toint(elf_State *inter, int x) {
	elf_Value v = inter->stack[x];
	if (isnum(v)) return vgetnum(v); // cast
	if (isint(v)) return vgetint(v);
	_check_arg_tag(inter, elf_tag_Num, v.tag,x);
	return 0;
}


elf_pubapi
elf_Handle elf_tosys(elf_State *inter, int x) {
	elf_Value v = inter->stack[x];
	if (issys(v)) return vgetsys(v);
	_check_arg_tag(inter, elf_tag_Handle, v.tag, x);
	return 0;
}


elf_pubapi
const char *elf_tostr(elf_State *inter, int x) {
	elf_Value v = inter->stack[x];
	if (isstr(v)) return vgetstr(v)->text;
	if (isnil(v)) return 0;
	_check_arg_tag(inter, elf_tag_String, v.tag, x);
	return 0;
}



const char *tag2s[] = {
	[elf_tag_Nil] = "nil",
	[elf_tag_Tomb] = "tomb",
	[elf_tag_Num] = "num",
	[elf_tag_Int] = "int",
	[elf_tag_Handle] = "sysobj",
	[elf_tag_UserObject] = "userobj",
	[elf_tag_Function] = "Function",
	[elf_tag_Closure] = "Closure",
	[elf_tag_String] = "String",
	[elf_tag_Table] = "Table",
};


elf_pubapi
elf_State *elf_new() {
	elf_State *inter = calloc(1, sizeof(*inter));
	elf_init_raw(inter);
	return inter;
}


elf_pubapi
int elf_pushglobals(elf_State *S) {
	TOP(S)->tag = elf_tag_Table;
	TOP(S)->x_tab = S->globals;
	return incstackptr(S);
}

int elf_pushnil(elf_State *S) {
	TOP(S)->tag = elf_tag_Nil;
	TOP(S)->x_i64 = 0;
	return incstackptr(S);
}

int elf_pushint(elf_State *S, elf_Integer x) {
	TOP(S)->tag = elf_tag_Int;
	TOP(S)->x_int = x;
	return incstackptr(S);
}

int elf_pushnum(elf_State *S, elf_Number x) {
	TOP(S)->tag = elf_tag_Num;
	TOP(S)->x_num = x;
	return incstackptr(S);
}

int elf_pushsys(elf_State *S, elf_Handle x) {
	TOP(S)->tag = elf_tag_Handle;
	TOP(S)->x_sys = x;
	return incstackptr(S);
}


int elf_pushfun(elf_State *S, elf_Function x) {
	vsetfnc(S->stack_ptr, x);
	return incstackptr(S);
}


int elf_pushtab(elf_State *S) {
	TOP(S)->tag = elf_tag_Table;
	TOP(S)->x_tab = elf_alloc_table(S);
	return incstackptr(S);
}


elf_pubapi
int elf_pushstr(elf_State *inter, const char *text) {
	elf_String *str = elf_alloc_string(inter, text);
	vsetstr(inter->stack_ptr, str);
	return incstackptr(inter);
}


elf_pubapi
int elf_pushstrl(elf_State *inter, const char *text, int length) {
	elf_String *str = elf_alloc_string3(inter, text, length);
	vsetstr(inter->stack_ptr, str);
	return incstackptr(inter);
}


elf_pubapi
void elf_setfield(elf_State *inter) {
	elf_Value tab = inter->stack_ptr[-3];
	elf_Value key = inter->stack_ptr[-2];
	elf_Value val = inter->stack_ptr[-1];

	if (tab.tag != elf_tag_Table) {
		elf_error(inter, NO_BYTE, "Not A Table!");
	}

	elf_raw_table_set(vgettab(tab), key, val);
	inter->stack_ptr -= 2;
}


elf_pubapi
void elf_arrayadd(elf_State *inter) {
	elf_Value tab = inter->stack_ptr[-2];
	elf_Value val = inter->stack_ptr[-1];

	if (tab.tag != elf_tag_Table) {
		elf_error(inter, NO_BYTE, "Not A Table!");
	}

	elf_raw_array_add(vgettab(tab), val);
	inter->stack_ptr -= 1;
}

