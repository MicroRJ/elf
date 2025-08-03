//
// See Copyright Notice In elf.h
//



elf_Table *elf_new_table(elf_State *R) {
	elf_Table *table = elf_alloc_table(R);
	elf_push_table_raw(R, table);
	return table;
}



elf_String *elf_new_string(elf_State *S, const char *text) {
	elf_String *string = elf_alloc_string(S, text);
	elf_push_string_raw(S, string);
	return string;
}


elf_String *elf_new_string2(elf_State *R, elf_i32 length) {
	elf_String *string = elf_alloc_string2(R,length);
	elf_push_string_raw(R,string);
	return string;
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




static void _check_arg_tag(elf_State *S, elf_Tag tag, elf_Tag got, int index) {
	elf_error(S,NO_BYTE,elf_tpf("'%s': argument %i is invalid, expected '%s'",tag2s[got],index,tag2s[tag]));
}


elf_Value elf_get_arg(elf_State *S, int x) {
	return S->frame.locals[x + 1];
}


elf_Object *elf_get_this(elf_State *S) {
	return S->frame.locals[0].x_obj;
}



void elf_push_value_raw(elf_State *S, elf_Value value) {
	PUSHV(S,value);
}


void elf_push_closure_raw(elf_State *S, elf_Closure *x) {
	PUSHV(S,VALUE_CLOSURE(x));
}

void elf_push_object_raw(elf_State *S, elf_Object *x) {
	if (x) PUSHV(S,VALUE_OBJECT(x)); else PUSHV(S,VALUE_NIL());
}



void elf_push_table_raw(elf_State *S, elf_Table *x) {
	PUSHV(S,VALUE_TABLE(x));
}



// todo: @deprecated!
void elf_push_string_raw(elf_State *S, elf_String *x) {
	if (x) PUSHV(S,VALUE_STRING(x)); else elf_push_nil(S);
}




elf_String *elf_get_string_arg(elf_State *R, int x) {
	elf_Value v=elf_get_arg(R,x);
	if (v.tag==elf_tag_Nil) return 0;
	if (v.tag==elf_tag_String) return v.x_str;
	_check_arg_tag(R,elf_tag_String,v.tag,x);
	return 0;
}


char *elf_get_text_from_string_on_stack(elf_State *inter, int stk) {
	elf_Value v = inter->stack[stk];
	if (v.tag == elf_tag_Nil) return 0;
	if (v.tag == elf_tag_String) return v.x_str->text;
	_check_arg_tag(inter, elf_tag_String, v.tag, stk);
	return 0;
}

char *elf_get_text_arg(elf_State *R, int x) {
	elf_Value v = elf_get_arg(R,x);
	if (v.tag == elf_tag_Nil) return 0;
	if (v.tag == elf_tag_String) return v.x_str->text;
	_check_arg_tag(R,elf_tag_String,v.tag,x);
	return 0;
}


elf_Object *elf_get_object_arg_raw(elf_State *R, int x) {
	elf_Value v=elf_get_arg(R,x);
	if (v.tag==elf_tag_Nil) return 0;
	if (IS_OBJ_TAG(v.tag)) return v.x_obj;
	_check_arg_tag(R,elf_tag_UserObject,v.tag,x);
	return 0;
}


elf_Table *elf_get_table(elf_State *R, int x) {
	elf_Value v=elf_get_arg(R,x);
	if (v.tag==elf_tag_Nil) return 0;
	if (v.tag==elf_tag_Table) return v.x_tab;
	_check_arg_tag(R,elf_tag_Table,v.tag,x);
	return 0;
}


elf_Closure *elf_get_closure(elf_State *S, int x) {
	elf_Value thing;
	thing=elf_get_arg(S,x);
	return thing.tag!=elf_tag_Closure?0:thing.x_closure;
}


elf_Handle elf_get_sysarg(elf_State *R, int x) {
	elf_Value v=elf_get_arg(R,x);
	if (v.tag==elf_tag_Nil) return 0;
	if (v.tag==elf_tag_Handle) return v.x_sys;
	_check_arg_tag(R,elf_tag_Handle,v.tag,x);
	return 0;
}


elf_Int elf_get_intarg(elf_State *R, int x) {
	elf_Value v=elf_get_arg(R,x);
	if (v.tag==elf_tag_Num) return (elf_Int) v.x_num;
	if (v.tag==elf_tag_Int) return v.x_int;
	_check_arg_tag(R,elf_tag_Int,v.tag,x);
	return 0;
}


elf_Num elf_get_numarg(elf_State *R, int x) {
	elf_Value v=elf_get_arg(R,x);
	if (v.tag==elf_tag_Int) return (elf_Num) v.x_int;
	if (v.tag==elf_tag_Num) return v.x_num;
	_check_arg_tag(R,elf_tag_Num,v.tag,x);
	return 0;
}