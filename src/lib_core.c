/*
** See Copyright Notice In elf.h
** lib_core.c
*/


static int core_lib_const_expr(elf_State *R) {
	elf_String *contents = elf_get_string(R,0);
	elf_Parser parser = {};
	prep_parser(&parser,R,"no name",contents->text);
	int ret = parse_const(&parser);
	return ret < 0 ? 0 : ret;
}

static elf_String *_read_file_io(elf_State *R, FILE *io) {
	fseek(io,0,SEEK_END);
	elf_i64 size = ftell(io);
	elf_String *contents = elf_new_string2(R,size);
	fseek(io,0,SEEK_SET);
	fread(contents->text,1,size,io);
	return contents;
}

static int _write_file_io(elf_State *R, FILE *io, char *text) {
	return fwrite(text,1,strlen(text),io);
}

static elf_String *_read_file(elf_State *R, char *name) {
	elf_String *contents = 0;
	FILE *file = fopen(name,"rb");
	if (file) {
		contents = _read_file_io(R,file);
		fclose(file);
		elf_debug_log("'%s': file read successfully",name);
	} else {
		elf_error_log("'%s': could not read",name);
	}
	return contents;
}

static int _write_file(elf_State *R, char *name, char *text) {
	FILE *file = fopen(name,"wb");
	int wrote = 0;
	if (file) {
		wrote = _write_file_io(R,file,text);
		fclose(file);
		elf_debug_log("'%s': file wrote successfully",name);
	} else {
		elf_error_log("'%s': could not write",name);
	}
	return wrote;
}

static int _call_file(elf_State *R, bool as_expr, int nargs, int nrets, elf_String *name) {
	elf_String *contents = _read_file(R,name->text);
	if(!contents) {
		nrets = 0;
		goto esc;
	}

	// todo: parse and gen file should just be one function,
	// or maybe just have the one 'parse' function...
	elf_Parser parser = {};
	treeID tree = parse(&parser,R,as_expr,name->text,contents->text);
	elf_File file = gen_file(&parser,tree);

	// todo: we need this in the file for debugging...
	file.contents = contents;
	file.name = name;

	// todo: how do we track this, should each proto
	// point to the file they are from?...
	elf_array_add(R->M->globals,VSTR(contents));
	elf_array_add(R->M->globals,VSTR(name));

	ARRAY_ADD(R->M->files,file);
	elf_Closure *cls = elf_new_closure(R,file.proto);

	// ASSERT(nargs >= 0);
	// ASSERT(nrets >= 0);
	/* todo: HACK!
	Todo: the file closure doesn't have to be kept alive... */
	elf_set_global(R->M,0,VCLS(cls));
	elf_Value *rets = R->stack_ptr;
	elf_push_closure(R,cls);
	elf_push_this(R);
	nrets = elf_call(R,nargs+1,nrets);
	R->stack_ptr = rets + nrets;
	esc:
	return nrets;
}

int core_lib_load_file(elf_State *R) {
	elf_String *name = elf_get_string(R,0);
	int nargs = elf_get_num_args(R) - 1;
	int nrets = elf_get_num_rets(R);
	nrets = _call_file(R,false,nargs,nrets,name);
	return nrets;
}


int core_lib_load_expr(elf_State *R) {
	elf_String *name = elf_get_string(R,0);
	int nargs = elf_get_num_args(R) - 1;
	int nrets = elf_get_num_rets(R);
	nrets = _call_file(R,true,nargs,nrets,name);
	return nrets;
}



// mostly experimental...
int core_lib_float2(elf_State *R) {
	elf_Value v={elf_tag_float2};
	v.x_f32=(float)elf_get_num(R,0);
	v.y_f32=v.x_f32;
	if (elf_get_num_args(R)>1){
		v.y_f32=(float)elf_get_num(R,1);
	}
	*R->stack_ptr++=v;
	return 1;
}


int core_lib_pause_collector(elf_State *R) {
	R->collector.paused = 1;
	if (elf_get_num_args(R) == 1) {
		R->collector.paused = elf_get_int(R,0) != 0;
	}
	return 0;
}


int core_lib_get_allocated_objects(elf_State *R) {
	elf_push_integer(R,ARRAY_LENGTH(R->collector.objects));
	return 1;
}


int core_lib_get_allocated_memory(elf_State *R) {
	elf_push_integer(R,R->collector.memory_allocated);
	return 1;
}


int core_lib_get_collector_threshold(elf_State *R) {
	elf_push_integer(R,R->collector.memory_threshold);
	return 1;
}


int core_lib_mark_object(elf_State *R) {
	elf_Int num = elf_mark_object(elf_get_obj(R,0));
	elf_push_integer(R,num);
	return 1;
}


int core_lib_collect(elf_State *R) {
	elf_trigger_collection_cycle(R);
	return 0;
}


int core_lib_tagof(elf_State *R) {
	elf_new_string(R,(char*)tag2s[elf_get_tag(R,0)]);
	return 1;
}


int core_lib_get_object_color(elf_State *R) {
	elf_Object *obj = elf_get_obj(R,0);
	int color = obj->color;
	elf_new_string(R,
	color == elf_GC_BLACK ? "black" :
	color == elf_GC_WHITE ? "white" :
	color == elf_GC_PINK  ? "pink"  :
	color == elf_GC_RED   ? "red"   :
	color == elf_GC_TRAP  ? "trap"  : "error");
	return 1;
}


int core_lib_set_object_trap(elf_State *R) {
	elf_Int set = elf_get_int(R,1);
	OBJ_COLOR(elf_get_obj(R,0)) = set ? elf_GC_TRAP : elf_GC_WHITE;
	return 0;
}


int core_lib_get_object_address(elf_State *R) {
	elf_push_integer(R,(elf_Int) (void *) elf_get_obj(R,0));
	return 1;
}


/* merges one or several tables together into
a new table, which is then returned. */
int core_lib_merge_tables(elf_State *R) {
	elf_Table *tab = elf_new_table(R);
	int i;
	for (i = 0; i < elf_get_num_args(R); i += 1) {
		elf_merge_tables(tab,elf_get_table(R,i));
	}
	return 1;
}


int core_lib_get_meta(elf_State *R) {
	elf_push_table(R,elf_get_obj(R,0)->meta);
	return 1;
}


int core_lib_set_meta(elf_State *R) {
	elf_get_obj(R,0)->meta=elf_get_table(R,1);
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
	elf_push_integer(R,flags);
	return 1;
}


int core_lib_debugger(elf_State *R) {
#if defined(_DEBUG)
	R->flags |= FLAG_DEBUGGER;
#else
	char *message = "no message";
	if (elf_get_num_args(R) != 0) {
		message = elf_get_text(R,0);
	}
	elf_debugger(message);
#endif
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
	elf_check_args(R,".include",1,"(the directory to include to add to the global directory)");
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

	elf_Table *globals = R->M->globals;
	elf_Entry entry;
	FOR_RANGE(i,0,globals->ntotal) {
		entry=globals->slots[i];
		if (entry.key.tag == elf_tag_str) {
			char *sym = in_sym_dir(dir,entry.key.x_str->text);
			if (*sym != '.') continue;
			elf_String *ref = elf_alloc_string(R,sym);
			elf_table_set(globals,VSTR(ref),globals->array[entry.idx]);
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

	elf_Parser parser = {};
	prep_parser(&parser, R, name, contents->text);

	elf_tabID tab = parse_json_obj(&parser);
	elf_push_table(R,tab);

	return 1;
	_error:
	elf_push_nil(R);
	return 1;
}

int core_lib_open_temp_file(elf_State *R) {
	FILE *file = {0};
#if defined(PLATFORM_WEB)
	file = tmpfile();
#else
	tmpfile_s(&file);
#endif
	elf_add_sys(R,(elf_Handle)file);
	return 1;
}

int core_lib_open_file(elf_State *R) {
	ASSERT(elf_get_num_args(R) == 2);
	char *name = elf_get_text(R,0);
	char *flags = elf_get_text(R,1);
	FILE *file = fopen(name,flags);
	elf_add_sys(R,(elf_Handle)file);
	return 1;
}

int core_lib_close_file(elf_State *R) {
	ASSERT(elf_get_num_args(R) == 1);
	elf_Handle file = elf_get_sysobj(R,0);
	if(file==0) elf_fail(R,NO_BYTE,"invalid argument");
	fclose(file);
	return 0;
}

int core_lib_get_file_size(elf_State *R) {
	FILE *file;
	if(elf_get_tag(R,0) == elf_tag_str) {
		file = fopen(elf_get_text(R,0),"rb");
	} else {
		file = (FILE *) elf_get_sysobj(R,0);
	}
	fseek(file,0,SEEK_END);
	int size = ftell(file);
	elf_push_integer(R,size);
	return 1;
}

int core_lib_read_file(elf_State *R) {
	elf_String *contents = 0;
	if(elf_get_tag(R,0) == elf_tag_str) {
		contents = _read_file(R,elf_get_text(R,0));
	} else {
		contents = _read_file_io(R,elf_get_sysobj(R,0));
	}
	elf_push_string(R,contents);
	return 1;
}

int core_lib_write_file(elf_State *R) {
	if(elf_get_tag(R,0) == elf_tag_str) {
		_write_file(R,elf_get_text(R,0),elf_get_text(R,1));
	} else {
		_write_file_io(R,elf_get_sysobj(R,0),elf_get_text(R,1));
	}
	return 0;
}

int core_lib_write_file_to_file(elf_State *R) {
	FILE *dst,*src;
	if(elf_get_tag(R,0) == elf_tag_str) {
		dst = fopen(elf_get_text(R,0),"wb");
	} else {
		dst = (FILE *) elf_get_sysobj(R,0);
	}
	if(elf_get_tag(R,1) == elf_tag_str) {
		src = fopen(elf_get_text(R,1),"rb");
	} else {
		src = (FILE *) elf_get_sysobj(R,1);
	}
	char buffer[4096];
	int read,wrote = 0;
	do {
		read = fread(buffer,1,sizeof(buffer),src);
		wrote += fwrite(buffer,1,read,dst);
	} while(read > 0);

	elf_push_integer(R,wrote);
	return 1;
}



int core_lib_change_work_dir(elf_State *R) {
	int ok = sys_set_work_dir(elf_get_text(R,0));
	elf_push_integer(R,ok);
	return 1;
}


int core_lib_get_work_dir(elf_State *R) {
	char buf[MAX_PATH];
	sys_get_work_dir(sizeof(buf),buf);
	elf_new_string(R,buf);
	return 1;
}


int core_lib_fpf(elf_State *S) {
	elf_Handle file = elf_get_sysobj(S,0);
	int wrote = 0;
	for (int i = 1; i < elf_get_num_args(S); i ++) {
		wrote += fpf_value(file,elf_get_arg(S,i),0);
	}
	elf_push_integer(S,wrote);
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

static elf_f64 _time_diff_s(elf_i64 time) {
	return (sys_get_clock_time() - time) / (elf_f64) sys_get_clock_freq();
}

int core_lib_clocktime(elf_State *R) {
	elf_push_integer(R,sys_get_clock_time());
	return 1;
}

int core_lib_timediffs(elf_State *S) {
	ASSERT(elf_get_num_args(S) == 1);
	elf_Int time = elf_get_int(S,0);
	elf_push_number(S,_time_diff_s(time));
	return 1;
}

int core_lib_timediffms(elf_State *S) {
	ASSERT(elf_get_num_args(S) == 1);
	elf_Int time = elf_get_int(S,0);
	elf_push_number(S,_time_diff_s(time) * 1000);
	return 1;
}

static void print_num_tabs(FILE *io, int num) {
	while (num --) fprintf(io,"\t");
}

// todo: move this to core.c?
void elf_unload(FILE *io, elf_Table *tab, int level) {
	fprintf(io,"{");
	int nitems = 0;
	// todo:
	if(tab->nslots) {
		for (elf_i64 i = 0; i < tab->ntotal; ++ i) {
			elf_Entry slot = tab->slots[i];
			if (slot.key.tag == elf_tag_nil) {
				continue;
			}
			elf_Value v = tab->array[slot.idx];
			if ((v.tag == elf_tag_closure) || (v.tag == elf_tag_proc)) {
				continue;
			}
			if (nitems ++ != 0) fprintf(io,",");
			fpf_value(io,slot.key,1);
			fprintf(io," = ");
			if (v.tag == elf_tag_tab) {
				elf_unload(io,v.x_tab,level+1);
			} else {
				fpf_value(io,v,1);
			}
		}
	} else {
		FOR_ARRAY(i,tab->array) {
			elf_Value v = tab->array[i];
			if (i != 0) fprintf(io,", ");
			if (v.tag==elf_tag_tab) {
				elf_unload(io,v.x_tab,level+1);
			} else {
				fpf_value(io,v,1);
			}
		}
	}
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
		elf_push_number(R,(elf_Num)v.x_int);
	} else elf_push_number(R,v.x_num);
	return 1;
}


// DEPRECATED SHOULD BE INTRINSIC
int core_lib_ntoi(elf_State *R) {
	elf_Value v = elf_get_arg(R,0);
	if (v.tag==elf_tag_num) {
		elf_push_integer(R,(elf_Int)v.x_num);
	} else elf_push_integer(R,v.x_int);
	return 1;
}


#if 0
void elf_include_core_lib(elf_State *R) {
	elf_gsetx_int(R,"elf.VERSION",0);
#if defined(PLATFORM_WEB)
	elf_gsetx_str(R,"elf.PLATFORM","WEB");
	elf_gsetx_str(R,"elf.OS","UNKNOWN");
#else
	elf_gsetx_str(R,"elf.PLATFORM","DESKTOP");
	#if defined(_WIN32)
	elf_gsetx_str(R,"elf.OS","WINDOWS");
	#else
	elf_gsetx_str(R,"elf.OS","UNKNOWN");
	#endif
#endif

	elf_gsetx_cfn(R,"elf.flags",core_lib_flags);
	elf_gsetx_cfn(R,"elf.debugger",core_lib_debugger);

	elf_gsetx_cfn(R,"elf.merge_tables",core_lib_merge_tables);

	elf_gsetx_cfn(R,"elf.pause_collector",core_lib_pause_collector);
	elf_gsetx_cfn(R,"elf.mark_object",core_lib_mark_object);
	elf_gsetx_cfn(R,"elf.get_collector_threshold",core_lib_get_collector_threshold);
	elf_gsetx_cfn(R,"elf.get_allocated_objects",core_lib_get_allocated_objects);
	elf_gsetx_cfn(R,"elf.get_allocated_memory",core_lib_get_allocated_memory);
	elf_gsetx_cfn(R,"elf.collect",core_lib_collect);
	elf_gsetx_cfn(R,"elf.tagof",core_lib_tagof);

	elf_gsetx_cfn(R,"elf.set_meta",core_lib_set_meta);
	elf_gsetx_cfn(R,"elf.get_meta",core_lib_get_meta);
	elf_gsetx_cfn(R,"elf.get_object_address",core_lib_get_object_address);
	elf_gsetx_cfn(R,"elf.get_object_color",core_lib_get_object_color);
	elf_gsetx_cfn(R,"elf.set_object_trap",core_lib_set_object_trap);


	elf_gsetx_cfn(R,"elf.log",core_lib_log);
	elf_gsetx_cfn(R,"elf.err",core_lib_err_log);

	/* todo: these should be intrinsic */
	elf_gsetx_cfn(R,"ntoi",core_lib_ntoi);
	elf_gsetx_cfn(R,"iton",core_lib_iton);

	elf_gsetx_cfn(R,"elf.clocktime",core_lib_clocktime);
	elf_gsetx_cfn(R,"elf.timediffs",core_lib_timediffs);
	elf_gsetx_cfn(R,"elf.timediffms",core_lib_timediffms);

	elf_gsetx_cfn(R,"elf.loadlib",core_lib_load_dll);
	elf_gsetx_cfn(R,"elf.libfn",core_lib_get_dll_fn);

	elf_gsetx_cfn(R,"elf.include",core_lib_include);
	elf_gsetx_cfn(R,"elf.loadcode",core_lib_load_code);
	elf_gsetx_cfn(R,"elf.loadexpr",core_lib_load_expr);
	elf_gsetx_cfn(R,"elf.unload",core_lib_unload);

	elf_gsetx_cfn(R,"elf.load_file",core_lib_load_file);
	elf_gsetx_cfn(R,"elf.load_json",core_lib_load_json);

	elf_gsetx_cfn(R,"elf.pf_indent",core_lib_pf_indent);
	elf_gsetx_cfn(R,"elf.pf",core_lib_pf);
	elf_gsetx_cfn(R,"elf.lpf",core_lib_lpf);


	elf_gsetx_cfn(R,"elf.change_work_dir",core_lib_change_work_dir);
	elf_gsetx_cfn(R,"elf.get_work_dir",core_lib_get_work_dir);

	// elf_gsetx_cfn(R,"elf.enumerate_folder",core_lib_enumerate_folder);
	// elf_gsetx_cfn(R,"elf.get_disk_info",core_lib_get_disk_info);
	// elf_gsetx_cfn(R,"elf.list_volumes",core_lib_list_volumes);
	// elf_gsetx_cfn(R,"elf.list_folder",core_lib_list_folder);


	elf_gsetx_sys(R,"elf.ferr",stderr);
	elf_gsetx_sys(R,"elf.fout",stdout);
	elf_gsetx_sys(R,"elf.fin",stdin);

	elf_gsetx_cfn(R,"elf.pf",core_lib_pf);
	elf_gsetx_cfn(R,"elf.fpf",core_lib_fpf);
	elf_gsetx_cfn(R,"elf.fload",core_lib_read_file);
	elf_gsetx_cfn(R,"elf.ftemp",core_lib_open_temp_file);
	elf_gsetx_cfn(R,"elf.fopen",core_lib_open_file);
	elf_gsetx_cfn(R,"elf.fclose",core_lib_close_file);
	elf_gsetx_cfn(R,"elf.fsize",core_lib_get_file_size);
	// elf_gsetx_cfn(R,"elf.get_file_size",core_lib_get_file_size);

	elf_gsetx_cfn(R,"elf.sleep",core_lib_sleep);
	elf_gsetx_cfn(R,"elf.exec",core_lib_exec);
	elf_gsetx_cfn(R,"elf.shell",lib_core_shell);

	elf_gsetx_cfn(R,"elf.float2",core_lib_float2);

	// math_lib_include(R);
}
#endif