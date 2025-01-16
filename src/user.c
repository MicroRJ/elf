/*
** See Copyright Notice In elf.h
** user.c
*/


elf_Node *elf_new_object(elf_State *R, elf_Int size) {
	elf_Node *obj=elf_alloc_object(R,GC_OBJ,size);
	elf_add_obj(R,obj);
	return obj;
}

elf_String *elf_new_string(elf_State *R, const char *text) {
	elf_String *string=elf_alloc_string(R,text);
	elf_push_string(R,string);
	return string;
}


elf_String *elf_new_string2(elf_State *R, elf_Int length) {
	elf_String *string=elf_alloc_string2(R,length);
	elf_push_string(R,string);
	return string;
}

elf_Table *elf_new_table(elf_State *R) {
	elf_Table *tab=elf_alloc_table(R);
	elf_push_table(R,tab);
	return tab;
}

elf_Closure *elf_new_closure(elf_State *R, elf_Proto fn) {
	elf_Closure *cls = elf_alloc_closure(R,fn);
	elf_push_closure(R,cls);
	return cls;
}


static void check_tag(elf_State *S, elf_tagenum tag, elf_tagenum got, elf_StackId x) {
	elf_fail(S,NO_BYTE,elf_tpf("expected '%s' at local %i, instead got '%s'",tag2s[tag],x,tag2s[got]));
}


int elf_get_num_args(elf_State *S) {
	return GET_FRAME(S)->nargs-1;
}
int elf_get_num_rets(elf_State *S) {
	return GET_FRAME(S)->nrets;
}


elf_Value elf_get_arg(elf_State *S, int x) {
	return GET_FRAME(S)->locals[x+1];
}


elf_tagenum elf_get_tag(elf_State *S, int x) {
	return GET_FRAME(S)->locals[x+1].tag;
}


elf_Node *elf_get_this(elf_State *S) {
	return GET_FRAME(S)->locals[0].x_obj;
}


void elf_push_this(elf_State *S) {
	PUSHV(S,GET_FRAME(S)->locals[0]);
}


void elf_push(elf_State *S, elf_Value value) {
	PUSHV(S,value);
}


void elf_push_nil(elf_State *S) {
	PUSHV(S,VNIL());
}


void elf_push_closure(elf_State *S, elf_Closure *x) {
	PUSHV(S,VCLS(x));
}


void elf_add_obj(elf_State *S, elf_Node *x) {
	if (x) PUSHV(S,VOBJ(x)); else PUSHV(S,VNIL());
}


void elf_push_function(elf_State *S, elf_Function x) {
	PUSHV(S,VCFN(x));
}


void elf_push_table(elf_State *S, elf_Table *x) {
	PUSHV(S,VTAB(x));
}


void elf_push_integer(elf_State *S, elf_Int x) {
	PUSHV(S,VINT(x));
}


void elf_push_number(elf_State *S, elf_Num x) {
	PUSHV(S,VNUM(x));
}


void elf_push_string(elf_State *S, elf_String *x) {
	if (x) PUSHV(S,VSTR(x)); else elf_push_nil(S);
}


void elf_add_sys(elf_State *S, elf_Handle x) {
	PUSHV(S,VSYS(x));
}


elf_String *elf_get_string(elf_State *R, elf_StackId x) {
	elf_Value v=elf_get_arg(R,x);
	if (v.tag==elf_tag_nil) return 0;
	if (v.tag==elf_tag_str) return v.x_str;
	check_tag(R,elf_tag_str,v.tag,x);
	return 0;
}


char *elf_get_text(elf_State *R, elf_StackId x) {
	elf_Value v=elf_get_arg(R,x);
	if (v.tag==elf_tag_nil) return 0;
	if (v.tag==elf_tag_str) return v.x_str->text;
	check_tag(R,elf_tag_str,v.tag,x);
	return 0;
}


elf_Node *elf_get_obj(elf_State *R, elf_StackId x) {
	elf_Value v=elf_get_arg(R,x);
	if (v.tag==elf_tag_nil) return 0;
	if (ISOBJT(v.tag)) return v.x_obj;
	check_tag(R,elf_tag_userobj,v.tag,x);
	return 0;
}


elf_Table *elf_get_table(elf_State *R, elf_StackId x) {
	elf_Value v=elf_get_arg(R,x);
	if (v.tag==elf_tag_nil) return 0;
	if (v.tag==elf_tag_tab) return v.x_tab;
	check_tag(R,elf_tag_tab,v.tag,x);
	return 0;
}


elf_Closure *elf_get_cls(elf_State *S, elf_StackId x) {
	elf_Value thing;
	thing=elf_get_arg(S,x);
	return thing.tag!=elf_tag_closure?0:thing.x_cls;
}


elf_Handle elf_get_sysobj(elf_State *R, elf_StackId x) {
	elf_Value v=elf_get_arg(R,x);
	if (v.tag==elf_tag_nil) return 0;
	if (v.tag==elf_tag_sysobj) return v.x_sys;
	check_tag(R,elf_tag_sysobj,v.tag,x);
	return 0;
}


elf_Int elf_get_int(elf_State *R, int x) {
	elf_Value v=elf_get_arg(R,x);
	if (v.tag==elf_tag_num) return (elf_Int) v.x_num;
	if (v.tag==elf_tag_int) return v.x_int;
	check_tag(R,elf_tag_int,v.tag,x);
	return 0;
}


elf_Num elf_get_num(elf_State *R, elf_StackId x) {
	elf_Value v=elf_get_arg(R,x);
	if (v.tag==elf_tag_int) return (elf_Num) v.x_int;
	if (v.tag==elf_tag_num) return v.x_num;
	check_tag(R,elf_tag_num,v.tag,x);
	return 0;
}


/* Todo: a bunch of weird variants, to be removed */
/* Todo: remove all this, the user can create their own utility
functions for all this, this just adds unnecessary bloat */
elAPI void elf_gset_bindings(elf_State *S, elf_CBinding *list, int num);
elAPI void elf_gsetx_cfn(elf_State *S, char *name, elf_Function thing);
elAPI void elf_gsetx_int(elf_State *S, char *name, elf_Int thing);
elAPI void elf_gsetx_tab(elf_State *S, char *name, elf_Table *thing);


void elf_gsetx_sys(elf_State *R, char *name, elf_Handle val) {
	elf_set_global(R->M,elf_new_string(R,name),VSYS(val));
}


void elf_gsetx_int(elf_State *R, char *name, elf_Int val) {
	elf_set_global(R->M,elf_new_string(R,name),VINT(val));
}


void elf_gsetx_tab(elf_State *R, char *name, elf_Table *val) {
	elf_set_global(R->M,elf_new_string(R,name),VTAB(val));
}


void elf_gsetx_str(elf_State *R, char *name, char *val) {
	elf_set_global(R->M,elf_new_string(R,name),VSTR(elf_new_string(R,val)));
}


void elf_gsetx_cfn(elf_State *R, char *name, elf_Function fn) {
	elf_set_global(R->M,elf_new_string(R,name),VCFN(fn));
}


void elf_gset_bindings(elf_State *R, elf_CBinding *list, int num) {
	elf_tsetx_bindings(R,R->M->globals,list,num);
}

elf_Int elf_tgetx_int(elf_Table *tab, char const *key, elf_Int or) {
	elf_Value val;
	val=elf_tgetx_any(tab,key);
	return val.tag!=elf_tag_nil?VN2I(val):or;
}


elf_Num elf_tgetx_num(elf_Table *tab, char const *key, elf_Num or) {
	elf_Value val;
	val=elf_tgetx_any(tab,key);
	return val.tag!=elf_tag_nil?VI2N(val):or;
}


elf_Table *elf_tgetx_tab(elf_Table *tab, char const *key, elf_Table *or) {
	elf_Value val;
	val=elf_tgetx_any(tab,key);
	return val.tag==elf_tag_tab?val.x_tab:or;
}


elf_Num elf_tgets_num(elf_Table *tab, elf_String *key) {
	elf_Value val;
	val=elf_tgets_any(tab,key);
	return VI2N(val);
}


elf_Int elf_tgets_int(elf_Table *tab, elf_String *key) {
	elf_Value val;
	val=elf_tgets_any(tab,key);
	return VN2I(val);
}


elf_Int elf_tgetsor_int(elf_Table *tab, elf_String *key, elf_Int or) {
	elf_Value val=elf_tgets_any(tab,key);
	if (val.tag!=elf_tag_nil) {
		return VN2I(val);
	} else return or;
}


elf_String *elf_tgets_str(elf_Table *tab, elf_String *key) {
	return elf_tgets_any(tab,key).x_str;
}


elf_Table *elf_tgets_tab(elf_Table *tab, elf_String *key) {
	return elf_tgets_any(tab,key).x_tab;
}


void elf_tsets_str(elf_Table *tab, elf_String *key, elf_String *val) {
	elf_table_set(tab,VSTR(key),VSTR(val));
}


void elf_tsets_int(elf_Table *tab, elf_String *key, elf_Int val) {
	elf_table_set(tab,VSTR(key),VINT(val));
}


void elf_tsets_num(elf_Table *tab, elf_String *key, elf_Num val) {
	elf_table_set(tab,VSTR(key),VNUM(val));
}


void elf_tsets_tab(elf_Table *tab, elf_String *key, elf_Table *val) {
	elf_table_set(tab,VSTR(key),VTAB(val));
}


void elf_tsetx_bindings(elf_State *R, elf_Table *tab, elf_CBinding *list, int num) {
	FOR_RANGE(i,0,num) {
		elf_table_set(tab,VSTR(elf_alloc_string(R,list[i].name)),VCFN(list[i].fn));
	}
}
