//
// See Copyright Notice In elf.h
//









elf_Table *elf_new_table(elf_State *R) {
	elf_Table *table = elf_alloc_table(R);
	elf_push_table_raw(R, table);
	return table;
}



// todo: deprecate!
void elf_check_num_args(elf_State *S, char *name, int nargs, char *usage) {
	if ((nargs - 1) != nargs) {
		elf_error(S, S->byte, elf_tpf("'%s': expects %i argument(s), you gave %i, usage: %s", name, nargs, (nargs - 1), usage));
	}
}




static void _check_arg_tag(elf_State *S, elf_Tag tag, elf_Tag got, int index) {
	elf_error(S,NO_BYTE,elf_tpf("'%s': argument %i is invalid, expected '%s'",tag2s[got],index,tag2s[tag]));
}


elf_Value elf_get_arg(elf_State *S, int x) {
	return S->frame.framebase[x + 1];
}


elf_Object *elf_get_this(elf_State *S) {
	return S->frame.framebase[0].x_obj;
}


elf_rawapi
void elf_push_value_raw(elf_State *inter, elf_Value value) {
	// @stack_push
	* inter->stack_ptr ++ = value;
}


void elf_push_table_raw(elf_State *S, elf_Table *x) {
	PUSHV(S,VALUE_TABLE(x));
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
	if (tisobject(v.tag)) return v.x_obj;
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


elf_Handle elf_get_sysarg(elf_State *R, int x) {
	elf_Value v=elf_get_arg(R,x);
	if (v.tag==elf_tag_Nil) return 0;
	if (v.tag==elf_tag_Handle) return v.x_sys;
	_check_arg_tag(R,elf_tag_Handle,v.tag,x);
	return 0;
}

elf_Integer elf_get_intarg(elf_State *R, int x) {
	elf_Value v=elf_get_arg(R,x);
	if (v.tag==elf_tag_Num) return (elf_Integer) v.x_num;
	if (v.tag==elf_tag_Int) return v.x_int;
	_check_arg_tag(R,elf_tag_Int,v.tag,x);
	return 0;
}


