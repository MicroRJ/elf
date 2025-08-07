//
// See Copyright Notice In elf.h
//

//
// todo: why does this file even exist! merge with internal.c
//

#define TOP(S) (S->stack_ptr)

elf_rawapi
inline elf_StkInt incstackptr(elf_State *S) {
	elf_StkInt index = S->stack_ptr ++ - S->stack;
	ASSERT(index < S->stack_max);
	return index;
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



static elf_String *stkstr(elf_State *inter, int index) {
	elf_Value *v = inter->stack_ptr + index;
	if (v->tag != elf_tag_Nil && v->tag != elf_tag_String) {
		elf_error(inter, NO_BYTE, "expected string");
	}
	return v->x_str;
}

// todo: remove, this function is retarded!
// return value is number of returns, if negative, errors occurred
int elf_exec(elf_State *inter, int nargs, int nrets, bool asexpr) {
	elf_String *name = stkstr(inter, -2);
	elf_String *contents = stkstr(inter, -1);
	if (!name || !contents) {
		elf_push_nil(inter);
		return 1;
	}
	return elf_raw_exec(inter, nargs, nrets, asexpr, name, contents);
}


elf_State *elf_new() {
	elf_State *inter = calloc(1, sizeof(*inter));
	elf_init_raw(inter);
	return inter;
}



int elf_push_globals(elf_State *S) {
	TOP(S)->tag = elf_tag_Table;
	TOP(S)->x_tab = S->globals;
	return incstackptr(S);
}

int elf_push_nil(elf_State *S) {
	TOP(S)->tag = elf_tag_Nil;
	TOP(S)->x_i64 = 0;
	return incstackptr(S);
}

int elf_push_int(elf_State *S, elf_Integer x) {
	TOP(S)->tag = elf_tag_Int;
	TOP(S)->x_int = x;
	return incstackptr(S);
}

int elf_push_num(elf_State *S, elf_Number x) {
	TOP(S)->tag = elf_tag_Num;
	TOP(S)->x_num = x;
	return incstackptr(S);
}

int elf_push_handle(elf_State *S, elf_Handle x) {
	TOP(S)->tag = elf_tag_Handle;
	TOP(S)->x_sys = x;
	return incstackptr(S);
}


int elf_push_this(elf_State *S) {
	*TOP(S) = S->frame.framebase[0];
	return incstackptr(S);
}

int elf_push_function(elf_State *S, elf_Function x) {
	TOP(S)->tag = elf_tag_Function;
	TOP(S)->x_proc = x;
	return incstackptr(S);
}

int elf_push_table(elf_State *S) {
	TOP(S)->tag = elf_tag_Table;
	TOP(S)->x_tab = elf_alloc_table(S);
	return incstackptr(S);
}

elf_pubapi
int elf_push_string(elf_State *inter, const char *text) {
	elf_String *str = elf_alloc_string(inter, text);
	vsetstr(inter->stack_ptr, str);
	return incstackptr(inter);
}

elf_pubapi
int elf_push_string3(elf_State *inter, const char *text, int length) {
	elf_String *str = elf_alloc_string3(inter, text, length);
	vsetstr(inter->stack_ptr, str);
	return incstackptr(inter);
}
