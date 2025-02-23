//
// See Copyright Notice In elf.h
// lib_meta.c
//

static int core_lib_get_global(elf_State *R) {
	elf_String *name=elf_get_string(R,0);
	int index=elf_get_global(R->M,name);
	elf_add_any(R,R->M->globals->array[index]);
	return 1;
}

static int core_lib_const_expr(elf_State *R) {
	elf_String *contents = elf_get_string(R,0);
	int ret=0;
	if(contents){
		elf_Parser parser = {};
		elf_prep_parser(&parser,R,"no name",contents->text);
		// the result is on the stack already
		ret=parse_const(&parser);
	}
	return ret < 0 ? 0 : ret;
}

int core_lib_load_file(elf_State *R) {
	elf_String *name, *contents;
	FILE *file;
	int auto_close = false;
	int pos = -1;
	int size = -1;
	int res = 0;
	if(elf_get_tag(R,0) == elf_tag_str) {
		name = elf_get_string(R,0);
		file = fopen(name->text,"rb");
		auto_close = true;
	} else {
		name = elf_new_string(R,"no name");
		file = (FILE *) elf_get_sysobj(R,0);
	}
	if (!file) {
		elf_error_log("'%s': failed to load file",name->text);
		goto esc;
	}

	if (elf_get_num_args(R) > 1) {
		size = elf_get_int(R,1);
		if (elf_get_num_args(R) > 2) {
			pos = elf_get_int(R,2);
		}
	}
	contents = elf_read_file_string(R,"noname",file,size,pos);

	int nargs = 1; // elf_get_num_args(R) - 1;
	int nrets = elf_get_num_rets(R);
	res = _exec(R,false,nargs,nrets,name,contents);

	if (auto_close) {
		fclose(file);
	}

	esc:
	return res;
}


int core_lib_load_expr(elf_State *R) {
	elf_String *name, *contents;
	int pos=-1,size=-1;

	if(elf_get_tag(R,0) == elf_tag_str) {
		name = elf_get_string(R,0);
		contents = elf_read_file_string(R,name->text,0,size,pos);
	} else {
		name = elf_new_string(R,"no name");
		contents = elf_read_file_string(R,0,(FILE *)elf_get_sysobj(R,0),size,pos);
	}

	int nargs = elf_get_num_args(R) - 1;
	int nrets = elf_get_num_rets(R);
	nrets = _exec(R,true,nargs,nrets,name,contents);
	return nrets;
}

int core_lib_tagof(elf_State *R) {
	elf_new_string(R,(char*)tag2s[elf_get_tag(R,0)]);
	return 1;
}

int core_lib_get_object_color(elf_State *R) {
	elf_Object *obj = elf_get_object(R,0);
	int color = obj->color;
	elf_new_string(R,
	color == GC_NOCOLLECT   ? "black" :
	color == GC_COLLECTABLE ? "white" : "other");
	return 1;
}


int core_lib_set_object_trap(elf_State *R) {
	// elf_Int set = elf_get_int(R,1);
	// OBJ_COLOR(elf_get_object(R,0)) = set ? elf_GC_TRAP : GC_COLLECTABLE;
	return 0;
}

int core_lib_get_object_address(elf_State *R) {
	elf_add_int(R,(elf_Int) (void *) elf_get_object(R,0));
	return 1;
}