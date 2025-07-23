//
// See Copyright Notice In elf.h
//
// todo: this could all be
// in core.c

void elf_check_num_args(elf_State *S, char *name, int nargs, char *usage) {
	if (elf_get_num_args(S) != nargs) {
		elf_error(S, S->byte, elf_tpf("'%s': expects %i argument(s), you gave %i, usage: %s", name, nargs, elf_get_num_args(S), usage));
	}
}

elf_Object *elf_new_object(elf_State *S, elf_i32 size) {
	elf_Object *obj = elf_alloc_object(S, GC_OBJ, size);
	elf_add_object(S, obj);
	return obj;
}

elf_String *elf_new_string(elf_State *S, const char *text) {
	elf_String *string = elf_alloc_string(S, text);
	elf_push_string(S, string);
	return string;
}


elf_String *elf_new_string2(elf_State *R, elf_i32 length) {
	elf_String *string = elf_alloc_string2(R,length);
	elf_push_string(R,string);
	return string;
}

elf_Table *elf_new_table(elf_State *S) {
	elf_Table *tab = elf_alloc_table(S);
	elf_add_table(S, tab);
	return tab;
}

elf_Closure *elf_new_closure(elf_State *R, elf_Proto fn) {
	elf_Closure *cls = elf_alloc_closure(R,fn);
	elf_add_closure(R,cls);
	return cls;
}


static void _check_arg_tag(elf_State *S, elf_tag_enum tag, elf_tag_enum got, int index) {
	elf_error(S,NO_BYTE,elf_tpf("'%s': argument %i is invalid, expected '%s'",tag2s[got],index,tag2s[tag]));
}

int elf_get_num_args(elf_State *S) {
	return S->frame.nargs - 1;
}

int elf_get_num_rets(elf_State *S) {
	return S->frame.nrets;
}

elf_Value elf_get_arg(elf_State *S, int x) {
	return S->frame.locals[x + 1];
}

elf_tag_enum elf_get_tag(elf_State *S, int x) {
	return S->frame.locals[x + 1].tag;
}

elf_Object *elf_get_this(elf_State *S) {
	return S->frame.locals[0].x_obj;
}

void elf_add_this(elf_State *S) {
	PUSHV(S,S->frame.locals[0]);
}

void elf_add_any(elf_State *S, elf_Value value) {
	PUSHV(S,value);
}

void elf_add_nil(elf_State *S) {
	PUSHV(S,VNIL());
}

void elf_add_closure(elf_State *S, elf_Closure *x) {
	PUSHV(S,VCLS(x));
}

void elf_add_object(elf_State *S, elf_Object *x) {
	if (x) PUSHV(S,VOBJ(x)); else PUSHV(S,VNIL());
}

void elf_add_proc(elf_State *S, elf_Function x) {
	PUSHV(S,VALUE_FUNCTION(x));
}

void elf_add_table(elf_State *S, elf_Table *x) {
	PUSHV(S,VALUE_TABLE(x));
}


void elf_add_int(elf_State *S, elf_Int x) {
	PUSHV(S,VALUE_INTEGER(x));
}


void elf_add_num(elf_State *S, elf_Num x) {
	PUSHV(S,VALUE_NUMBER(x));
}


void elf_push_string(elf_State *S, elf_String *x) {
	if (x) PUSHV(S,VALUE_STRING(x)); else elf_add_nil(S);
}


void elf_add_sys(elf_State *S, elf_Handle x) {
	PUSHV(S,VSYS(x));
}


elf_String *elf_get_string(elf_State *R, int x) {
	elf_Value v=elf_get_arg(R,x);
	if (v.tag==elf_tag_nil) return 0;
	if (v.tag==elf_tag_str) return v.x_str;
	_check_arg_tag(R,elf_tag_str,v.tag,x);
	return 0;
}


char *elf_get_text(elf_State *R, int x) {
	elf_Value v = elf_get_arg(R,x);
	if (v.tag == elf_tag_nil) return 0;
	if (v.tag == elf_tag_str) return v.x_str->text;
	_check_arg_tag(R,elf_tag_str,v.tag,x);
	return 0;
}


elf_Object *elf_get_object(elf_State *R, int x) {
	elf_Value v=elf_get_arg(R,x);
	if (v.tag==elf_tag_nil) return 0;
	if (ISOBJT(v.tag)) return v.x_obj;
	_check_arg_tag(R,elf_tag_userobj,v.tag,x);
	return 0;
}


elf_Table *elf_get_table(elf_State *R, int x) {
	elf_Value v=elf_get_arg(R,x);
	if (v.tag==elf_tag_nil) return 0;
	if (v.tag==elf_tag_tab) return v.x_tab;
	_check_arg_tag(R,elf_tag_tab,v.tag,x);
	return 0;
}


elf_Closure *elf_get_cls(elf_State *S, int x) {
	elf_Value thing;
	thing=elf_get_arg(S,x);
	return thing.tag!=elf_tag_closure?0:thing.x_closure;
}


elf_Handle elf_get_sysobj(elf_State *R, int x) {
	elf_Value v=elf_get_arg(R,x);
	if (v.tag==elf_tag_nil) return 0;
	if (v.tag==elf_tag_sysobj) return v.x_sys;
	_check_arg_tag(R,elf_tag_sysobj,v.tag,x);
	return 0;
}


elf_Int elf_get_int(elf_State *R, int x) {
	elf_Value v=elf_get_arg(R,x);
	if (v.tag==elf_tag_num) return (elf_Int) v.x_num;
	if (v.tag==elf_tag_int) return v.x_int;
	_check_arg_tag(R,elf_tag_int,v.tag,x);
	return 0;
}


elf_Num elf_get_num(elf_State *R, int x) {
	elf_Value v=elf_get_arg(R,x);
	if (v.tag==elf_tag_int) return (elf_Num) v.x_int;
	if (v.tag==elf_tag_num) return v.x_num;
	_check_arg_tag(R,elf_tag_num,v.tag,x);
	return 0;
}