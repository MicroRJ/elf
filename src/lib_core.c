/*
** See Copyright Notice In elf.h
** lib_core.c
*/



#include "lib_math.c"


static int core_lib_float2(elf_State *R);
static int core_lib_pause_collector(elf_State *R);
static int core_lib_get_allocated_objects(elf_State *R);
static int core_lib_get_allocated_memory(elf_State *R);
static int core_lib_get_collector_threshold(elf_State *R);
static int core_lib_mark_object(elf_State *R);
static int core_lib_collect(elf_State *R);
static int core_lib_tagof(elf_State *R);
static int core_lib_get_object_color(elf_State *R);
static int core_lib_set_object_trap(elf_State *R);
static int core_lib_get_object_address(elf_State *R);
static int core_lib_merge_tables(elf_State *R);
static int core_lib_get_meta(elf_State *R);
static int core_lib_set_meta(elf_State *R);
static int core_lib_abort(elf_State *R);
static int core_lib_exit(elf_State *R);
static int core_lib_flags(elf_State *R);
static int core_lib_debugger(elf_State *R);
static int core_lib_log(elf_State *R);
static int core_lib_err_log(elf_State *R);
static int core_lib_include(elf_State *R);
static int core_lib_load_expr(elf_State *R);
static int core_lib_load_code(elf_State *R);
static int core_lib_load_file(elf_State *R);
static int core_lib_load_dll(elf_State *R);
static int core_lib_shell(elf_State *R);
static int core_lib_exec(elf_State *R);

static int core_lib_fopen(elf_State *R);
static int core_lib_fclose(elf_State *R);
static int core_lib_fsize(elf_State *R);
static int core_lib_load_file_data(elf_State *R);
static int core_lib_ftemp(elf_State *R);
static int core_lib_change_work_dir(elf_State *R);
static int core_lib_get_work_dir(elf_State *R);
static int core_lib_get_disk_info(elf_State *R);
static int core_lib_list_volumes(elf_State *R);
static int core_lib_list_folder(elf_State *R);
static int core_lib_enumerate_folder(elf_State *R);

static void elf_include_core_lib(elf_State *R);


// mostly experimental...
int core_lib_float2(elf_State *R) {
	elf_Value v={elf_TAG_FLOAT2};
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
	elf_add_int(R,ARRAY_LENGTH(R->collector.objects));
	return 1;
}


int core_lib_get_allocated_memory(elf_State *R) {
	elf_add_int(R,R->collector.memory_allocated);
	return 1;
}


int core_lib_get_collector_threshold(elf_State *R) {
	elf_add_int(R,R->collector.memory_threshold);
	return 1;
}


int core_lib_mark_object(elf_State *R) {
	elf_Int num = elf_mark_object(elf_get_obj(R,0));
	elf_add_int(R,num);
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
	elf_Node *obj = elf_get_obj(R,0);
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
	elf_add_int(R,(elf_Int) (void *) elf_get_obj(R,0));
	return 1;
}


/* merges one or several tables together into
a new table, which is then returned. */
int core_lib_merge_tables(elf_State *R) {
	elf_Table *tab = elf_new_table(R);
	int i;
	for (i = 0; i < elf_get_num_args(R); i += 1) {
		elf_merge_tables(tab,elf_get_tab(R,i));
	}
	return 1;
}


int core_lib_get_meta(elf_State *R) {
	elf_add_tab(R,elf_get_obj(R,0)->meta);
	return 1;
}


int core_lib_set_meta(elf_State *R) {
	elf_get_obj(R,0)->meta=elf_get_tab(R,1);
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
	elf_add_int(R,flags);
	return 1;
}


int core_lib_debugger(elf_State *R) {
#if defined(_DEBUG)
	R->flags |= FLAG_DEBUGGER;
#else
	char *message = "no message";
	if (elf_get_num_args(R) != 0) {
		message = elf_get_txt(R,0);
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
	char *dir = elf_get_txt(R,0);
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
		if (entry.key.tag == elf_TAG_STR) {
			char *sym = in_sym_dir(dir,entry.key.x_str->text);
			if (*sym != '.') continue;
			elf_String *ref = elf_alloc_string(R,sym);
			elf_table_set(globals,VSTR(ref),globals->array[entry.idx]);
		}
	}
	return 0;
}


int core_lib_load_expr(elf_State *R) {
	elf_String *filename = 0;
	elf_String *contents = 0;
	if (elf_get_num_args(R) == 2) {
		filename = elf_get_str(R,0);
		contents = elf_get_str(R,1);
	} else if (elf_get_num_args(R) == 1) {
		filename = elf_new_string(R,"unnamed");
		contents = elf_get_str(R,0);
	} else NO_CODE;
	NO_CODE;
	(void) filename;
	(void) contents;
	// elf_parse_expr3(R,filename,GET_FRAME(R)->ry,GET_FRAME(R)->ntoyield,contents);
	/* no need to do hoisting */
	return 0;
}


int core_lib_load_code(elf_State *R) {
	elf_String *filename = 0;
	elf_String *contents = 0;
	if (elf_get_num_args(R) == 2) {
		filename = elf_get_str(R,0);
		contents = elf_get_str(R,1);
	} else if (elf_get_num_args(R) == 1) {
		filename = elf_new_string(R,"unnamed");
		contents = elf_get_str(R,0);
	} else NO_CODE;
	NO_CODE;
	(void) filename;
	(void) contents;
	// elf_parse_code3(R,filename,GET_FRAME(R)->ry,GET_FRAME(R)->ntoyield,contents);
	/* no need to do hoisting */
	return 0;
}


int core_lib_load_file(elf_State *R) {
	elf_String *filename;
	int nresults;

	filename=elf_get_str(R,0);
	nresults=elf_exec_file(R,filename,GET_FRAME(R)->nargs,GET_FRAME(R)->nrets);
	// if (nresults != -1 && !strcmp(filename->text,"patterns.elf")){
	// 	__debugbreak();
	// }
	// copy_memory(R->frame->locals-1,R->stack_ptr-nresults,nresults*sizeof(elf_Value));
	return nresults;
}




// typedef void (*em_dlopen_callback)(void* handle, void* user_data);
// void emscripten_dlopen(const char *filename, int flags, void* user_data, em_dlopen_callback onsuccess, em_arg_callback_func onerror);
int core_lib_get_dll_fn(elf_State *S) {
	elf_Handle lib = elf_get_sys(S,0);
	char *name = elf_get_txt(S,1);
	elf_Function fn = (elf_Function) sys_get_dll_fn(lib,name);

	if (fn != 0) elf_add_cfn(S,fn);
	else elf_add_nil(S);
	return 1;
}


int core_lib_load_dll(elf_State *R) {
	char *file = elf_get_txt(R,0);
	elf_Handle lib = sys_load_dll(file);

	if (lib != 0) elf_add_sys(R,lib);
	else elf_add_nil(R);
	return 1;
}


int core_lib_shell(elf_State *R) {
	char *verb = elf_get_txt(R,0);
	char *file = elf_get_txt(R,1);
	char *args = elf_get_txt(R,2);
	elf_add_int(R,sys_shell(verb,file,args));
	return 1;
}


int core_lib_exec(elf_State *R) {
	char *cline=elf_get_txt(R,0);
	int result=sys_exec(0,cline);
	elf_add_int(R,result);
	return 1;
}


int core_lib_fopen(elf_State *R) {
	ASSERT(elf_get_num_args(R) == 2);
	char *name = elf_get_txt(R,0);
	char *flags = elf_get_txt(R,1);
	FILE *file = fopen(name,flags);
	elf_add_sys(R,(elf_Handle)file);
	return 1;
}


int core_lib_fclose(elf_State *R) {
	ASSERT(elf_get_num_args(R) == 1);
	elf_Handle file = elf_get_sys(R,0);
	fclose(file);
	return 0;
}


int core_lib_fsize(elf_State *R) {
	if (elf_get_tag(R,0) == elf_TAG_SYS) {
		elf_Handle file = (FILE*) elf_get_sys(R,0);
		fseek(file,0,SEEK_END);
		elf_add_int(R,ftell(file));
	} else NO_CODE;
	return 1;
}


/* todo: update to use sys layer */
int core_lib_load_file_data(elf_State *R) {
	FILE *file = (FILE*) elf_get_sys(R,0);
	if (file != 0) {
		fseek(file,0,SEEK_END);
		long size = ftell(file);
		fseek(file,0,SEEK_SET);
		elf_String *buf = elf_alloc_string2(R,size);
		fread(buf->text,1,size,file);
		elf_add_str(R,buf);
	} else elf_add_nil(R);
	return 1;
}


int core_lib_ftemp(elf_State *R) {
	FILE *file = {0};
#if defined(PLATFORM_WEB)
	file = tmpfile();
#else
	tmpfile_s(&file);
#endif
	elf_add_sys(R,(elf_Handle)file);
	return 1;
}


int core_lib_change_work_dir(elf_State *R) {
	int ok = sys_set_work_dir(elf_get_txt(R,0));
	elf_add_int(R,ok);
	return 1;
}


int core_lib_get_work_dir(elf_State *R) {
	char buf[MAX_PATH];
	sys_get_work_dir(sizeof(buf),buf);
	elf_new_string(R,buf);
	return 1;
}


int core_lib_fpf(elf_State *S) {
	elf_Handle file = elf_get_sys(S,0);
	int wrote = 0;
	for (int i = 1; i < elf_get_num_args(S); i ++) {
		wrote += fpf_value(file,elf_get_arg(S,i),0);
	}
	elf_add_int(S,wrote);
	return 1;
}


GLOBAL int pf_indent;
GLOBAL int pf_char;
int core_lib_pf_indent(elf_State *S) {
	pf_indent = elf_get_int(S,0);
	elf_add_int(S,pf_indent);
	return 1;
}


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


int core_lib_pf(elf_State *S) {
	for (int j = 0; j < pf_indent; ++ j) {
		fprintf(stdout,"  ");
	}
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


int core_lib_clocktime(elf_State *rt) {
	elf_add_int(rt,sys_get_clock_time());
	return 1;
}


int core_lib_timediffs(elf_State *S) {
	ASSERT(elf_get_num_args(S) == 1);
	elf_Int time = elf_get_int(S,0);
	elf_add_num(S,elf_time_diff_s(time));
	return 1;
}


int core_lib_timediffms(elf_State *S) {
	ASSERT(elf_get_num_args(S) == 1);
	elf_Int time = elf_get_int(S,0);
	elf_add_num(S,elf_time_diff_ms(time));
	return 1;
}


int core_lib_get_file_size(elf_State *S) {
	char *path = elf_get_txt(S,0);
	elf_Int size = -1;
	#if 0
	#if defined(_WIN32)
	WIN32_FILE_ATTRIBUTE_DATA attrs;
	if (GetFileAttributesEx(path,GetFileExInfoStandard,&attrs)) {
		size = (attrs.nFileSizeHigh * (MAXDWORD + 1)) + attrs.nFileSizeLow;
	}
 	#endif
 	#endif
	elf_add_int(S,size);
	return 1;
}


int core_lib_get_disk_info(elf_State *R) {
	elf_Table *info = elf_new_table(R);
#if 0
	DWORD SectorsPerCluster;
	DWORD BytesPerSector;
	DWORD NumberOfFreeClusters;
	DWORD TotalNumberOfClusters;
	GetDiskFreeSpaceA(elf_get_txt(R,0),&SectorsPerCluster,&BytesPerSector,&NumberOfFreeClusters,&TotalNumberOfClusters);
	elf_tsets_int(info,elf_alloc_string(R,"SectorsPerCluster"),SectorsPerCluster);
	elf_tsets_int(info,elf_alloc_string(R,"BytesPerSector"),BytesPerSector);
	elf_tsets_int(info,elf_alloc_string(R,"NumberOfFreeClusters"),NumberOfFreeClusters);
	elf_tsets_int(info,elf_alloc_string(R,"TotalNumberOfClusters"),TotalNumberOfClusters);
#else
	elf_debug_log("this function is not implemented for this platform");
#endif
	return 1;
}


int core_lib_list_volumes(elf_State *R) {
	elf_Table *list = elf_new_table(R); /* <- */
#if 0
	char buffer[MAX_PATH];
	HANDLE handle = FindFirstVolumeA(buffer,MAX_PATH);

	elf_String *name = 0;
	if (handle != INVALID_HANDLE_VALUE) do {

		elf_Table *volume = elf_new_table(R);
		name = elf_new_string(R,buffer);

		elf_tsets_tab(list,name,volume);

		elf_tsets_str(volume,elf_alloc_string(R,"name"),name);

		elf_Table *path_names = elf_new_table(R);
		elf_tsets_tab(volume,elf_alloc_string(R,"path_names"),path_names);

		if (GetVolumePathNamesForVolumeNameA(name->text,buffer,MAX_PATH,NULL)) {
			char *cursor = buffer;
			while (*cursor != '\0') {
				elf_array_add(path_names,VSTR(elf_alloc_string(R,buffer)));
				cursor += strlen(cursor) + 1;
			}
		}
	} while(FindNextVolumeA(handle,buffer,MAX_PATH));
	FindVolumeClose(handle);
#endif

	elf_add_tab(R,list); /* <- */
	return 1;
}


typedef struct folder_enumerator {
	elf_State *state;
	elf_Closure *closure;
	elf_String *sfolder,*sfile,*sname,*spath,*stype,*ssize;
	elf_Table *storage;
} folder_enumerator;


static int core_lib_enumerate_folder_callback(void *user, int filetype, size_t filesize, char *filename, char *filepath) {
	folder_enumerator *fc=user;
	elf_Closure *cls=fc->closure;
	elf_State *state=fc->state;
	elf_String *name,path;
	elf_Table *storage=fc->storage;
	int nrets;

	elf_Table *file=elf_new_table(state);
	elf_tsets_str(file,fc->sname,elf_new_string(state,filename));
	elf_tsets_str(file,fc->spath,elf_new_string(state,filepath));
	elf_tsets_str(file,fc->stype,filetype?fc->sfolder:fc->sfile);
	elf_tsets_int(file,fc->ssize,filesize);

	if (cls){
		elf_add_cls(state,cls);
		elf_add_obj(state,elf_get_this(state));
		elf_add_tab(state,file);
		nrets=elf_call(state,2,1);
	}
	if (storage){
		elf_tadd_tab(storage,file);
	}
		// int results = 0; NO_CODE; // elf_call(R,base,1,1);
		// if (is_directory) {
		// 	if ((results > 0) && elf_get_int(R,base) != 0) {
		// 		core_lib_enumerate_folder_(R,path,cls);
		// 	}
		// }
		// SET_TOP(R,top);
	return 0;
}


static folder_enumerator make_folder_enumerator(elf_State *state, elf_Closure *closure, elf_Table *storage){
	folder_enumerator folder;
	folder.state=state;
	folder.closure=closure;
	folder.storage=storage;
	folder.sfolder=elf_new_string(state,"folder");
	folder.sfile=elf_new_string(state,"file");
	folder.sname=elf_new_string(state,"name");
	folder.spath=elf_new_string(state,"path");
	folder.stype=elf_new_string(state,"type");
	folder.ssize=elf_new_string(state,"size");
	return folder;
}


int core_lib_list_folder(elf_State *R) {
	ASSERT(elf_get_num_args(R) > 0);

	char *filename;
	elf_Closure *closure;
	elf_Table *storage;
	folder_enumerator folder;

	filename=elf_get_txt(R,0);
	closure=elf_get_cls(R,1);
	storage=elf_new_table(R);
	folder=make_folder_enumerator(R,closure,storage);


	sys_enumerate_folder(THREAD_ALLOCATOR,filename,&folder,core_lib_enumerate_folder_callback);

	elf_add_tab(R,storage);
	return 1;
}


int core_lib_enumerate_folder(elf_State *R) {
	ASSERT(elf_get_num_args(R)==2);
	/* push these keys temporarily so they won't
	be gc'd and also to to avoid creating them so often  */

	char *filename;
	folder_enumerator folder;

	filename=elf_get_txt(R,0);
	folder=make_folder_enumerator(R,elf_get_cls(R,1),0);
	sys_enumerate_folder(THREAD_ALLOCATOR,filename,&folder,core_lib_enumerate_folder_callback);
	return 0;
}


void ftabs(FILE *io, int level) {
	while (level --) fprintf(io,"\t");
}


void elf_unload(FILE *io, elf_Table *tab, int level) {
	fprintf(io,"{");
	int nitems = 0;
	for (elf_Int i = 0; i < tab->ntotal; ++ i) {
		elf_Entry slot = tab->slots[i];
		if (slot.key.tag == elf_TAG_NIL) {
			continue;
		}
		elf_Value v = tab->array[slot.idx];
		if ((v.tag == elf_TAG_CLS) || (v.tag == elf_TAG_CFN)) {
			continue;
		}
		if (nitems ++ != 0) fprintf(io,",");
		fpf_value(io,slot.key,1);
		fprintf(io," = ");
		if (v.tag == elf_TAG_TAB) {
			elf_unload(io,v.x_tab,level+1);
		} else {
			fpf_value(io,v,1);
		}
	}
	fprintf(io,"}");
}


int core_lib_unload(elf_State *S) {
	elf_Handle io = elf_get_sys(S,0);
	elf_Table *tab = elf_get_tab(S,1);
	elf_unload(io,tab,0);
	return 0;
}


// DEPRECATED
int core_lib_iton(elf_State *R) {
	elf_Value v = elf_get_arg(R,0);
	if (v.tag==elf_TAG_INT) {
		elf_add_num(R,(elf_Num)v.x_int);
	} else elf_add_num(R,v.x_num);
	return 1;
}


// DEPRECATED
int core_lib_ntoi(elf_State *R) {
	elf_Value v = elf_get_arg(R,0);
	if (v.tag==elf_TAG_NUM) {
		elf_add_int(R,(elf_Int)v.x_num);
	} else elf_add_int(R,v.x_int);
	return 1;
}


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
	elf_gsetx_cfn(R,"elf.load_file",core_lib_load_file);
	elf_gsetx_cfn(R,"elf.unload",core_lib_unload);

	elf_gsetx_cfn(R,"elf.pf_indent",core_lib_pf_indent);
	elf_gsetx_cfn(R,"elf.pf",core_lib_pf);
	elf_gsetx_cfn(R,"elf.lpf",core_lib_lpf);


	elf_gsetx_cfn(R,"elf.change_work_dir",core_lib_change_work_dir);
	elf_gsetx_cfn(R,"elf.get_work_dir",core_lib_get_work_dir);
	elf_gsetx_cfn(R,"elf.enumerate_folder",core_lib_enumerate_folder);
	elf_gsetx_cfn(R,"elf.list_folder",core_lib_list_folder);
	elf_gsetx_cfn(R,"elf.list_volumes",core_lib_list_volumes);
	elf_gsetx_cfn(R,"elf.get_disk_info",core_lib_get_disk_info);

	elf_gsetx_sys(R,"elf.ferr",stderr);
	elf_gsetx_sys(R,"elf.fout",stdout);
	elf_gsetx_sys(R,"elf.fin",stdin);

	elf_gsetx_cfn(R,"elf.pf",core_lib_pf);
	elf_gsetx_cfn(R,"elf.fpf",core_lib_fpf);
	elf_gsetx_cfn(R,"elf.fload",core_lib_load_file_data);
	elf_gsetx_cfn(R,"elf.ftemp",core_lib_ftemp);
	elf_gsetx_cfn(R,"elf.fopen",core_lib_fopen);
	elf_gsetx_cfn(R,"elf.fclose",core_lib_fclose);
	elf_gsetx_cfn(R,"elf.fsize",core_lib_fsize);
	elf_gsetx_cfn(R,"elf.get_file_size",core_lib_get_file_size);

	elf_gsetx_cfn(R,"elf.sleep",core_lib_sleep);
	elf_gsetx_cfn(R,"elf.exec",core_lib_exec);
	elf_gsetx_cfn(R,"elf.shell",core_lib_shell);

	elf_gsetx_cfn(R,"elf.float2",core_lib_float2);

	math_lib_include(R);
}
