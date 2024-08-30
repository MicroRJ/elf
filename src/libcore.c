/*
** See Copyright Notice In elf.h
** libcore.c
*/



#include "libmath.c"


static int core_lib_float2(elf_Shell *R);
static int core_lib_pause_collector(elf_Shell *R);
static int core_lib_get_allocated_objects(elf_Shell *R);
static int core_lib_get_allocated_memory(elf_Shell *R);
static int core_lib_get_collector_threshold(elf_Shell *R);
static int core_lib_mark_object(elf_Shell *R);
static int core_lib_collect(elf_Shell *R);
static int core_lib_tagof(elf_Shell *R);
static int core_lib_get_object_color(elf_Shell *R);
static int core_lib_set_object_trap(elf_Shell *R);
static int core_lib_get_object_address(elf_Shell *R);
static int core_lib_merge_tables(elf_Shell *R);
static int core_lib_get_metatable(elf_Shell *R);
static int core_lib_set_metatable(elf_Shell *R);
static int core_lib_abort(elf_Shell *R);
static int core_lib_exit(elf_Shell *R);
static int core_lib_flags(elf_Shell *R);
static int core_lib_debugger(elf_Shell *R);
static int core_lib_log(elf_Shell *R);
static int core_lib_err_log(elf_Shell *R);
static int core_lib_include(elf_Shell *R);
static int core_lib_load_expr(elf_Shell *R);
static int core_lib_load_code(elf_Shell *R);
static int core_lib_load_file(elf_Shell *R);
static int core_lib_load_dll(elf_Shell *R);
static int core_lib_shell(elf_Shell *R);
static int core_lib_exec(elf_Shell *R);
static int core_lib_fopen(elf_Shell *R);
static int core_lib_fclose(elf_Shell *R);
static int core_lib_fsize(elf_Shell *R);
static int core_lib_load_file_data(elf_Shell *R);
static int core_lib_ftemp(elf_Shell *R);
static int core_lib_change_work_dir(elf_Shell *R);
static int core_lib_get_work_dir(elf_Shell *R);
static int core_lib_get_disk_info(elf_Shell *R);
static int core_lib_list_volumes(elf_Shell *R);
static int core_lib_list_folder(elf_Shell *R);
static int core_lib_enumerate_folder(elf_Shell *R);
static void elf_include_core_lib(elf_Shell *R);



// mostly experimental...
int core_lib_float2(elf_Shell *R) {
	elf_Value v={TAG_FLOAT2};
	v.x_f32=(float)elf_get_number(R,0);
	v.y_f32=v.x_f32;
	if (elf_get_num_args(R)>1){
		v.y_f32=(float)elf_get_number(R,1);
	}
	*R->stack_ptr++=v;
	return 1;
}


int core_lib_pause_collector(elf_Shell *R) {
	R->collector.paused = 1;
	if (elf_get_num_args(R) == 1) {
		R->collector.paused = elf_get_integer(R,0) != 0;
	}
	return 0;
}


int core_lib_get_allocated_objects(elf_Shell *R) {
	elf_put_integer(R,ARRAY_LENGTH(R->collector.objects));
	return 1;
}


int core_lib_get_allocated_memory(elf_Shell *R) {
	elf_put_integer(R,R->collector.memory_allocated);
	return 1;
}


int core_lib_get_collector_threshold(elf_Shell *R) {
	elf_put_integer(R,R->memory.memory_threshold);
	return 1;
}


int core_lib_mark_object(elf_Shell *R) {
	elf_Int num = elf_mark_object(elf_get_object(R,0));
	elf_put_integer(R,num);
	return 1;
}


int core_lib_collect(elf_Shell *R) {
	elf_trigger_collection_cycle(R);
	return 0;
}


int core_lib_tagof(elf_Shell *R) {
	elf_new_string(R,(char*)tag2s[elf_get_tag(R,0)]);
	return 1;
}


int core_lib_get_object_color(elf_Shell *R) {
	elf_Object *obj = elf_get_object(R,0);
	int color = obj->color;
	elf_new_string(R,
	color == elf_GC_BLACK ? "black" :
	color == elf_GC_WHITE ? "white" :
	color == elf_GC_PINK  ? "pink"  :
	color == elf_GC_RED   ? "red"   :
	color == elf_GC_TRAP  ? "trap"  : "error");
	return 1;
}


int core_lib_set_object_trap(elf_Shell *R) {
	elf_Int set = elf_get_integer(R,1);
	OBJ_COLOR(elf_get_object(R,0)) = set ? elf_GC_TRAP : elf_GC_WHITE;
	return 0;
}


int core_lib_get_object_address(elf_Shell *R) {
	elf_put_integer(R,(elf_Int) (void *) elf_get_object(R,0));
	return 1;
}


/* merges one or several tables together into
a new table, which is then returned. */
int core_lib_merge_tables(elf_Shell *R) {
	elf_Table *tab = elf_put_new_table(R);
	int i;
	for (i = 0; i < elf_get_num_args(R); i += 1) {
		elf_merge_tables(tab,elf_get_table(R,i));
	}
	return 1;
}


int core_lib_get_metatable(elf_Shell *R) {
	elf_new_table(R,elf_get_object(R,0)->metatable);
	return 1;
}


int core_lib_set_metatable(elf_Shell *R) {
	elf_get_object(R,0)->metatable=elf_get_table(R,1);
	PUSHV(R,elf_get_arg(R,0));
	return 1;
}


/*  */
int core_lib_abort(elf_Shell *R) {
	if(1) abort();
	return 0;
}


int core_lib_exit(elf_Shell *R) {
	if(1) exit(elf_get_integer(R,0));
	return 0;
}


/* debugging */
int core_lib_flags(elf_Shell *R) {
	int flags = R->flags;
	R->flags |= elf_get_integer(R,0);
	elf_put_integer(R,flags);
	return 1;
}


int core_lib_debugger(elf_Shell *R) {
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


int core_lib_log(elf_Shell *R) {
	FOR_RANGE(i,0,elf_get_num_args(R)) {
		fpf_value(stdout,elf_get_arg(R,i),0);
	}
	fprintf(stdout,"\n");
	return 0;
}


int core_lib_err_log(elf_Shell *R) {
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
int core_lib_include(elf_Shell *R) {
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
		if (entry.key.tag == TAG_STR) {
			char *sym = in_sym_dir(dir,entry.key.x_str->text);
			if (*sym != '.') continue;
			elf_String *ref = elf_alloc_string(R,sym);
			elf_tset(globals,elSTR(ref),globals->array[entry.idx]);
		}
	}
	return 0;
}


int core_lib_load_expr(elf_Shell *R) {
	elf_String *filename = 0;
	elf_String *contents = 0;
	if (elf_get_num_args(R) == 2) {
		filename = elf_get_string(R,0);
		contents = elf_get_string(R,1);
	} else if (elf_get_num_args(R) == 1) {
		filename = elf_new_string(R,"unnamed");
		contents = elf_get_string(R,0);
	} else NO_CODE;
	NO_CODE;
	(void) filename;
	(void) contents;
	// elf_parse_expr3(R,filename,elGETFRAME(R)->ry,elGETFRAME(R)->ntoyield,contents);
	/* no need to do hoisting */
	return 0;
}


int core_lib_load_code(elf_Shell *R) {
	elf_String *filename = 0;
	elf_String *contents = 0;
	if (elf_get_num_args(R) == 2) {
		filename = elf_get_string(R,0);
		contents = elf_get_string(R,1);
	} else if (elf_get_num_args(R) == 1) {
		filename = elf_new_string(R,"unnamed");
		contents = elf_get_string(R,0);
	} else NO_CODE;
	NO_CODE;
	(void) filename;
	(void) contents;
	// elf_parse_code3(R,filename,elGETFRAME(R)->ry,elGETFRAME(R)->ntoyield,contents);
	/* no need to do hoisting */
	return 0;
}


int core_lib_load_file(elf_Shell *R) {
	elf_String *filename;
	int nresults;

	filename=elf_get_string(R,0);
	nresults=elf_load_file(R,filename,elGETFRAME(R)->nargs,elGETFRAME(R)->nregs);
	// if (nresults != -1 && !strcmp(filename->text,"patterns.elf")){
	// 	__debugbreak();
	// }
	// copy_memory(R->frame->locals-1,R->stack_ptr-nresults,nresults*sizeof(elf_Value));
	return nresults;
}




// typedef void (*em_dlopen_callback)(void* handle, void* user_data);
// void emscripten_dlopen(const char *filename, int flags, void* user_data, em_dlopen_callback onsuccess, em_arg_callback_func onerror);
int core_lib_get_dll_fn(elf_Shell *S) {
	elf_Handle lib = elf_get_handle(S,0);
	char *name = elf_get_text(S,1);
	elf_CFunction fn = (elf_CFunction) sys_get_dll_fn(lib,name);

	if (fn != 0) elf_put_cfunction(S,fn);
	else elf_put_nil(S);
	return 1;
}


int core_lib_load_dll(elf_Shell *R) {
	char *file = elf_get_text(R,0);
	elf_Handle lib = sys_load_dll(file);

	if (lib != 0) elf_put_handle(R,lib);
	else elf_put_nil(R);
	return 1;
}


int core_lib_shell(elf_Shell *R) {
	char *verb = elf_get_text(R,0);
	char *file = elf_get_text(R,1);
	char *args = elf_get_text(R,2);
	elf_put_integer(R,sys_shell(verb,file,args));
	return 1;
}


int core_lib_exec(elf_Shell *R) {
	char *cline=elf_get_text(R,0);
	int result=sys_exec(0,cline);
	elf_put_integer(R,result);
	return 1;
}


int core_lib_fopen(elf_Shell *R) {
	ASSERT(elf_get_num_args(R) == 2);
	char *name = elf_get_text(R,0);
	char *flags = elf_get_text(R,1);
	FILE *file = fopen(name,flags);
	elf_put_handle(R,(elf_Handle)file);
	return 1;
}


int core_lib_fclose(elf_Shell *R) {
	ASSERT(elf_get_num_args(R) == 1);
	elf_Handle file = elf_get_handle(R,0);
	fclose(file);
	return 0;
}


int core_lib_fsize(elf_Shell *R) {
	if (elf_get_tag(R,0) == TAG_SYS) {
		elf_Handle file = (FILE*) elf_get_handle(R,0);
		fseek(file,0,SEEK_END);
		elf_put_integer(R,ftell(file));
	} else NO_CODE;
	return 1;
}


/* todo: update to use sys layer */
int core_lib_load_file_data(elf_Shell *R) {
	FILE *file = (FILE*) elf_get_handle(R,0);
	if (file != 0) {
		fseek(file,0,SEEK_END);
		long size = ftell(file);
		fseek(file,0,SEEK_SET);
		elf_String *buf = elf_alloc_string2(R,size);
		fread(buf->text,1,size,file);
		elf_put_string(R,buf);
	} else elf_put_nil(R);
	return 1;
}


int core_lib_ftemp(elf_Shell *R) {
	FILE *file = {0};
#if defined(PLATFORM_WEB)
	file = tmpfile();
#else
	tmpfile_s(&file);
#endif
	elf_put_handle(R,(elf_Handle)file);
	return 1;
}


int core_lib_change_work_dir(elf_Shell *R) {
	int ok = sys_set_work_dir(elf_get_text(R,0));
	elf_put_integer(R,ok);
	return 1;
}


int core_lib_get_work_dir(elf_Shell *R) {
	char buf[MAX_PATH];
	sys_get_work_dir(sizeof(buf),buf);
	elf_new_string(R,buf);
	return 1;
}


int core_lib_fpf(elf_Shell *S) {
	elf_Handle file = elf_get_handle(S,0);
	int wrote = 0;
	for (int i = 1; i < elf_get_num_args(S); i ++) {
		wrote += fpf_value(file,elf_get_arg(S,i),0);
	}
	elf_put_integer(S,wrote);
	return 1;
}


GLOBAL int pf_indent;
GLOBAL int pf_char;
int core_lib_pf_indent(elf_Shell *S) {
	pf_indent = elf_get_integer(S,0);
	elf_put_integer(S,pf_indent);
	return 1;
}


int core_lib_lpf(elf_Shell *S) {
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


int core_lib_pf(elf_Shell *S) {
	for (int j = 0; j < pf_indent; ++ j) {
		fprintf(stdout,"  ");
	}
	for (int i = 0; i < elf_get_num_args(S); i ++) {
		fpf_value(stdout,elf_get_arg(S,i),0);
	}
	fprintf(stdout,"\n");
	return 0;
}


int core_lib_sleep(elf_Shell *S) {
	ASSERT(elf_get_num_args(S) >= 1);
	sys_sleep(elf_get_integer(S,0));
	return 0;
}


int core_lib_clocktime(elf_Shell *rt) {
	elf_put_integer(rt,sys_get_clock_time());
	return 1;
}


int core_lib_timediffs(elf_Shell *S) {
	ASSERT(elf_get_num_args(S) == 1);
	elf_Int time = elf_get_integer(S,0);
	elf_put_number(S,elf_time_diff_s(time));
	return 1;
}


int core_lib_timediffms(elf_Shell *S) {
	ASSERT(elf_get_num_args(S) == 1);
	elf_Int time = elf_get_integer(S,0);
	elf_put_number(S,elf_time_diff_ms(time));
	return 1;
}


int core_lib_get_file_size(elf_Shell *S) {
	char *path = elf_get_text(S,0);
	elf_Int size = -1;
	#if 0
	#if defined(_WIN32)
	WIN32_FILE_ATTRIBUTE_DATA attrs;
	if (GetFileAttributesEx(path,GetFileExInfoStandard,&attrs)) {
		size = (attrs.nFileSizeHigh * (MAXDWORD + 1)) + attrs.nFileSizeLow;
	}
 	#endif
 	#endif
	elf_put_integer(S,size);
	return 1;
}


int core_lib_get_disk_info(elf_Shell *R) {
	elf_Table *info = elf_put_new_table(R);
#if 0
	DWORD SectorsPerCluster;
	DWORD BytesPerSector;
	DWORD NumberOfFreeClusters;
	DWORD TotalNumberOfClusters;
	GetDiskFreeSpaceA(elf_get_text(R,0),&SectorsPerCluster,&BytesPerSector,&NumberOfFreeClusters,&TotalNumberOfClusters);
	elf_tsets_int(info,elf_alloc_string(R,"SectorsPerCluster"),SectorsPerCluster);
	elf_tsets_int(info,elf_alloc_string(R,"BytesPerSector"),BytesPerSector);
	elf_tsets_int(info,elf_alloc_string(R,"NumberOfFreeClusters"),NumberOfFreeClusters);
	elf_tsets_int(info,elf_alloc_string(R,"TotalNumberOfClusters"),TotalNumberOfClusters);
#else
	elf_debug_log("this function is not implemented for this platform");
#endif
	return 1;
}


int core_lib_list_volumes(elf_Shell *R) {
	elf_Table *list = elf_put_new_table(R); /* <- */
#if 0
	char buffer[MAX_PATH];
	HANDLE handle = FindFirstVolumeA(buffer,MAX_PATH);

	elf_String *name = 0;
	if (handle != INVALID_HANDLE_VALUE) do {

		elf_Table *volume = elf_put_new_table(R);
		name = elf_new_string(R,buffer);

		elf_tsets_tab(list,name,volume);

		elf_tsets_str(volume,elf_alloc_string(R,"name"),name);

		elf_Table *path_names = elf_put_new_table(R);
		elf_tsets_tab(volume,elf_alloc_string(R,"path_names"),path_names);

		if (GetVolumePathNamesForVolumeNameA(name->text,buffer,MAX_PATH,NULL)) {
			char *cursor = buffer;
			while (*cursor != '\0') {
				elf_tadd(path_names,elSTR(elf_alloc_string(R,buffer)));
				cursor += strlen(cursor) + 1;
			}
		}
	} while(FindNextVolumeA(handle,buffer,MAX_PATH));
	FindVolumeClose(handle);
#endif

	elf_new_table(R,list); /* <- */
	return 1;
}


void core_lib_list_folder_(elf_Shell *R, elf_Table *list, int level, elf_String *dir);
int core_lib_list_folder(elf_Shell *R) {
	ASSERT(elf_get_num_args(R) > 0);
	elf_String *dir = elf_get_string(R,0);
	elf_Int level = 0;
	if (elf_get_num_args(R) > 1) {
		level = elf_get_integer(R,1);
	}
	elf_Table *list = elf_put_new_table(R);
	core_lib_list_folder_(R,list,level,dir);
	elf_new_table(R,list);
	return 1;
}


/* Use -1 for recursive always, 0 for just this layer */
void core_lib_list_folder_(elf_Shell *R, elf_Table *list, int level, elf_String *dir) {
#if 0
	// defined(_WIN32)
	WIN32_FIND_DATAA f;
	HANDLE h = FindFirstFileA(elf_tpf("%s\\*",dir->c),&f);
	if (h != INVALID_HANDLE_VALUE) do {
		if (elf_is_virtual_file_name(f.cFileName)) continue;
		int is_directory = 0 != (f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
		elf_Value *top = GET_TOP(R);
		elf_String *name = elf_new_string(R,f.cFileName);
		elf_String *path = elf_new_string(R,elf_tpf("%s\\%s",dir->c,f.cFileName));
		elf_Table *file = elf_put_new_table(R);
		elf_tsets_str(file,elf_new_string(R,"name"),name);
		elf_tsets_str(file,elf_new_string(R,"path"),path);
		elf_tsets_int(file,elf_new_string(R,"is_directory"),is_directory);
		elf_tsets_int(file,elf_new_string(R,"size"),f.nFileSizeLow);
		elf_tadd(list,elTAB(file));
		if (level != 0) {
			if (is_directory) {
				core_lib_list_folder_(R,list,level-1,path);
			}
		}
		SET_TOP(R,top);
	} while (FindNextFileA(h,&f));
#else
	elf_fail(R,NO_BYTE,"unsupported platform");
#endif
}


#if 0
void core_lib_enumerate_folder_(elf_Shell *R, elf_String *dir, elf_Closure *cls) {
// defined(PLATFORM_DESKTOP)
	WIN32_FIND_DATAA f;
	HANDLE h = FindFirstFileA(elf_tpf("%s\\*",dir->c),&f);
	if (h != INVALID_HANDLE_VALUE) do {
		if (elf_is_virtual_file_name(f.cFileName)) continue;
		int is_directory = 0 != (f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
		elf_Value *top = GET_TOP(R);
		elf_String *name = elf_new_string(R,f.cFileName);
		elf_String *path = elf_new_string(R,elf_tpf("%s\\%s",dir->c,f.cFileName));
		elf_Table *file = elf_put_new_table(R);
		elf_tsets_str(file,elf_new_string(R,"name"),name);
		elf_tsets_str(file,elf_new_string(R,"path"),path);
		elf_tsets_int(file,elf_new_string(R,"is_directory"),is_directory);
		elf_tsets_int(file,elf_new_string(R,"size"),f.nFileSizeLow);
		elf_put_closure(R,cls);
		int results = 0; NO_CODE; // elf_call_function(R,base,1,1);
		// if (is_directory) {
		// 	if ((results > 0) && elf_get_integer(R,base) != 0) {
		// 		core_lib_enumerate_folder_(R,path,cls);
		// 	}
		// }
		SET_TOP(R,top);
	} while (FindNextFileA(h,&f));

	// defined(PLATFORM_WEB)
	DIR *dirfd = opendir(dir->c);
	if (dirfd != 0) {
		struct dirent *entry;
		while ((entry = readdir(dirfd)) != 0) {
			if (elf_is_virtual_file_name(entry->d_name)) {
				continue;
			}
			elf_Bool isdir = (entry->d_type & DT_DIR) != 0;
			elf_Value *top = GET_TOP(R);

			elf_String *name = elf_new_string(R,entry->d_name);
			elf_String *path = elf_new_string(R,elf_tpf("%s/%s",dir->c,entry->d_name));
			elf_StackId base = elf_put_closure(R,cls);
			elf_Table *file = elf_put_new_table(R);

			elf_tsets_str(file,elf_new_string(R,"name"),name);
			elf_tsets_str(file,elf_new_string(R,"path"),path);
			elf_tsets_int(file,elf_new_string(R,"isdir"),isdir);
			int r = elf_call_function(R,base,1,1);
			if ((r > 0) && isdir && elf_get_integer(R,base)) {
				core_lib_enumerate_folder_(R,path,cls);
			}
			SET_TOP(R,top);
		}
		closedir(dirfd);
	}
}
#endif


static int core_lib_enumerate_folder_callback(void *data, int fileflags, char *filename, char *filepath) {
	return 0;
}


int core_lib_enumerate_folder(elf_Shell *R) {
	ASSERT(elf_get_num_args(R)==2);
	/* push these keys temporarily so they won't
	be gc'd and also to to avoid creating them so often  */
	char *file=elf_get_text(R,0);
	elf_Closure *cls=elf_get_closure(R,1);
	sys_enumerate_folder(HEAP_ALLOCATOR,file,cls,core_lib_enumerate_folder_callback);
	// core_lib_enumerate_folder_(R,dir,cls);
	return 1;
}


void ftabs(FILE *io, int level) {
	while (level --) fprintf(io,"\t");
}


void elf_unload(FILE *io, elf_Table *tab, int level) {
	fprintf(io,"{");
	int nitems = 0;
	for (elf_Int i = 0; i < tab->ntotal; ++ i) {
		elf_Entry slot = tab->slots[i];
		if (slot.key.tag == TAG_NIL) {
			continue;
		}
		elf_Value v = tab->array[slot.idx];
		if ((v.tag == TAG_CLS) || (v.tag == TAG_CFN)) {
			continue;
		}
		if (nitems ++ != 0) fprintf(io,",");
		fpf_value(io,slot.key,1);
		fprintf(io," = ");
		if (v.tag == TAG_TAB) {
			elf_unload(io,v.x_tab,level+1);
		} else {
			fpf_value(io,v,1);
		}
	}
	fprintf(io,"}");
}


int core_lib_unload(elf_Shell *S) {
	elf_Handle io = elf_get_handle(S,0);
	elf_Table *tab = elf_get_table(S,1);
	elf_unload(io,tab,0);
	return 0;
}


// DEPRECATED
int core_lib_iton(elf_Shell *R) {
	elf_Value v = elf_get_arg(R,0);
	if (v.tag==TAG_INT) {
		elf_put_number(R,(elf_Num)v.x_int);
	} else elf_put_number(R,v.x_num);
	return 1;
}


// DEPRECATED
int core_lib_ntoi(elf_Shell *R) {
	elf_Value v = elf_get_arg(R,0);
	if (v.tag==TAG_NUM) {
		elf_put_integer(R,(elf_Int)v.x_num);
	} else elf_put_integer(R,v.x_int);
	return 1;
}


void elf_include_core_lib(elf_Shell *R) {
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

	elf_gsetx_cfn(R,"elf.set_object_metatable",core_lib_set_metatable);
	elf_gsetx_cfn(R,"elf.get_object_metatable",core_lib_get_metatable);
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
	elf_gsetx_cfn(R,"elf.loadfile",core_lib_load_file);
	elf_gsetx_cfn(R,"elf.unload",core_lib_unload);

	elf_gsetx_cfn(R,"elf.pf_indent",core_lib_pf_indent);
	elf_gsetx_cfn(R,"elf.pf",core_lib_pf);
	elf_gsetx_cfn(R,"elf.lpf",core_lib_lpf);


	elf_gsetx_cfn(R,"elf.change_work_dir",core_lib_change_work_dir);
	elf_gsetx_cfn(R,"elf.get_work_dir",core_lib_get_work_dir);
	/* todo: deprecate name */
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
