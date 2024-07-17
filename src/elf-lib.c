/*
** See Copyright Notice In elf.h
** elf-lib.c
** elf lib
*/


int elf_lib_pause_collector(elState *R) {
	R->memory.paused = elTrue;
	if (elf_get_num_args(R) == 1) {
		R->memory.paused = elf_get_integer(R,0) != 0;
	}
	return 0;
}


int elf_lib_get_allocated_objects(elState *R) {
	elf_add_integer(R,elf_xarray_length(R->memory.objects));
	return 1;
}


int elf_lib_get_allocated_memory(elState *R) {
	elf_add_integer(R,R->memory.allocated);
	return 1;
}


int elf_lib_get_collector_threshold(elState *R) {
	elf_add_integer(R,R->memory.threshold);
	return 1;
}


int elf_lib_mark_everything(elState *R) {
	elInteger num = elf_mark_everything(R);
	elf_add_integer(R,num);
	return 1;
}


int elf_lib_mark_object(elState *R) {
	elInteger num = elf_mark_object(elf_get_object(R,0));
	elf_add_integer(R,num);
	return 1;
}


int elf_lib_unmark_objects(elState *R) {
	elInteger num = elf_unmark_objects(R);
	elf_add_integer(R,num);
	return 1;
}


int elf_lib_collect(elState *R) {
	elf_collect(R);
	return 0;
}


int elf_lib_get_value_tag(elState *R) {
	elf_add_new_string(R,(char*)tag2s[elf_get_tag(R,0)]);
	return 1;
}


int elf_lib_get_object_color(elState *R) {
	int color = elf_get_object(R,0)->color;
	elf_add_new_string(R,
	color == GC_BLACK ? "black" :
	color == GC_WHITE ? "white" :
	color == GC_PINK  ? "pink"  :
	color == GC_RED   ? "red"   : "error");
	return 1;
}


int elf_lib_set_object_trap(elState *R) {
	elf_get_object(R,0)->color = GC_PINK;
	return 1;
}


int elf_lib_get_object_address(elState *R) {
	elf_add_integer(R,(elInteger) (void *) elf_get_object(R,0));
	return 1;
}


/*
** Merges one or several tables together into
** a new table, which is then returned.
*/
int elf_lib_merge_tables(elState *R) {
	elTable *tab = elf_add_new_table(R);
	int i;
	for (i=1;i<elf_get_num_args(R);++i) {
		elf_merge_tables(tab,elf_get_table(R,i));
	}
	return 1;
}


int elf_lib_get_metatable(elState *R) {
	elf_add_table(R,elf_get_object(R,0)->metatable);
	return 1;
}


int elf_lib_set_metatable(elState *R) {
	elf_get_object(R,0)->metatable = elf_get_table(R,1);
	elf_add_value(R,elf_get_value(R,0));
	return 1;
}


/* math */
int elflib_floor(elState *R) {
	elf_add_number(R,floor(elf_get_number(R,0)));
	return 1;
}


int elf_lib_sqrt(elState *R) {
	elf_add_number(R,sqrt(elf_get_number(R,0)));
	return 1;
}


int elf_lib_pow(elState *R) {
	elf_add_number(R,pow(elf_get_number(R,0),elf_get_number(R,1)));
	return 1;
}


int elf_lib_sin(elState *R) {
	elf_add_number(R,sin(elf_get_number(R,0)));
	return 1;
}


int elf_lib_cos(elState *R) {
	elf_add_number(R,cos(elf_get_number(R,0)));
	return 1;
}


int elf_lib_acos(elState *R) {
	elf_add_number(R,acos(elf_get_number(R,0)));
	return 1;
}


int elf_lib_tan(elState *R) {
	elf_add_number(R,tan(elf_get_number(R,0)));
	return 1;
}


int elf_lib_atan2(elState *R) {
	elf_add_number(R,atan2(elf_get_number(R,0),elf_get_number(R,1)));
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
int elflib_bytelogging(elState *R) {
	elf_add_integer(R,R->call->caller->logging);
	R->call->caller->logging = elf_get_integer(R,0);
	return 0;
}


int elflib_globalbytelogging(elState *R) {
	elf_add_integer(R,R->bytelogging);
	R->bytelogging = elf_get_integer(R,0);
	return 0;
}


int elflib_oncalldebugger(elState *R) {
	R->oncalldebuggerflag = elf_get_integer(R,0);
	return 0;
}


int elflib_debugger(elState *R) {
#if defined(_DEBUG)
	R->debuggerflag = elTrue;
#else
	char *message = "no message";
	if (elf_get_num_args(R) != 0) {
		message = elf_get_cstring(R,0);
	}
	elf_debugger(message);
#endif
	return 0;
}


/* logging */
int elflib_log(elState *R) {
	for (int i = 0; i < R->call->nx; i ++) {
		elf_fpf_value(stdout,elf_get_value(R,i),false);
	} printf("\n");
	return 0;
}


int elflib_err(elState *R) {
	for (int i = 0; i < R->call->nx; i ++) {
		elf_fpf_value(stdout,elf_get_value(R,i),false);
	} printf("\n");
	return 0;
}


/* the include function merges or includes
** the given symbol tree into the
** global symbol tree.
**
** Usage:
** 	elf.pf("before")
** 	elf.include("elf")
** 	.pf("after")
**
**
** REMARKS: useful for when you want to
** avoid having to access a symbol
** explicitly each time.
** - Note that this will not improve
** performance in any way, if anything
** it'll augment the number of symbols
** in the global directory.
** - Also note that this is done at runtime
** and you should use especial for this function.
**
** The include function finds all the symbols
** within the given directory and for each
** target symbol it creates a new one in the
** root directory that points to the \value
** of the target symbol.
**
** For instance elf.include("elf")
** finds all symbols within elf.* and
** adds them to the root directory,
** now pf is accessible through '.pf'
** as supposed to 'elf.pf'.
** Note how there's a '.' before pf,
** this is for a reason.
** The dot remains so that the loader
** knows beforehand that you're
** explicitly referring to a symbol.
**
** If the symbol is a single word there's
** no need to add the '.' because the
** loader would know, but there are some
** drawbacks and edge cases that come
** with this, for instance:
**
** 1) It becomes less clear what you're
** referring to.
** 2) It could conflict with a local or
** an already defined global with the
** same name.
** 3) It is not consistent.
** So include will always keep the '.'
** to make it explicit to the loader
** that you're referring to a global
** symbol.
** For the elf directory, elf is a
** reserved keyword, and so the loader
** already knows you're referring to
** the elf directory.
** So in other words, using '.' avoids
** semantic ambiguities and inconsistencies.
** Here's one example:
** The directory elf.ray contains:
** BlendMode.ADDITIVE, to access ADDITIVE
** you'd do: elf.ray.BlendMode.ADDITIVE,
** that's long name, you'd rather have a
** shortcut, so you do:
** elf.include("elf.ray")
** Now, elf.include happens at runtime so
** the loader has no idea of what you just
** did, and besides, given the dynamic nature
** of the language it has no way of knowing
** what it is you're referring to precisely.
** However, you can still do:
** .BlendMode.ADDITIVE,
** because '.' is a language feature that
** tells the loader: Hey, this is a symbol
** not a field reference and not some local.
** So the loader links to the global as
** it is '.BlendMode.ADDITIVE', trusting
** that it'll be bound at runtime before,
** it's first use, and effectively, it gets
** bound by the previous include.
*/



char *elf_insymdir(char *dir, char *sym) {
	do {
		if (*dir ++ != *sym ++) {
			return *sym == '.' ? dir : sym;
		}
	} while (*dir);
	return sym;
}


int elflib_include(elState *R) {
	elf_check_args(R,".include",1,"(the directory to include to add to the global directory)");
	char *dir = elf_get_cstring(R,0);
	int plen = elf_cstrlen(dir);
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
			char *sym = elf_insymdir(dir,slot.k.x_str->c);
			if (*sym != '.') continue;
			elString *ref = elf_new_string(R,sym);
			elf_table_insert(globals,elf_string_value(ref),globals->array[slot.i]);
			// elf_debug_log("added %s <- %s",ref->c,slot.k.x_str->c);
		}
	}
	return 0;
}


int elflib_loadexpr(elState *R) {
	elString *filename = elNil,*contents = elNil;
	if (R->call->nx == 2) {
		filename = elf_get_string(R,0);
		contents = elf_get_string(R,1);
	} else if (R->call->nx == 1) {
		filename = elf_add_new_string(R,"unnamed");
		contents = elf_get_string(R,0);
	} else elf_unreachable;
	elf_loadexpr(R,filename,R->call->ry,R->call->ny,contents->c);
	/* no need to do hoisting */
	return 0;
}


int elflib_loadcode(elState *R) {
	elString *filename = elNil,*contents = elNil;
	if (R->call->nx == 2) {
		filename = elf_get_string(R,0);
		contents = elf_get_string(R,1);
	} else if (R->call->nx == 1) {
		filename = elf_add_new_string(R,"unnamed");
		contents = elf_get_string(R,0);
	} else elf_unreachable;
	elf_loadcode(R,filename,R->call->ry,R->call->ny,contents->c);
	/* no need to do hoisting */
	return 0;
}


int elflib_loadfile(elState *R) {
	elString *filename = elf_get_string(R,0);
	elf_loadfile(R,filename,R->call->ry,R->call->ny);
	/* no need to do hoisting */
	return 0;
}


int elflib_iton(elState *R) {
	elValue v = elf_get_value(R,0);
	if (v.tag == TAG_INT) {
		elf_add_number(R,(elNumber)v.i);
	} else elf_add_number(R,v.n);
	return 1;
}


int elflib_ntoi(elState *R) {
	elValue v = elf_get_value(R,0);
	if (v.tag == TAG_NUM) {
		elf_add_integer(R,(elInteger)v.n);
	} else elf_add_integer(R,v.i);
	return 1;
}


// typedef void (*em_dlopen_callback)(void* handle, void* user_data);
// void emscripten_dlopen(const char *filename, int flags, void* user_data, em_dlopen_callback onsuccess, em_arg_callback_func onerror);
int elflib_libfn(elState *rt) {
	elHandle lib = elf_get_handle(rt,0);
	elString *name = elf_get_string(rt,1);
	elBinding fn = (elBinding) sys_libfn(lib,name->c);
	if (fn != elNil) {
		elf_pushbinding(rt,fn);
	} else {
		elf_pushnil(rt);
	}
	return 1;
}


int elflib_loadlib(elState *R) {
	elString *name = elf_get_string(R,0);
	elClosure *callback = elf_get_closure(R,1);
	elHandle lib = sys_loadlib(name->c);
	if (lib != elNil) elf_pushsys(R,lib);
	else elf_pushnil(R);
	return 1;
}


int elflib_exec(elState *R) {
#if defined(_WIN32)
	elString *cmd = elf_get_string(R,0);
	STARTUPINFO si = {sizeof(si)};
	PROCESS_INFORMATION pi = {0};
	int result = CreateProcess(NULL,cmd->c,NULL,NULL,FALSE,0,NULL,NULL,&si,&pi);
	elf_add_integer(R,result);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
#else
	elf_add_integer(R,0);
#endif
	return 1;
}


int elf_lib_fopen(elState *R) {
	elf_ensure(R->call->nx == 2);
	char *name = elf_get_cstring(R,0);
	char *flags = elf_get_cstring(R,1);
	FILE *file = fopen(name,flags);
	elf_pushsys(R,(elHandle)file);
	return 1;
}


int elf_lib_fclose(elState *R) {
	elf_ensure(R->call->nx == 1);
	elHandle file = elf_get_handle(R,0);
	fclose(file);
	return 0;
}


int elf_lib_fsize(elState *R) {
	if (elf_get_tag(R,0) == TAG_SYS) {
		elHandle file = (FILE*) elf_get_handle(R,0);
		fseek(file,0,SEEK_END);
		elf_add_integer(R,ftell(file));
	} else elf_unreachable;
	return 1;
}


int elflib_fload(elState *R) {
	FILE *file = (FILE*) elf_get_handle(R,0);
	if (file != 0) {
		fseek(file,0,SEEK_END);
		long size = ftell(file);
		fseek(file,0,SEEK_SET);
		elString *buf = elf_new_string_of_length(R,size);
		fread(buf->c,1,size,file);
		elf_add_string(R,buf);
	} else elf_pushnil(R);
	return 1;
}


int elflib_ftemp(elState *R) {
	FILE *file = {0};
#if defined(PLATFORM_WEB)
	file = tmpfile();
#else
	tmpfile_s(&file);
#endif
	elf_pushsys(R,(elHandle)file);
	return 1;
}


int elflib_change_work_dir(elState *R) {
	sys_changeworkdir(elf_get_cstring(R,0));
	return 1;
}


int elflib_get_work_dir(elState *R) {
	char buf[MAX_PATH];
	sys_getworkdir(sizeof(buf),buf);
	elf_add_new_string(R,buf);
	return 1;
}


int elf_fpf_value(FILE *file, elValue v, elBool quotes) {
	switch (v.tag) {
		case TAG_NIL: return fprintf(file,"nil");
		case TAG_SYS: return fprintf(file,"h%llX",v.i);
		case TAG_INT: return fprintf(file,"%lli",v.i);
		case TAG_NUM: return fprintf(file,"%f",v.n);
		case TAG_CLS: return fprintf(file,"F()");
		case TAG_BID: return fprintf(file,"C()");
		case TAG_TAB: {
			int wrote = 0;
			elTable *tab = v.x_tab;
			wrote += fprintf(file,"{");
			elInteger i,j,n;
			for (i=0;i<elf_xarray_length(tab->array);++i) {
				if (i != 0) wrote += fprintf(file,", ");
				for (j=0,n=0;j<tab->ntotal;++j) {
					elEntry it = tab->slots[j];
					if (it.k.tag == TAG_NIL) continue;
					if (it.i != i) continue;
					if (n ++ != 0) wrote += fprintf(file,", ");
					wrote += elf_fpf_value(file,it.k,elTrue);
				}
				if (n != 0) wrote += fprintf(file," = ");
				wrote += elf_fpf_value(file,tab->array[i],elTrue);
			}
			// for (i=0,n=0;i<tab->nslots;++i) {
			// 	elEntry it = tab->slots[i];
			// 	if (it.k.tag == TAG_NIL) continue;
			// 	if (n ++ != 0) wrote += fprintf(file,", ");
			// 	wrote += elf_fpf_value(file,it.k,elTrue);
			// 	wrote += fprintf(file," = ");
			// 	wrote += elf_fpf_value(file,tab->array[it.i],elTrue);
			// }
			// elf_xarray_foreachi(t->v) {
			// 	if (i != 0) wrote += fprintf(file,", ");
			// 	wrote += elf_fpf_value(file,t->v[i],elTrue);
			// }
			wrote += fprintf(file,"}");
			return wrote;
		} break;
		case TAG_STR: {
			if (quotes) {
				return fprintf(file,"\"%s\"",v.s->string);
			} else {
				return fprintf(file,"%s",v.s->string);
			}
		} break;
		default: return fprintf(file,"(?)");
	}
}


int elflib_fpf(elState *S) {
	elHandle file = elf_get_handle(S,0);
	int wrote = 0;
	for (int i = 1; i < elf_get_num_args(S); i ++) {
		wrote += elf_fpf_value(file,elf_get_value(S,i),false);
	}
	elf_add_integer(S,wrote);
	return 1;
}


int elf_lib_pf_indent(elState *S) {
	S->lib.pf_indent = elf_get_integer(S,0);
	elf_add_integer(S,S->lib.pf_indent);
	return 1;
}


int elf_lib_lpf(elState *S) {
	for (int i = 0; i < elf_get_num_args(S); i ++) {
		if (i != 0) fprintf(stdout,"\n");
		for (int j = 0; j < S->lib.pf_indent; ++ j) {
			fprintf(stdout, "  ");
		}
		elf_fpf_value(stdout,elf_get_value(S,i),false);
	}
	fprintf(stdout,"\n");
	return 0;
}


int elf_lib_pf(elState *S) {
	for (int j = 0; j < S->lib.pf_indent; ++ j) {
		fprintf(stdout,"  ");
	}
	for (int i = 0; i < elf_get_num_args(S); i ++) {
		elf_fpf_value(stdout,elf_get_value(S,i),false);
	}
	fprintf(stdout,"\n");
	return 0;
}


elf_api int elflib_sleep(elState *rt) {
	elf_ensure(rt->f->x == 1);
	sys_sleep(elf_get_integer(rt,0));
	return 0;
}


elf_api int elflib_clocktime(elState *rt) {
	elf_add_integer(rt,sys_clocktime());
	return 1;
}


elf_api int elf_lib_timediffs(elState *S) {
	elf_ensure(elf_get_num_args(S) == 1);
	elInteger time = elf_get_integer(S,0);
	elf_add_number(S,elf_timediffs(time));
	return 1;
}


elf_api int elf_lib_timediffms(elState *S) {
	elf_ensure(elf_get_num_args(S) == 1);
	elInteger time = elf_get_integer(S,0);
	elf_add_number(S,elf_timediffms(time));
	return 1;
}


elBool elf_is_virtual_file_name(char const *fn) {
	while (*fn == '.') ++ fn;
	return *fn == 0;
}


void elf_lib_list_folder_(elState *R, elTable *list, int level, elString *dir);

elf_api int elf_lib_list_folder(elState *R) {
	elf_ensure(elf_get_num_args(R) > 0);
	elString *dir = elf_get_string(R,0);
	elInteger level = 0;
	if (elf_get_num_args(R) > 1) {
		level = elf_get_integer(R,1);
	}
	elTable *list = elf_add_new_table(R);
	elf_lib_list_folder_(R,list,level,dir);
	elf_add_table(R,list);
	return 1;
}


/* Use -1 for recursive always, 0 for just this layer */
void elf_lib_list_folder_(elState *R, elTable *list, int level, elString *dir) {
#if defined(PLATFORM_DESKTOP)
	WIN32_FIND_DATAA f;
	HANDLE h = FindFirstFileA(elf_tpf("%s\\*",dir->c),&f);
	if (h != INVALID_HANDLE_VALUE) do {
		if (elf_is_virtual_file_name(f.cFileName)) continue;
		int is_directory = 0 != (f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
		elValue *top = elf_get_stack_top(R);
		elString *name = elf_add_new_string(R,f.cFileName);
		elString *path = elf_add_new_string(R,elf_tpf("%s\\%s",dir->c,f.cFileName));
		elTable *file = elf_add_new_table(R);
		elf_table_set_string_field(file,elf_add_new_string(R,"name"),name);
		elf_table_set_string_field(file,elf_add_new_string(R,"path"),path);
		elf_table_set_integer_field(file,elf_add_new_string(R,"is_directory"),is_directory);
		elf_table_set_integer_field(file,elf_add_new_string(R,"size"),f.nFileSizeLow);
		elf_table_add(list,elf_table_value(file));
		if (level != 0) {
			if (is_directory) {
				elf_lib_list_folder_(R,list,level-1,path);
			}
		}
		elf_set_stack_top(R,top);
	} while (FindNextFileA(h,&f));
#else
	elf_throw(R,NO_BYTE,"unsupported platform");
#endif
}


void elf_lib_enumerate_directory_(elState *R, elString *dir, elClosure *cls) {
#if defined(PLATFORM_DESKTOP)
	WIN32_FIND_DATAA f;
	HANDLE h = FindFirstFileA(elf_tpf("%s\\*",dir->c),&f);
	if (h != INVALID_HANDLE_VALUE) do {
		if (elf_is_virtual_file_name(f.cFileName)) continue;
		int is_directory = 0 != (f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
		elValue *top = elf_get_stack_top(R);
		elString *name = elf_add_new_string(R,f.cFileName);
		elString *path = elf_add_new_string(R,elf_tpf("%s\\%s",dir->c,f.cFileName));
		elRegId base = elf_add_closure(R,cls);
		elTable *file = elf_add_new_table(R);
		elf_table_set_string_field(file,elf_add_new_string(R,"name"),name);
		elf_table_set_string_field(file,elf_add_new_string(R,"path"),path);
		elf_table_set_integer_field(file,elf_add_new_string(R,"is_directory"),is_directory);
		elf_table_set_integer_field(file,elf_add_new_string(R,"size"),f.nFileSizeLow);
		int results = elf_call_function(R,base,1,1);
		if (is_directory) {
			if ((results > 0) && elf_get_integer(R,base) != 0) {
				elf_lib_enumerate_directory_(R,path,cls);
			}
		}
		elf_set_stack_top(R,top);
	} while (FindNextFileA(h,&f));
#elif defined(PLATFORM_WEB)
	DIR *dirfd = opendir(dir->c);
	if (dirfd != elNil) {
		struct dirent *entry;
		while ((entry = readdir(dirfd)) != elNil) {
			if (elf_is_virtual_file_name(entry->d_name)) {
				continue;
			}
			elBool isdir = (entry->d_type & DT_DIR) != false;
			elValue *top = elf_get_stack_top(R);

			elString *name = elf_add_new_string(R,entry->d_name);
			elString *path = elf_add_new_string(R,elf_tpf("%s/%s",dir->c,entry->d_name));
			elRegId base = elf_add_closure(R,cls);
			elTable *file = elf_add_new_table(R);

			elf_table_set_string_field(file,elf_add_new_string(R,"name"),name);
			elf_table_set_string_field(file,elf_add_new_string(R,"path"),path);
			elf_table_set_integer_field(file,elf_add_new_string(R,"isdir"),isdir);
			int r = elf_call_function(R,base,1,1);
			if ((r > 0) && isdir && elf_get_integer(R,base)) {
				elf_lib_enumerate_directory_(R,path,cls);
			}
			elf_set_stack_top(R,top);
		}
		closedir(dirfd);
	}
#endif
}


elf_api int elf_lib_enumerate_directory(elState *R) {
	elf_ensure(R->frame->x == 2);
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
		if ((v.tag == TAG_CLS) || (v.tag == TAG_BID)) {
			continue;
		}
		if (nitems ++ != 0) fprintf(io,",");
		elf_fpf_value(io,slot.k,elTrue);
		fprintf(io," = ");
		if (v.tag == TAG_TAB) {
			elf_unload(io,v.t,level+1);
		} else {
			elf_fpf_value(io,v,elTrue);
		}
	}
	fprintf(io,"}");
}


int elflib_unload(elState *S) {
	elHandle io = elf_get_handle(S,0);
	elTable *tab = elf_get_table(S,1);
	elf_unload(io,tab,0);
	return 0;
}


elf_api void elflib_loadall(elState *R) {
	elf_register_integer(R,"elf.VERSION",0);
#if defined(PLATFORM_WEB)
	elf_register_string(R,"elf.PLATFORM","WEB");
	elf_register_string(R,"elf.OS","UNKNOWN");
#else
	elf_register_string(R,"elf.PLATFORM","DESKTOP");
	#if defined(_WIN32)
	elf_register_string(R,"elf.OS","WINDOWS");
	#else
	elf_register_string(R,"elf.OS","UNKNOWN");
	#endif
#endif

	elf_register_binding(R,"elf.debugger",elflib_debugger);
	elf_register_binding(R,"elf.oncalldebugger",elflib_oncalldebugger);
	elf_register_binding(R,"elf.bytelogging",elflib_bytelogging);
	elf_register_binding(R,"elf.globalbytelogging",elflib_globalbytelogging);

	elf_register_binding(R,"elf.merge_tables",elf_lib_merge_tables);

	elf_register_binding(R,"elf.pause_collector",elf_lib_pause_collector);
	elf_register_binding(R,"elf.mark_object",elf_lib_mark_object);
	elf_register_binding(R,"elf.mark_everything",elf_lib_mark_everything);
	elf_register_binding(R,"elf.unmark_objects",elf_lib_unmark_objects);
	elf_register_binding(R,"elf.get_collector_threshold",elf_lib_get_collector_threshold);
	elf_register_binding(R,"elf.get_allocated_objects",elf_lib_get_allocated_objects);
	elf_register_binding(R,"elf.get_allocated_memory",elf_lib_get_allocated_memory);
	elf_register_binding(R,"elf.collect",elf_lib_collect);

	elf_register_binding(R,"elf.get_value_tag",elf_lib_get_value_tag);

	elf_register_binding(R,"elf.set_object_metatable",elf_lib_set_metatable);
	elf_register_binding(R,"elf.get_object_metatable",elf_lib_get_metatable);
	elf_register_binding(R,"elf.get_object_address",elf_lib_get_object_address);
	elf_register_binding(R,"elf.get_object_color",elf_lib_get_object_color);
	elf_register_binding(R,"elf.set_object_trap",elf_lib_set_object_trap);


	elf_register_binding(R,"elf.log",elflib_log);
	elf_register_binding(R,"elf.err",elflib_err);

	/* todo: these should be intrinsic */
	elf_register_binding(R,"ntoi",elflib_ntoi);
	elf_register_binding(R,"iton",elflib_iton);

	elf_register_binding(R,"elf.clocktime",elflib_clocktime);
	elf_register_binding(R,"elf.timediffs",elf_lib_timediffs);
	elf_register_binding(R,"elf.timediffms",elf_lib_timediffms);

	elf_register_binding(R,"elf.loadlib",elflib_loadlib);
	elf_register_binding(R,"elf.libfn",elflib_libfn);

	elf_register_binding(R,"elf.include",elflib_include);
	elf_register_binding(R,"elf.loadcode",elflib_loadcode);
	elf_register_binding(R,"elf.loadexpr",elflib_loadexpr);
	elf_register_binding(R,"elf.loadfile",elflib_loadfile);
	elf_register_binding(R,"elf.unload",elflib_unload);

	elf_register_binding(R,"elf.pf_indent",elf_lib_pf_indent);
	elf_register_binding(R,"elf.pf",elf_lib_pf);
	elf_register_binding(R,"elf.lpf",elf_lib_lpf);


	elf_register_binding(R,"elf.change_work_dir",elflib_change_work_dir);
	elf_register_binding(R,"elf.get_work_dir",elflib_get_work_dir);
	/* todo: deprecate name */
	elf_register_binding(R,"elf.enumerate_directory",elf_lib_enumerate_directory);
	elf_register_binding(R,"elf.enumerate_folder",elf_lib_enumerate_directory);
	elf_register_binding(R,"elf.list_folder",elf_lib_list_folder);

	elf_register_handle(R,"elf.ferr",stderr);
	elf_register_handle(R,"elf.fout",stdout);
	elf_register_handle(R,"elf.fin",stdin);

	elf_register_binding(R,"elf.pf",elf_lib_pf);
	elf_register_binding(R,"elf.fpf",elflib_fpf);
	elf_register_binding(R,"elf.fload",elflib_fload);
	elf_register_binding(R,"elf.ftemp",elflib_ftemp);
	elf_register_binding(R,"elf.fopen",elf_lib_fopen);
	elf_register_binding(R,"elf.fclose",elf_lib_fclose);
	elf_register_binding(R,"elf.fsize",elf_lib_fsize);

	elf_register_binding(R,"floor",elflib_floor);
	elf_register_binding(R,"sqrt",elf_lib_sqrt);
	elf_register_binding(R,"pow",elf_lib_pow);
	elf_register_binding(R,"sin",elf_lib_sin);
	elf_register_binding(R,"cos",elf_lib_cos);
	elf_register_binding(R,"acos",elf_lib_acos);
	elf_register_binding(R,"tan",elf_lib_tan);
	elf_register_binding(R,"atan2",elf_lib_atan2);

	elf_register_binding(R,"elf.sleep",elflib_sleep);
	elf_register_binding(R,"elf.exec",elflib_exec);
}
