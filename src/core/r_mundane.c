//
// See Copyright Notice In elf.h
//
// mundane functions that get lumped together
//


elf_Value *elf_get_stack(elf_State *S) {
	return S->stack;
}

elf_Value *elf_get_stack_ptr(elf_State *S) {
	return S->stack_ptr;
}

static void _set_stack_ptr(elf_State *S, elf_Value *stack_ptr) {
	S->stack_ptr = stack_ptr;
}


// todo: deprecate!
void elf_check_num_args(elf_State *S, char *name, int nargs, char *usage) {
	if (elf_get_num_args(S) != nargs) {
		elf_error(S, S->byte, elf_tpf("'%s': expects %i argument(s), you gave %i, usage: %s", name, nargs, elf_get_num_args(S), usage));
	}
}

elf_Table *elf_new_table(elf_State *S) {
	elf_Table *tab = elf_alloc_table(S);
	elf_push_table(S, tab);
	return tab;
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

void elf_push_this(elf_State *S) {
	PUSHV(S,S->frame.locals[0]);
}

void elf_push_any(elf_State *S, elf_Value value) {
	PUSHV(S,value);
}

void elf_push_nil(elf_State *S) {
	PUSHV(S,VALUE_NIL());
}

void elf_push_closure(elf_State *S, elf_Closure *x) {
	PUSHV(S,VALUE_CLOSURE(x));
}

void elf_push_object(elf_State *S, elf_Object *x) {
	if (x) PUSHV(S,VALUE_OBJECT(x)); else PUSHV(S,VALUE_NIL());
}

void elf_push_function(elf_State *S, elf_Function x) {
	PUSHV(S,VALUE_FUNCTION(x));
}

void elf_push_table(elf_State *S, elf_Table *x) {
	PUSHV(S,VALUE_TABLE(x));
}


void elf_push_int(elf_State *S, elf_Int x) {
	PUSHV(S,VALUE_INTEGER(x));
}


void elf_push_num(elf_State *S, elf_Num x) {
	PUSHV(S,VALUE_NUMBER(x));
}


void elf_push_string(elf_State *S, elf_String *x) {
	if (x) PUSHV(S,VALUE_STRING(x)); else elf_push_nil(S);
}


void elf_push_handle(elf_State *S, elf_Handle x) {
	PUSHV(S,VALUE_HANDLE(x));
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
	if (IS_OBJ_TAG(v.tag)) return v.x_obj;
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


elf_Closure *elf_get_closure(elf_State *S, int x) {
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