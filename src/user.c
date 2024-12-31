/*
** See Copyright Notice In elf.h
** user.c
*/


elf_String *elf_new_string(elf_Shell *R, const char *text) {
	elf_String *string=elf_alloc_string(R,text);
	elf_add_str(R,string);
	return string;
}


elf_String *elf_new_string2(elf_Shell *R, elf_Int length) {
	elf_String *string=elf_alloc_string2(R,length);
	elf_add_str(R,string);
	return string;
}


elf_Object *elf_new_object(elf_Shell *R, elf_Int size) {
	elf_Object *obj=elf_alloc_object(R,GC_OBJ,size);
	elf_add_obj(R,obj);
	return obj;
}


elf_Table *elf_new_table(elf_Shell *R) {
	elf_Table *tab=elf_alloc_table(R);
	elf_add_tab(R,tab);
	return tab;
}


elf_Closure *elf_new_closure(elf_Shell *R, elf_Function fn) {
	elf_Closure *cls=elf_alloc_closure(R,fn);
	elf_add_cls(R,cls);
	return cls;
}


static void check_tag(elf_Shell *S, elf_ValueTag tag, elf_ValueTag got, elf_StackId x) {
	elf_fail(S,NO_BYTE,elf_tpf("expected '%s' at local %i, instead got '%s'",tag2s[tag],x,tag2s[got]));
}


int elf_get_num_args(elf_Shell *S) {
	return GET_FRAME(S)->nargs-1;
}


elf_Value elf_get_arg(elf_Shell *S, int x) {
	return GET_FRAME(S)->locals[x+1];
}


elf_ValueTag elf_get_tag(elf_Shell *S, int x) {
	return GET_FRAME(S)->locals[x+1].tag;
}


elf_Object *elf_get_this(elf_Shell *S) {
	return GET_FRAME(S)->locals[0].x_obj;
}


void elf_add_this(elf_Shell *S) {
	PUSHV(S,GET_FRAME(S)->locals[0]);
}


void elf_add_any(elf_Shell *S, elf_Value value) {
	PUSHV(S,value);
}


void elf_add_nil(elf_Shell *S) {
	PUSHV(S,VNIL());
}


void elf_add_cls(elf_Shell *S, elf_Closure *x) {
	PUSHV(S,VCLS(x));
}


void elf_add_obj(elf_Shell *S, elf_Object *x) {
	if (x) PUSHV(S,VOBJ(x)); else PUSHV(S,VNIL());
}


void elf_add_cfn(elf_Shell *S, elf_CFunction x) {
	PUSHV(S,VCFN(x));
}


void elf_add_tab(elf_Shell *S, elf_Table *x) {
	PUSHV(S,VTAB(x));
}


void elf_add_int(elf_Shell *S, elf_Int x) {
	PUSHV(S,VINT(x));
}


void elf_add_num(elf_Shell *S, elf_Num x) {
	PUSHV(S,VNUM(x));
}


void elf_add_str(elf_Shell *S, elf_String *x) {
	PUSHV(S,VSTR(x));
}


void elf_add_sys(elf_Shell *S, elf_Handle x) {
	PUSHV(S,VSYS(x));
}


elf_String *elf_get_str(elf_Shell *R, elf_StackId x) {
	elf_Value v=elf_get_arg(R,x);
	if (v.tag==elf_TAG_NIL) return 0;
	if (v.tag==elf_TAG_STR) return v.x_str;
	check_tag(R,elf_TAG_STR,v.tag,x);
	return 0;
}


char *elf_get_txt(elf_Shell *R, elf_StackId x) {
	elf_Value v=elf_get_arg(R,x);
	if (v.tag==elf_TAG_NIL) return 0;
	if (v.tag==elf_TAG_STR) return v.x_str->text;
	check_tag(R,elf_TAG_STR,v.tag,x);
	return 0;
}


elf_Object *elf_get_obj(elf_Shell *R, elf_StackId x) {
	elf_Value v=elf_get_arg(R,x);
	if (v.tag==elf_TAG_NIL) return 0;
	if (ISOBJT(v.tag)) return v.x_obj;
	check_tag(R,elf_TAG_OBJ,v.tag,x);
	return 0;
}


elf_Table *elf_get_tab(elf_Shell *R, elf_StackId x) {
	elf_Value v=elf_get_arg(R,x);
	if (v.tag==elf_TAG_NIL) return 0;
	if (v.tag==elf_TAG_TAB) return v.x_tab;
	check_tag(R,elf_TAG_TAB,v.tag,x);
	return 0;
}


elf_Closure *elf_get_cls(elf_Shell *S, elf_StackId x) {
	elf_Value thing;
	thing=elf_get_arg(S,x);
	return thing.tag!=elf_TAG_CLS?0:thing.x_cls;
}


elf_Handle elf_get_sys(elf_Shell *R, elf_StackId x) {
	elf_Value v=elf_get_arg(R,x);
	if (v.tag==elf_TAG_NIL) return 0;
	if (v.tag==elf_TAG_SYS) return v.x_sys;
	check_tag(R,elf_TAG_SYS,v.tag,x);
	return 0;
}


elf_Int elf_get_int(elf_Shell *R, int x) {
	elf_Value v=elf_get_arg(R,x);
	if (v.tag==elf_TAG_NUM) return (elf_Int) v.x_num;
	if (v.tag==elf_TAG_INT) return v.x_int;
	check_tag(R,elf_TAG_INT,v.tag,x);
	return 0;
}


elf_Num elf_get_num(elf_Shell *R, elf_StackId x) {
	elf_Value v=elf_get_arg(R,x);
	if (v.tag==elf_TAG_INT) return (elf_Num) v.x_int;
	if (v.tag==elf_TAG_NUM) return v.x_num;
	check_tag(R,elf_TAG_NUM,v.tag,x);
	return 0;
}


elf_SymbolId elf_ggets(elf_Module *M, elf_String *name) {
	if (name != 0) return elf_tget_ornew(M->globals,VSTR(name));
	return ARRAY_GROW(M->globals->array,1);
}


elf_SymbolId elf_gsets(elf_Module *M, elf_String *name, elf_Value value) {
	elf_SymbolId id;
	id=elf_ggets(M,name);
	M->globals->array[id]=value;
	return id;
}


void elf_gsetx_sys(elf_Shell *R, char *name, elf_Handle val) {
	elf_gsets(R->M,elf_new_string(R,name),VSYS(val));
}


void elf_gsetx_int(elf_Shell *R, char *name, elf_Int val) {
	elf_gsets(R->M,elf_new_string(R,name),VINT(val));
}


void elf_gsetx_tab(elf_Shell *R, char *name, elf_Table *val) {
	elf_gsets(R->M,elf_new_string(R,name),VTAB(val));
}


void elf_gsetx_str(elf_Shell *R, char *name, char *val) {
	elf_gsets(R->M,elf_new_string(R,name),VSTR(elf_new_string(R,val)));
}


void elf_gsetx_cfn(elf_Shell *R, char *name, elf_CFunction fn) {
	elf_gsets(R->M,elf_new_string(R,name),VCFN(fn));
}


void elf_gset_bindings(elf_Shell *R, elf_CBinding *list, int num) {
	elf_tsetx_bindings(R,R->M->globals,list,num);
}
