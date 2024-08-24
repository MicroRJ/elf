/*
** See Copyright Notice In elf.h
** elf-lib.c
** elf lib
*/


int elf_lib_pause_collector(elState *R) {
	R->memory.paused = 1;
	if (elGETNARGS(R) == 1) {
		R->memory.paused = elf_get_integer(R,0) != 0;
	}
	return 0;
}


typedef struct elObjectTracePath elObjectTracePath;
typedef struct elObjectTracePath {
	elObjectTracePath *trace;
	char *name;
} elObjectTracePath;

void print_object_trace(elObjectTracePath path) {
	if (path.trace) {
		print_object_trace(*path.trace);
		printf(".");
	}
	printf("%s",path.name);
}

#if 0
elInteger get_index_entry(elTable *tab, elInteger index) {
	elEntry *entry;
	for (entry = tab->entries; entry < tab->entries + tab->ntotal; entry += 1) {
		if (entry->key.tag != TAG_NIL) {
			if (entry->index == index) {
				return entry - tab->entries;
			}
		}
	}
	return -1;
}

elBool elf_trace_object(elState *S, elTable *visited, elObject *obj, elObject *thru, elObjectTracePath path) {
	if (elf_tset(visited,elOBJ(thru),elOBJ(thru))) {
		return 0;
	}
	elInteger traces = 0;
	if (thru == obj) {
		elf_debug_log("object found through: ");
		print_object_trace(path); printf("\n");
		traces = 1;
	} else if (thru->type == GC_CLS) {
		elObjectTracePath child = { &path, "closure.values" };
		elClosure *cls = (elClosure *) thru;
		FOR_RANGE(k,0,cls->proto.nlocals) {
			if (elISOBJTAG(cls->values[k].tag)) {
				traces += elf_trace_object(S,visited,obj,cls->values[k].x_obj,child);
			}
		}
	} else if (thru->type == GC_TAB) {
		elTable *tab = (elTable *) thru;

		{
			elObjectTracePath child = { &path, elf_tpf("(table.entry)") };
			elEntry *entry;
			for (entry = tab->entries; entry < tab->entries + tab->ntotal; entry += 1) {
				if (elISOBJTAG(entry->key.tag)) {
					elInteger found = elf_trace_object(S,visited,obj,entry->key.x_obj,child);
					traces += found;
				}
			}
		}

		{
			elObjectTracePath child = { &path, "(table.value)" };

			elValue *value;
			for (value = tab->array; value < tab->array + ARRAY_LENGTH(tab->array); value += 1) {
				if (elISOBJTAG(value->tag)) {
					elInteger found = elf_trace_object(S,visited,obj,value->x_obj,child);
					traces += found;
					if (found) {
						elInteger id = get_index_entry(tab,value-tab->array);
						if (id != -1) {
							elEntry entry = tab->entries[id];
							if (entry.key.tag == TAG_STR) {
								printf("(%i) by key: %s\n", tab->obj.color, entry.key.x_str->contents);
							} else {
								printf("(%i) by key: (not a string)\n", tab->obj.color);
							}
						} else {
							printf("(%i) by index: %lli\n", tab->obj.color, value-tab->array);
						}
					}
				}
			}
		}
	}
	return traces;
}


int elf_lib_trace_object(elState *R) {
	elObject *obj = elf_get_object(R,0);
	elTable *visited = elf_put_new_table(R);

	elObjectTracePath child = { 0 };
	child.name = "global";

	elInteger traces = elf_trace_object(R,visited,obj,(elObject*)R->M->globals,child);

	child.name = "stack";
	elValue *Ki;
	for (Ki = R->K; Ki < elGETTOP(R); ++ Ki) {
		if (elISOBJTAG(Ki->tag)) {
			traces += elf_trace_object(R,visited,obj,Ki->x_obj,child);
		}
	}
	elf_put_integer(R,traces);
	return 1;
}
#endif

int elf_lib_trace_object(elState *R) {
	return 0;
}


int elf_lib_get_allocated_objects(elState *R) {
	elf_put_integer(R,ARRAY_LENGTH(R->memory.objects));
	return 1;
}


int elf_lib_get_allocated_memory(elState *R) {
	elf_put_integer(R,R->memory.memory_allocated);
	return 1;
}


int elf_lib_get_collector_threshold(elState *R) {
	elf_put_integer(R,R->memory.memory_threshold);
	return 1;
}


int elf_lib_mark_everything(elState *R) {
	NO_CODE;
	// elInteger num = elf_hold_phase(R);
	// elf_put_integer(R,num);
	return 1;
}


int elf_lib_mark_object(elState *R) {
	elInteger num = elf_mark_object(elf_get_object(R,0));
	elf_put_integer(R,num);
	return 1;
}


int elf_lib_collect(elState *R) {
	elf_trigger_collection_cycle(R);
	return 0;
}


int elf_lib_tagof(elState *R) {
	elf_put_new_string(R,(char*)tag2s[elGETTAG(R,0)]);
	return 1;
}


int elf_lib_get_object_color(elState *R) {
	elObject *obj = elf_get_object(R,0);
	int color = obj->color;
	elf_put_new_string(R,
	color == GC_BLACK ? "black" :
	color == GC_WHITE ? "white" :
	color == GC_PINK  ? "pink"  :
	color == GC_RED   ? "red"   :
	color == GC_TRAP  ? "trap"  : "error");
	return 1;
}


int elf_lib_set_object_trap(elState *R) {
	elInteger set = elf_get_integer(R,1);
	elOBJCOLOR(elf_get_object(R,0)) = set ? GC_TRAP : GC_WHITE;
	return 0;
}


int elf_lib_get_object_address(elState *R) {
	elf_put_integer(R,(elInteger) (void *) elf_get_object(R,0));
	return 1;
}


int elf_lib_get_local_addr(elState *R) {
	#if 0
	elValue *local;
	local=&elGETARG(R,elf_get_integer(R,0));
	elf_put_integer(R,(elInteger)(local-R->stack));
	#endif
	NO_CODE;
	return 1;
}


/*
** Merges one or several tables together into
** a new table, which is then returned.
** todo: introduce merge keyword!
*/
int elf_lib_merge_tables(elState *R) {
	elTable *tab = elf_put_new_table(R);
	int i;
	for ( i = 0; i < elGETNARGS(R); i += 1 ) {
		elf_merge_tables(tab,elf_get_table(R,i));
	}
	return 1;
}


int elf_lib_get_metatable(elState *R) {
	elf_put_table(R,elf_get_object(R,0)->metatable);
	return 1;
}


int elf_lib_set_metatable(elState *R) {
	elf_get_object(R,0)->metatable = elf_get_table(R,1);
	elPUSH(R,elGETARG(R,0));
	return 1;
}


/* math */
int elflib_floor(elState *R) {
	elf_put_number(R,floor(elf_get_number(R,0)));
	return 1;
}


int elflib_ceil(elState *R) {
	elf_put_number(R,ceil(elf_get_number(R,0)));
	return 1;
}


int elf_lib_sqrt(elState *R) {
	elf_put_number(R,sqrt(elf_get_number(R,0)));
	return 1;
}


int elf_lib_pow(elState *R) {
	elf_put_number(R,pow(elf_get_number(R,0),elf_get_number(R,1)));
	return 1;
}


int elf_lib_sin(elState *R) {
	elf_put_number(R,sin(elf_get_number(R,0)));
	return 1;
}


int elf_lib_cos(elState *R) {
	elf_put_number(R,cos(elf_get_number(R,0)));
	return 1;
}


int elf_lib_acos(elState *R) {
	elf_put_number(R,acos(elf_get_number(R,0)));
	return 1;
}


int elf_lib_tan(elState *R) {
	elf_put_number(R,tan(elf_get_number(R,0)));
	return 1;
}


int elf_lib_atan2(elState *R) {
	elf_put_number(R,atan2(elf_get_number(R,0),elf_get_number(R,1)));
	return 1;
}


/*  */
int elflib_abort(elState *R) {
	if(1) abort();
	return 0;
}


int elflib_exit(elState *R) {
	if(1) exit(elf_get_integer(R,0));
	return 0;
}


/* debugging */
int elf_lib_flags(elState *R) {
	int flags = R->flags;
	R->flags |= elf_get_integer(R,0);
	elf_put_integer(R,flags);
	return 1;
}


int elf_lib_debugger(elState *R) {
#if defined(_DEBUG)
	R->flags |= FLAG_DEBUGGER;
#else
	char *message = "no message";
	if (elGETNARGS(R) != 0) {
		message = elf_get_text(R,0);
	}
	elf_debugger(message);
#endif
	return 0;
}


/* logging */
int elf_lib_log(elState *R) {
	FOR_RANGE(i,0,elGETNARGS(R)) {
		elf_fpf_value(stdout,elGETARG(R,i),0);
	}
	fprintf(stdout,"\n");
	return 0;
}


int elf_lib_err_log(elState *R) {
	FOR_RANGE(i,0,elGETNARGS(R)) {
		elf_fpf_value(stderr,elGETARG(R,i),0);
	}
	fprintf(stderr,"\n");
	return 0;
}


char *elf_insymdir(char *dir, char *sym) {
	do {
		if (*dir ++ != *sym ++) {
			return *sym == '.' ? dir : sym;
		}
	} while (*dir);
	return sym;
}


/* the dot is added so that symbols like elf.math.floor
are not mistaken with table accesses when shortened, math.floor != .math.floor */
int elf_lib_include(elState *R) {
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

	elTable *globals = R->M->globals;
	for (int i = 0; i < globals->ntotal; ++ i) {
		elEntry slot = globals->slots[i];
		if (slot.k.tag == TAG_STR) {
			char *sym = elf_insymdir(dir,slot.k.x_str->text);
			if (*sym != '.') continue;
			elString *ref = elf_new_string(R,sym);
			elf_tset(globals,elSTR(ref),globals->array[slot.i]);
			// elf_debug_log("added %s <- %s",ref->c,slot.k.x_str->c);
		}
	}
	return 0;
}


int elf_lib_load_expr(elState *R) {
	elString *filename = 0;
	elString *contents = 0;
	if (elGETNARGS(R) == 2) {
		filename = elf_get_string(R,0);
		contents = elf_get_string(R,1);
	} else if (elGETNARGS(R) == 1) {
		filename = elf_put_new_string(R,"unnamed");
		contents = elf_get_string(R,0);
	} else NO_CODE;
	NO_CODE;
	(void) filename;
	(void) contents;
	// elf_parse_expr3(R,filename,elGETFRAME(R)->ry,elGETFRAME(R)->ntoyield,contents);
	/* no need to do hoisting */
	return 0;
}


int elf_lib_load_code(elState *R) {
	elString *filename = 0;
	elString *contents = 0;
	if (elGETNARGS(R) == 2) {
		filename = elf_get_string(R,0);
		contents = elf_get_string(R,1);
	} else if (elGETNARGS(R) == 1) {
		filename = elf_put_new_string(R,"unnamed");
		contents = elf_get_string(R,0);
	} else NO_CODE;
	NO_CODE;
	(void) filename;
	(void) contents;
	// elf_parse_code3(R,filename,elGETFRAME(R)->ry,elGETFRAME(R)->ntoyield,contents);
	/* no need to do hoisting */
	return 0;
}


int elf_lib_load_file(elState *R) {
	elString *filename;
	int nresults;

	filename=elf_get_string(R,0);
	nresults=elf_load_file(R,filename,elGETFRAME(R)->nargs,elGETFRAME(R)->nregs);
	// if (nresults != -1 && !strcmp(filename->text,"patterns.elf")){
	// 	__debugbreak();
	// }
	// elf_copy_memory(R->frame->locals-1,R->stack_ptr-nresults,nresults*sizeof(elValue));
	return nresults;
}


int elflib_iton(elState *R) {
	elValue v = elGETARG(R,0);
	if (v.tag==TAG_INT) {
		elf_put_number(R,(elNumber)v.x_int);
	} else elf_put_number(R,v.x_num);
	return 1;
}


int elflib_ntoi(elState *R) {
	elValue v = elGETARG(R,0);
	if (v.tag==TAG_NUM) {
		elf_put_integer(R,(elInteger)v.x_num);
	} else elf_put_integer(R,v.x_int);
	return 1;
}


// typedef void (*em_dlopen_callback)(void* handle, void* user_data);
// void emscripten_dlopen(const char *filename, int flags, void* user_data, em_dlopen_callback onsuccess, em_arg_callback_func onerror);
int elf_lib_get_dll_fn(elState *S) {
	elHandle lib = elf_get_handle(S,0);
	char *name = elf_get_text(S,1);
	elCFunction fn = (elCFunction) sys_get_dll_fn(lib,name);

	if (fn != 0) elf_put_cfunction(S,fn);
	else elf_put_nil(S);
	return 1;
}


int elf_lib_load_dll(elState *R) {
	char *file = elf_get_text(R,0);
	elHandle lib = sys_load_dll(file);

	if (lib != 0) elf_put_handle(R,lib);
	else elf_put_nil(R);
	return 1;
}


int elf_lib_shell(elState *R) {
	char *verb = elf_get_text(R,0);
	char *file = elf_get_text(R,1);
	char *args = elf_get_text(R,2);
	elf_put_integer(R,sys_shell(verb,file,args));
	return 1;
}


int elf_lib_exec(elState *R) {
	char *cline=elf_get_text(R,0);
	int result=sys_exec(0,cline);
	elf_put_integer(R,result);
	return 1;
}


int elf_lib_fopen(elState *R) {
	ASSERT(elGETNARGS(R) == 2);
	char *name = elf_get_text(R,0);
	char *flags = elf_get_text(R,1);
	FILE *file = fopen(name,flags);
	elf_put_handle(R,(elHandle)file);
	return 1;
}


int elf_lib_fclose(elState *R) {
	ASSERT(elGETNARGS(R) == 1);
	elHandle file = elf_get_handle(R,0);
	fclose(file);
	return 0;
}


int elf_lib_fsize(elState *R) {
	if (elGETTAG(R,0) == TAG_SYS) {
		elHandle file = (FILE*) elf_get_handle(R,0);
		fseek(file,0,SEEK_END);
		elf_put_integer(R,ftell(file));
	} else NO_CODE;
	return 1;
}


/* todo: update to use sys layer */
int elf_lib_load_file_data(elState *R) {
	FILE *file = (FILE*) elf_get_handle(R,0);
	if (file != 0) {
		fseek(file,0,SEEK_END);
		long size = ftell(file);
		fseek(file,0,SEEK_SET);
		elString *buf = elf_new_lstring(R,size);
		fread(buf->text,1,size,file);
		elf_put_string(R,buf);
	} else elf_put_nil(R);
	return 1;
}


int elflib_ftemp(elState *R) {
	FILE *file = {0};
#if defined(PLATFORM_WEB)
	file = tmpfile();
#else
	tmpfile_s(&file);
#endif
	elf_put_handle(R,(elHandle)file);
	return 1;
}


int elf_lib_change_work_dir(elState *R) {
	int ok = sys_set_work_dir(elf_get_text(R,0));
	elf_put_integer(R,ok);
	return 1;
}


int elflib_get_work_dir(elState *R) {
	char buf[MAX_PATH];
	sys_get_work_dir(sizeof(buf),buf);
	elf_put_new_string(R,buf);
	return 1;
}


int elf_fpf_value(FILE *file, elValue v, elBool quotes) {
	switch (v.tag) {
		case TAG_NIL: return fprintf(file,"nil");
		case TAG_SYS: return fprintf(file,"h%llX",v.x_int);
		case TAG_INT: return fprintf(file,"%lli",v.x_int);
		case TAG_NUM: return fprintf(file,"%f",v.x_num);
		case TAG_CLS: return fprintf(file,"F()");
		case TAG_CFN: return fprintf(file,"C()");
		case TAG_TAB: {
			int wrote = 0;
			elTable *tab = v.x_tab;
			wrote += fprintf(file,"{");
			elInteger i,j,n;
			for (i=0;i<ARRAY_LENGTH(tab->array);++i) {
				if (i != 0) wrote += fprintf(file,", ");
				for (j=0,n=0;j<tab->ntotal;++j) {
					elEntry it = tab->slots[j];
					if (it.k.tag == TAG_NIL) continue;
					if (it.i != i) continue;
					if (n ++ != 0) wrote += fprintf(file,", ");
					wrote += elf_fpf_value(file,it.k,1);
				}
				if (n != 0) wrote += fprintf(file," = ");
				wrote += elf_fpf_value(file,tab->array[i],1);
			}
			// for (i=0,n=0;i<tab->nslots;++i) {
			// 	elEntry it = tab->slots[i];
			// 	if (it.k.tag == TAG_NIL) continue;
			// 	if (n ++ != 0) wrote += fprintf(file,", ");
			// 	wrote += elf_fpf_value(file,it.k,1);
			// 	wrote += fprintf(file," = ");
			// 	wrote += elf_fpf_value(file,tab->array[it.i],1);
			// }
			// FOR_ARRAY(t->v) {
			// 	if (i != 0) wrote += fprintf(file,", ");
			// 	wrote += elf_fpf_value(file,t->v[i],1);
			// }
			wrote += fprintf(file,"}");
			return wrote;
		} break;
		case TAG_STR: {
			if (quotes) {
				return fprintf(file,"\"%s\"",v.x_str->text);
			} else {
				return fprintf(file,"%s",v.x_str->text);
			}
		} break;
		default: return fprintf(file,"(?)");
	}
}


int elflib_fpf(elState *S) {
	elHandle file = elf_get_handle(S,0);
	int wrote = 0;
	for (int i = 1; i < elGETNARGS(S); i ++) {
		wrote += elf_fpf_value(file,elGETARG(S,i),0);
	}
	elf_put_integer(S,wrote);
	return 1;
}


elGLOBAL int pf_indent;
elGLOBAL int pf_char;
int elf_lib_pf_indent(elState *S) {
	pf_indent = elf_get_integer(S,0);
	elf_put_integer(S,pf_indent);
	return 1;
}


int elf_lib_lpf(elState *S) {
	for (int i = 0; i < elGETNARGS(S); i ++) {
		if (i != 0) fprintf(stdout,"\n");
		for (int j = 0; j < pf_indent; ++ j) {
			fprintf(stdout, "  ");
		}
		elf_fpf_value(stdout,elGETARG(S,i),0);
	}
	fprintf(stdout,"\n");
	return 0;
}


int elf_lib_pf(elState *S) {
	for (int j = 0; j < pf_indent; ++ j) {
		fprintf(stdout,"  ");
	}
	for (int i = 0; i < elGETNARGS(S); i ++) {
		elf_fpf_value(stdout,elGETARG(S,i),0);
	}
	fprintf(stdout,"\n");
	return 0;
}


elAPI int elf_lib_sleep(elState *S) {
	ASSERT(elGETNARGS(S) >= 1);
	sys_sleep(elf_get_integer(S,0));
	return 0;
}


elAPI int elflib_clocktime(elState *rt) {
	elf_put_integer(rt,sys_get_clock_time());
	return 1;
}


elAPI int elf_lib_timediffs(elState *S) {
	ASSERT(elGETNARGS(S) == 1);
	elInteger time = elf_get_integer(S,0);
	elf_put_number(S,elf_time_diff_s(time));
	return 1;
}


elAPI int elf_lib_timediffms(elState *S) {
	ASSERT(elGETNARGS(S) == 1);
	elInteger time = elf_get_integer(S,0);
	elf_put_number(S,elf_time_diff_ms(time));
	return 1;
}


int elf_lib_get_file_size(elState *S) {
	char *path = elf_get_text(S,0);
	elInteger size = -1;
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


elBool elf_is_virtual_file_name(char const *fn) {
	while (*fn == '.') ++ fn;
	return *fn == 0;
}


elAPI int elf_lib_get_disk_info(elState *R) {
	elTable *info = elf_put_new_table(R);
#if 0
	DWORD SectorsPerCluster;
	DWORD BytesPerSector;
	DWORD NumberOfFreeClusters;
	DWORD TotalNumberOfClusters;
	GetDiskFreeSpaceA(elf_get_text(R,0),&SectorsPerCluster,&BytesPerSector,&NumberOfFreeClusters,&TotalNumberOfClusters);
	elf_tsets_int(info,elf_new_string(R,"SectorsPerCluster"),SectorsPerCluster);
	elf_tsets_int(info,elf_new_string(R,"BytesPerSector"),BytesPerSector);
	elf_tsets_int(info,elf_new_string(R,"NumberOfFreeClusters"),NumberOfFreeClusters);
	elf_tsets_int(info,elf_new_string(R,"TotalNumberOfClusters"),TotalNumberOfClusters);
#else
	elf_debug_log("this function is not implemented for this platform");
#endif
	return 1;
}


elAPI int elf_lib_list_volumes(elState *R) {
	elTable *list = elf_put_new_table(R); /* <- */
#if 0
	char buffer[MAX_PATH];
	HANDLE handle = FindFirstVolumeA(buffer,MAX_PATH);

	elString *name = 0;
	if (handle != INVALID_HANDLE_VALUE) do {

		elTable *volume = elf_put_new_table(R);
		name = elf_put_new_string(R,buffer);

		elf_tsets_tab(list,name,volume);

		elf_tsets_str(volume,elf_new_string(R,"name"),name);

		elTable *path_names = elf_put_new_table(R);
		elf_tsets_tab(volume,elf_new_string(R,"path_names"),path_names);

		if (GetVolumePathNamesForVolumeNameA(name->text,buffer,MAX_PATH,NULL)) {
			char *cursor = buffer;
			while (*cursor != '\0') {
				elf_tadd(path_names,elSTR(elf_new_string(R,buffer)));
				cursor += strlen(cursor) + 1;
			}
		}
	} while(FindNextVolumeA(handle,buffer,MAX_PATH));
	FindVolumeClose(handle);
#endif

	elf_put_table(R,list); /* <- */
	return 1;
}


void elf_lib_list_folder_(elState *R, elTable *list, int level, elString *dir);
elAPI int elf_lib_list_folder(elState *R) {
	ASSERT(elGETNARGS(R) > 0);
	elString *dir = elf_get_string(R,0);
	elInteger level = 0;
	if (elGETNARGS(R) > 1) {
		level = elf_get_integer(R,1);
	}
	elTable *list = elf_put_new_table(R);
	elf_lib_list_folder_(R,list,level,dir);
	elf_put_table(R,list);
	return 1;
}


/* Use -1 for recursive always, 0 for just this layer */
void elf_lib_list_folder_(elState *R, elTable *list, int level, elString *dir) {
#if 0
	// defined(_WIN32)
	WIN32_FIND_DATAA f;
	HANDLE h = FindFirstFileA(elf_tpf("%s\\*",dir->c),&f);
	if (h != INVALID_HANDLE_VALUE) do {
		if (elf_is_virtual_file_name(f.cFileName)) continue;
		int is_directory = 0 != (f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
		elValue *top = elGETTOP(R);
		elString *name = elf_put_new_string(R,f.cFileName);
		elString *path = elf_put_new_string(R,elf_tpf("%s\\%s",dir->c,f.cFileName));
		elTable *file = elf_put_new_table(R);
		elf_tsets_str(file,elf_put_new_string(R,"name"),name);
		elf_tsets_str(file,elf_put_new_string(R,"path"),path);
		elf_tsets_int(file,elf_put_new_string(R,"is_directory"),is_directory);
		elf_tsets_int(file,elf_put_new_string(R,"size"),f.nFileSizeLow);
		elf_tadd(list,elTAB(file));
		if (level != 0) {
			if (is_directory) {
				elf_lib_list_folder_(R,list,level-1,path);
			}
		}
		elSETTOP(R,top);
	} while (FindNextFileA(h,&f));
#else
	elf_fail(R,NO_BYTE,"unsupported platform");
#endif
}


void elf_lib_enumerate_directory_(elState *R, elString *dir, elClosure *cls) {
#if 0
// defined(PLATFORM_DESKTOP)
	WIN32_FIND_DATAA f;
	HANDLE h = FindFirstFileA(elf_tpf("%s\\*",dir->c),&f);
	if (h != INVALID_HANDLE_VALUE) do {
		if (elf_is_virtual_file_name(f.cFileName)) continue;
		int is_directory = 0 != (f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
		elValue *top = elGETTOP(R);
		elString *name = elf_put_new_string(R,f.cFileName);
		elString *path = elf_put_new_string(R,elf_tpf("%s\\%s",dir->c,f.cFileName));
		elTable *file = elf_put_new_table(R);
		elf_tsets_str(file,elf_put_new_string(R,"name"),name);
		elf_tsets_str(file,elf_put_new_string(R,"path"),path);
		elf_tsets_int(file,elf_put_new_string(R,"is_directory"),is_directory);
		elf_tsets_int(file,elf_put_new_string(R,"size"),f.nFileSizeLow);
		elf_put_closure(R,cls);
		int results = 0; NO_CODE; // elf_call_function(R,base,1,1);
		// if (is_directory) {
		// 	if ((results > 0) && elf_get_integer(R,base) != 0) {
		// 		elf_lib_enumerate_directory_(R,path,cls);
		// 	}
		// }
		elSETTOP(R,top);
	} while (FindNextFileA(h,&f));
#endif
#if 0
	// defined(PLATFORM_WEB)
	DIR *dirfd = opendir(dir->c);
	if (dirfd != 0) {
		struct dirent *entry;
		while ((entry = readdir(dirfd)) != 0) {
			if (elf_is_virtual_file_name(entry->d_name)) {
				continue;
			}
			elBool isdir = (entry->d_type & DT_DIR) != 0;
			elValue *top = elGETTOP(R);

			elString *name = elf_put_new_string(R,entry->d_name);
			elString *path = elf_put_new_string(R,elf_tpf("%s/%s",dir->c,entry->d_name));
			elRegId base = elf_put_closure(R,cls);
			elTable *file = elf_put_new_table(R);

			elf_tsets_str(file,elf_put_new_string(R,"name"),name);
			elf_tsets_str(file,elf_put_new_string(R,"path"),path);
			elf_tsets_int(file,elf_put_new_string(R,"isdir"),isdir);
			int r = elf_call_function(R,base,1,1);
			if ((r > 0) && isdir && elf_get_integer(R,base)) {
				elf_lib_enumerate_directory_(R,path,cls);
			}
			elSETTOP(R,top);
		}
		closedir(dirfd);
	}
#endif
}


elAPI int elf_lib_enumerate_directory(elState *R) {
	ASSERT(elGETNARGS(R) == 2);
	/* push these keys temporarily so they won't
	be gc'd and also to to avoid creating them so often  */

	elString *dir = elf_get_string(R,0);
	elClosure *cls = elf_get_closure(R,1);
	elf_lib_enumerate_directory_(R,dir,cls);
	return 1;
}


void ftabs(FILE *io, int level) {
	while (level --) fprintf(io,"\t");
}


void elf_unload(FILE *io, elTable *tab, int level) {
	fprintf(io,"{");
	int nitems = 0;
	for (elInteger i = 0; i < tab->ntotal; ++ i) {
		elEntry slot = tab->slots[i];
		if (slot.k.tag == TAG_NIL) {
			continue;
		}
		elValue v = tab->array[slot.i];
		if ((v.tag == TAG_CLS) || (v.tag == TAG_CFN)) {
			continue;
		}
		if (nitems ++ != 0) fprintf(io,",");
		elf_fpf_value(io,slot.k,1);
		fprintf(io," = ");
		if (v.tag == TAG_TAB) {
			elf_unload(io,v.x_tab,level+1);
		} else {
			elf_fpf_value(io,v,1);
		}
	}
	fprintf(io,"}");
}


int elf_lib_unload(elState *S) {
	elHandle io = elf_get_handle(S,0);
	elTable *tab = elf_get_table(S,1);
	elf_unload(io,tab,0);
	return 0;
}


elAPI void elf_lib_load_functions(elState *R) {
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

	elf_gsetx_cfn(R,"elf.flags",elf_lib_flags);
	elf_gsetx_cfn(R,"elf.debugger",elf_lib_debugger);

	elf_gsetx_cfn(R,"elf.merge_tables",elf_lib_merge_tables);

	elf_gsetx_cfn(R,"elf.pause_collector",elf_lib_pause_collector);
	elf_gsetx_cfn(R,"elf.trace_object",elf_lib_trace_object);
	elf_gsetx_cfn(R,"elf.mark_object",elf_lib_mark_object);
	elf_gsetx_cfn(R,"elf.mark_everything",elf_lib_mark_everything);
	elf_gsetx_cfn(R,"elf.get_collector_threshold",elf_lib_get_collector_threshold);
	elf_gsetx_cfn(R,"elf.get_allocated_objects",elf_lib_get_allocated_objects);
	elf_gsetx_cfn(R,"elf.get_allocated_memory",elf_lib_get_allocated_memory);
	elf_gsetx_cfn(R,"elf.collect",elf_lib_collect);

	elf_gsetx_cfn(R,"elf.tagof",elf_lib_tagof);

	elf_gsetx_cfn(R,"elf.set_object_metatable",elf_lib_set_metatable);
	elf_gsetx_cfn(R,"elf.get_object_metatable",elf_lib_get_metatable);
	elf_gsetx_cfn(R,"elf.get_object_address",elf_lib_get_object_address);
	elf_gsetx_cfn(R,"elf.get_object_color",elf_lib_get_object_color);
	elf_gsetx_cfn(R,"elf.set_object_trap",elf_lib_set_object_trap);
	elf_gsetx_cfn(R,"elf.get_local_addr",elf_lib_get_local_addr);


	elf_gsetx_cfn(R,"elf.log",elf_lib_log);
	elf_gsetx_cfn(R,"elf.err",elf_lib_err_log);

	/* todo: these should be intrinsic */
	elf_gsetx_cfn(R,"ntoi",elflib_ntoi);
	elf_gsetx_cfn(R,"iton",elflib_iton);

	elf_gsetx_cfn(R,"elf.clocktime",elflib_clocktime);
	elf_gsetx_cfn(R,"elf.timediffs",elf_lib_timediffs);
	elf_gsetx_cfn(R,"elf.timediffms",elf_lib_timediffms);

	elf_gsetx_cfn(R,"elf.loadlib",elf_lib_load_dll);
	elf_gsetx_cfn(R,"elf.libfn",elf_lib_get_dll_fn);

	elf_gsetx_cfn(R,"elf.include",elf_lib_include);
	elf_gsetx_cfn(R,"elf.loadcode",elf_lib_load_code);
	elf_gsetx_cfn(R,"elf.loadexpr",elf_lib_load_expr);
	elf_gsetx_cfn(R,"elf.loadfile",elf_lib_load_file);
	elf_gsetx_cfn(R,"elf.unload",elf_lib_unload);

	elf_gsetx_cfn(R,"elf.pf_indent",elf_lib_pf_indent);
	elf_gsetx_cfn(R,"elf.pf",elf_lib_pf);
	elf_gsetx_cfn(R,"elf.lpf",elf_lib_lpf);


	elf_gsetx_cfn(R,"elf.change_work_dir",elf_lib_change_work_dir);
	elf_gsetx_cfn(R,"elf.get_work_dir",elflib_get_work_dir);
	/* todo: deprecate name */
	elf_gsetx_cfn(R,"elf.enumerate_directory",elf_lib_enumerate_directory);
	elf_gsetx_cfn(R,"elf.enumerate_folder",elf_lib_enumerate_directory);
	elf_gsetx_cfn(R,"elf.list_folder",elf_lib_list_folder);
	elf_gsetx_cfn(R,"elf.list_volumes",elf_lib_list_volumes);
	elf_gsetx_cfn(R,"elf.get_disk_info",elf_lib_get_disk_info);

	elf_gsetx_sys(R,"elf.ferr",stderr);
	elf_gsetx_sys(R,"elf.fout",stdout);
	elf_gsetx_sys(R,"elf.fin",stdin);

	elf_gsetx_cfn(R,"elf.pf",elf_lib_pf);
	elf_gsetx_cfn(R,"elf.fpf",elflib_fpf);
	elf_gsetx_cfn(R,"elf.fload",elf_lib_load_file_data);
	elf_gsetx_cfn(R,"elf.ftemp",elflib_ftemp);
	elf_gsetx_cfn(R,"elf.fopen",elf_lib_fopen);
	elf_gsetx_cfn(R,"elf.fclose",elf_lib_fclose);
	elf_gsetx_cfn(R,"elf.fsize",elf_lib_fsize);
	elf_gsetx_cfn(R,"elf.get_file_size",elf_lib_get_file_size);

	elf_gsetx_cfn(R,"floor",elflib_floor);
	elf_gsetx_cfn(R,"ceil",elflib_ceil);
	elf_gsetx_cfn(R,"sqrt",elf_lib_sqrt);
	elf_gsetx_cfn(R,"pow",elf_lib_pow);
	elf_gsetx_cfn(R,"sin",elf_lib_sin);
	elf_gsetx_cfn(R,"cos",elf_lib_cos);
	elf_gsetx_cfn(R,"acos",elf_lib_acos);
	elf_gsetx_cfn(R,"tan",elf_lib_tan);
	elf_gsetx_cfn(R,"atan2",elf_lib_atan2);

	elf_gsetx_cfn(R,"elf.sleep",elf_lib_sleep);
	elf_gsetx_cfn(R,"elf.exec",elf_lib_exec);
	elf_gsetx_cfn(R,"elf.shell",elf_lib_shell);
}
