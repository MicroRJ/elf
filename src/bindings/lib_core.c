//
// See Copyright Notice In elf.h
//

//
// Exposes The Core Functionality Of elf
//




// todo:
static elf_String *elf_read_file_string(elf_State *R, char *name, FILE *io, int size, int pos);

static int core_lib_get_global(elf_State *R) {
	elf_String *name = elf_get_string(R, 0);

	int index = elf_get_global_slot(R, name);
	elf_push_any(R, R->globals->array[index]);
	return 1;
}

static int core_lib_const_expr(elf_State *R) {
	elf_String *contents = elf_get_string(R,0);

	int ret = 0;

	if (contents) {

		// elf_Parser parser = {};
		// c_parser_init(R, &parser, "no name", contents->text);
		// ret = c_parse_const(&parser);
		ret = elf_parse_const(R, "no name", contents->text);
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
	res = elf_exec(R,false,nargs,nrets,name,contents);

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
	nrets = elf_exec(R,true,nargs,nrets,name,contents);
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
	elf_push_int(R,(elf_Int) (void *) elf_get_object(R,0));
	return 1;
}

#if 0
int core_lib_parse_expr(elf_State *R) {
	elf_String *contents = elf_get_string(R, 0);
	elf_Parser parser = {};
	c_parser_init(&parser, R, "no name", contents->text);
	treeID v = parse_expr(&parser, 0);

	return 1;
}
#endif


/* merges one or several tables together into
a new table, which is then returned. */
int core_lib_merge_tables(elf_State *R) {
	elf_Table *tab = elf_new_table(R);
	int i;
	for (i = 0; i < elf_get_num_args(R); i += 1) {
		elf_table_merge(tab,elf_get_table(R,i));
	}
	return 1;
}


int core_lib_get_meta(elf_State *R) {
	elf_push_table(R,elf_get_object(R,0)->meta);
	return 1;
}


int core_lib_set_meta(elf_State *R) {
	elf_get_object(R,0)->meta=elf_get_table(R,1);
	PUSHV(R,elf_get_arg(R,0));
	return 1;
}


/*  */
int core_lib_abort(elf_State *R) {
	if(1) abort();
	return 0;
}


int core_lib_exit(elf_State *R) {
	if(1) exit(elf_get_int(R,0));
	return 0;
}


/* debugging */
int core_lib_flags(elf_State *R) {
	int flags = R->flags;
	R->flags |= elf_get_int(R,0);
	elf_push_int(R,flags);
	return 1;
}


int core_lib_debugger(elf_State *R) {
	char *message = "no message";
	if (elf_get_num_args(R) != 0) {
		message = elf_get_text(R,0);
	}
	elf_debugger(message);
	return 0;
}


int core_lib_log(elf_State *R) {
	FOR_RANGE(i,0,elf_get_num_args(R)) {
		fpf_value(stdout,elf_get_arg(R,i),0);
	}
	fprintf(stdout,"\n");
	return 0;
}


int core_lib_err_log(elf_State *R) {
	FOR_RANGE(i,0,elf_get_num_args(R)) {
		fpf_value(stderr,elf_get_arg(R,i),0);
	}
	fprintf(stderr,"\n");
	return 0;
}


static char *in_sym_dir(char *dir, char *sym) {
	do {
		if (*dir ++ != *sym ++) {
			return *sym == '.' ? dir : sym;
		}
	} while (*dir);
	return sym;
}


/* the dot is added so that symbols like elf.math.floor
are not mistaken with table accesses when shortened,
math.floor != .math.floor */
int core_lib_include(elf_State *R) {
	elf_check_num_args(R,".include",1,"(the directory to include to add to the global directory)");
	char *dir = elf_get_text(R,0);
	int plen = text_length(dir);
	/* accumulate all symbols here first to
	avoid faulting under repeating patterns:
	elf.ray.elf.ray could include the symbol
	many more times when the new key is added
	as we traverse the array. the new key is
	encountered and we keep repeating the
	process... this would override the previous
	value and result in erroneous behavior.
	todo: */

	elf_Table *globals = R->globals;
	elf_Table_Entry entry;
	FOR_RANGE(i,0,globals->ntotal) {
		entry=globals->slots[i];
		if (entry.key.tag == elf_tag_str) {
			char *sym = in_sym_dir(dir,entry.key.x_str->text);
			if (*sym != '.') continue;
			elf_String *ref = elf_alloc_string(R,sym);
			elf_table_set(globals,VALUE_STRING(ref),globals->array[entry.idx]);
		}
	}
	return 0;
}


int core_lib_load_json(elf_State *R) {
	char *name = elf_get_text(R,0);

	FILE *file = fopen(name,"rb");
	if (file == 0) goto _error;

	fseek(file,0,SEEK_END);
	long size = ftell(file);

	elf_String *contents = elf_new_string2(R,size);

	fseek(file,0,SEEK_SET);
	fread(contents->text,1,size,file);

	elf_Table * tab = elf_parse_json(R, name, contents->text);
	elf_push_table(R,tab);

	return 1;
	_error:
	elf_push_nil(R);
	return 1;
}



int core_lib_fpf(elf_State *S) {
	elf_Handle file = elf_get_sysobj(S,0);
	int wrote = 0;
	for (int i = 1; i < elf_get_num_args(S); i ++) {
		wrote += fpf_value(file,elf_get_arg(S,i),0);
	}
	elf_push_int(S,wrote);
	return 1;
}

#if 0
int core_lib_lpf(elf_State *S) {
	for (int i = 0; i < elf_get_num_args(S); i ++) {
		if (i != 0) fprintf(stdout,"\n");
		for (int j = 0; j < pf_indent; ++ j) {
			fprintf(stdout, "  ");
		}
		fpf_value(stdout,elf_get_arg(S,i),0);
	}
	fprintf(stdout,"\n");
	return 0;
}
#endif

int core_lib_pf(elf_State *S) {
	#if 0
	for (int j = 0; j < pf_indent; ++ j) {
		fprintf(stdout,"  ");
	}
	#endif
	for (int i = 0; i < elf_get_num_args(S); i ++) {
		fpf_value(stdout,elf_get_arg(S,i),0);
	}
	fprintf(stdout,"\n");
	return 0;
}


int core_lib_sleep(elf_State *S) {
	ASSERT(elf_get_num_args(S) >= 1);
	sys_sleep(elf_get_int(S,0));
	return 0;
}

static void print_num_tabs(FILE *io, int num) {
	while (num --) fprintf(io,"\t");
}

// todo: cyclic references will break this
void elf_unload(FILE *io, elf_Table *tab, int level) {
	fprintf(io,"{\n");
	level += 1;
	int nitems = 0;
	// todo:
	if(tab->nslots) {
		for (elf_i64 i = 0; i < tab->ntotal; ++ i) {
			elf_Table_Entry slot = tab->slots[i];
			if (slot.key.tag == elf_tag_nil) {
				continue;
			}
			elf_Value v = tab->array[slot.idx];
			if ((v.tag == elf_tag_closure) || (v.tag == elf_tag_function)) {
				continue;
			}
			if (nitems ++ != 0) fprintf(io,",\n");
			print_num_tabs(io,level);
			fpf_value(io,slot.key,1);
			fprintf(io," = ");
			if (v.tag == elf_tag_tab) {
				elf_unload(io,v.x_tab,level);
			} else {
				fpf_value(io,v,1);
			}
		}
	} else {
		FOR_ARRAY(i,tab->array) {
			elf_Value v = tab->array[i];
			if (i != 0) fprintf(io,",\n");
			print_num_tabs(io,level);
			if (v.tag==elf_tag_tab) {
				elf_unload(io,v.x_tab,level+1);
			} else {
				fpf_value(io,v,1);
			}
		}
	}
	fprintf(io,"\n");
	print_num_tabs(io,level-1);
	fprintf(io,"}");
}


int core_lib_unload(elf_State *S) {
	elf_Handle io = elf_get_sysobj(S,0);
	elf_Table *tab = elf_get_table(S,1);
	elf_unload(io,tab,0);
	return 0;
}


// DEPRECATED SHOULD BE INTRINSIC
int core_lib_iton(elf_State *R) {
	elf_Value v = elf_get_arg(R,0);
	if (v.tag==elf_tag_int) {
		elf_push_num(R,(elf_Num)v.x_int);
	} else elf_push_num(R,v.x_num);
	return 1;
}


// DEPRECATED SHOULD BE INTRINSIC
int core_lib_ntoi(elf_State *R) {
	elf_Value v = elf_get_arg(R,0);
	if (v.tag==elf_tag_num) {
		elf_push_int(R,(elf_Int)v.x_num);
	} else elf_push_int(R,v.x_int);
	return 1;
}