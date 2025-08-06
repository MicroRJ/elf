//
// See Copyright Notice In elf.h
//

//
// todo: why does this file even exist! merge with internal.c
//


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


#define TOP(S) (S->stack_ptr)

elf_rawapi
inline elf_StkInt incstackptr(elf_State *S) {
	elf_StkInt index = S->stack_ptr ++ - S->stack;
	ASSERT(index < S->stack_max);
	return index;
}

static elf_String *stkstr(elf_State *inter, int index) {
	elf_Value *v = inter->stack_ptr + index;
	if (v->tag != elf_tag_Nil && v->tag != elf_tag_String) {
		elf_error(inter, NO_BYTE, "expected string");
	}
	return v->x_str;
}

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

int elf_get_num_args(elf_State *S) {
	return S->frame.nargs - 1;
}

int elf_get_num_rets(elf_State *S) {
	return S->frame.nrets;
}

elf_Tag elf_get_argtag(elf_State *S, int x) {
	return S->frame.framebase[x + 1].tag;
}

int elf_push_nil(elf_State *S) {
	TOP(S)->tag = elf_tag_Nil;
	TOP(S)->x_i64 = 0;
	return incstackptr(S);
}

int elf_push_int(elf_State *S, elf_Int x) {
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

int elf_push_globals(elf_State *S) {
	TOP(S)->tag = elf_tag_Table;
	TOP(S)->x_tab = S->globals;
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


elf_IndexInt elf_table_set(elf_State *S) {
	elf_Value value = * -- S->stack_ptr;
	elf_Value key = * -- S->stack_ptr;
	elf_Value *table = S->stack_ptr - 1;
	if (table->tag != elf_tag_Table) {
		elf_error(S, NO_BYTE, "Not A Table!");
	}
	return elf_raw_table_set(table->x_tab, key, value);
}

elf_IndexInt elf_array_add(elf_State *S) {
	elf_Value value = * -- S->stack_ptr;
	elf_Value *table = S->stack_ptr - 1;
	if (table->tag != elf_tag_Table) {
		elf_error(S, NO_BYTE, "Not A Table!");
	}
	return elf_array_add_k(table->x_tab, value);
}
