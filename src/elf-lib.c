/*
** See Copyright Notice In elf.h
** elf-lib.c
** elf lib
*/


/* math */
int elflib_floor(elf_ThreadState *R) {
	elf_locnum(R,floor(elf_getnum(R,0)));
	return 1;
}


int elflib_sqrt(elf_ThreadState *R) {
	elf_locnum(R,sqrt(elf_getnum(R,0)));
	return 1;
}


int elflib_sin(elf_ThreadState *R) {
	elf_locnum(R,sin(elf_getnum(R,0)));
	return 1;
}


int elflib_cos(elf_ThreadState *R) {
	elf_locnum(R,cos(elf_getnum(R,0)));
	return 1;
}


int elflib_acos(elf_ThreadState *R) {
	elf_locnum(R,acos(elf_getnum(R,0)));
	return 1;
}


int elflib_tan(elf_ThreadState *R) {
	elf_locnum(R,tan(elf_getnum(R,0)));
	return 1;
}


int elflib_atan2(elf_ThreadState *R) {
	elf_locnum(R,atan2(elf_getnum(R,0),elf_getnum(R,1)));
	return 1;
}


/*  */
int elflib_abort(elf_ThreadState *R) {
	if(1) abort();
	return 0;
}


int elflib_exit(elf_ThreadState *R) {
	if(1) exit(elf_getint(R,0));
	return 0;
}


/* debugging */
int elflib_bytelogging(elf_ThreadState *R) {
	elf_locint(R,R->call->caller->logging);
	R->call->caller->logging = elf_getint(R,0);
	return 0;
}


int elflib_globalbytelogging(elf_ThreadState *R) {
	elf_locint(R,R->bytelogging);
	R->bytelogging = elf_getint(R,0);
	return 0;
}


int elflib_debugger(elf_ThreadState *R) {
#if defined(_DEBUG)
	R->debuggerflag = ltrue;
#else
	char *message = "no message";
	if (R->call->nx != 0) {
		message = elf_getstr(R,0)->c;
	}
	elf_debugger(message);
#endif
	return 0;
}


/* logging */
int elflib_log(elf_ThreadState *R) {
	for (int i = 0; i < R->call->nx; i ++) {
		elf_valfpf(stdout,elf_getval(R,i),lfalse);
	} printf("\n");
	return 0;
}


int elflib_err(elf_ThreadState *R) {
	for (int i = 0; i < R->call->nx; i ++) {
		elf_valfpf(stdout,elf_getval(R,i),lfalse);
	} printf("\n");
	return 0;
}


/* code */
int elflib_loadexpr(elf_ThreadState *R) {
	elf_String *filename = lnil,*contents = lnil;
	if (R->call->nx == 2) {
		filename = elf_getstr(R,0);
		contents = elf_getstr(R,1);
	} else if (R->call->nx == 1) {
		filename = elf_newlocstr(R,"unnamed");
		contents = elf_getstr(R,1);
	} else LNOBRANCH;
	elf_loadexpr(R,filename,R->call->ry,R->call->ny,contents->c);
	/* no need to do hoisting */
	return 0;
}


int elflib_loadcode(elf_ThreadState *R) {
	elf_ensure(R->call->nx == 2);
	elf_String *filename = elf_getstr(R,0);
	elf_String *contents = elf_getstr(R,1);
	elf_loadcode(R,filename,R->call->ry,R->call->ny,contents->c);
	/* no need to do hoisting */
	return 0;
}


int elflib_loadfile(elf_ThreadState *R) {
	elf_String *filename = elf_getstr(R,0);
	elf_loadfile(R,filename,R->call->ry,R->call->ny);
	/* no need to do hoisting */
	return 0;
}



int elflib_GCM(elf_ThreadState *R) {
	elf_locint(R,R->gcmemory);
	return 1;
}


int elflib_GCT(elf_ThreadState *R) {
	elf_locint(R,R->gcthreshold);
	return 1;
}


int elflib_GCN(elf_ThreadState *R) {
	elf_locint(R,elf_varlen(R->gc));
	return 1;
}


int elflib_iton(elf_ThreadState *R) {
	elf_Value v = elf_getval(R,0);
	if (v.tag == TAG_INT) {
		elf_locnum(R,(elf_num)v.i);
	} else elf_locnum(R,v.n);
	return 1;
}


int elflib_ntoi(elf_ThreadState *R) {
	elf_Value v = elf_getval(R,0);
	if (v.tag == TAG_NUM) {
		elf_locint(R,(elf_int)v.n);
	} else elf_locint(R,v.i);
	return 1;
}


// typedef void (*em_dlopen_callback)(void* handle, void* user_data);
// void emscripten_dlopen(const char *filename, int flags, void* user_data, em_dlopen_callback onsuccess, em_arg_callback_func onerror);
int elflib_libfn(elf_ThreadState *rt) {
	elf_Handle lib = elf_getsys(rt,0);
	elf_String *name = elf_getstr(rt,1);
	lBinding fn = (lBinding) sys_libfn(lib,name->c);
	if (fn != lnil) {
		elf_locbinding(rt,fn);
	} else {
		elf_locnil(rt);
	}
	return 1;
}


int elflib_loadlib(elf_ThreadState *R) {
	elf_String *name = elf_getstr(R,0);
	elf_Closure *callback = elf_getcls(R,1);
	elf_Handle lib = sys_loadlib(name->c);
	if (lib != lnil) elf_locsys(R,lib);
	else elf_locnil(R);
	return 1;
}


int elflib_exec(elf_ThreadState *R) {
#if defined(_WIN32)
	elf_String *cmd = elf_getstr(R,0);
	STARTUPINFO si = {sizeof(si)};
	PROCESS_INFORMATION pi = {0};
	int result = CreateProcess(NULL,cmd->c,NULL,NULL,FALSE,0,NULL,NULL,&si,&pi);
	elf_locint(R,result);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
#else
	elf_locint(R,0);
#endif
	return 1;
}


int elflib_fopen(elf_ThreadState *R) {
	elf_ensure(R->call->nx == 2);
	elf_String *name = elf_getstr(R,0);
	elf_String *flags = elf_getstr(R,1);
	FILE *file = fopen(name->c,flags->c);
	elf_locsys(R,(elf_Handle)file);
	return 1;
}


int elflib_fclose(elf_ThreadState *R) {
	elf_ensure(R->call->nx == 1);
	elf_Handle file = elf_getsys(R,0);
	fclose(file);
	return 0;
}


int elflib_fsize(elf_ThreadState *R) {
	elf_Handle file = (FILE*) elf_getsys(R,0);
	fseek(file,0,SEEK_END);
	elf_locint(R,ftell(file));
	return 1;
}


int elflib_fload(elf_ThreadState *R) {
	FILE *file = (FILE*) elf_getsys(R,0);
	if (file != 0) {
		fseek(file,0,SEEK_END);
		long size = ftell(file);
		fseek(file,0,SEEK_SET);
		elf_String *buf = elf_newstrlen(R,size);
		fread(buf->c,1,size,file);
		elf_locstr(R,buf);
	} else elf_locnil(R);
	return 1;
}


int elflib_ftemp(elf_ThreadState *R) {
	FILE *file = {0};
#if defined(PLATFORM_WEB)
	file = tmpfile();
#else
	tmpfile_s(&file);
#endif
	elf_locsys(R,(elf_Handle)file);
	return 1;
}


int elflib_mydir(elf_ThreadState *R) {
	char buf[MAX_PATH];
	sys_pwd(sizeof(buf),buf);
	elf_newlocstr(R,buf);
	if (R->call->nx == 1) {
		sys_setpwd(elf_getstr(R,0)->c);
	}
	return 1;
}


int elflib_fpf(elf_ThreadState *rt) {
	elf_Handle file = elf_getsys(rt,0);
	int wrote = 0;
	for (int i = 1; i < rt->f->x; i ++) {
		wrote += elf_valfpf(file,elf_getval(rt,i),lfalse);
	}
	elf_locint(rt,wrote);
	return 1;
}


int elflib_lpf(elf_ThreadState *rt) {
	for (int i = 0; i < rt->f->x; i ++) {
		if (i != 0) fprintf(stdout,"\n");
		elf_valfpf(stdout,elf_getval(rt,i),lfalse);
	}
	fprintf(stdout,"\n");
	return 0;
}


int elflib_pf(elf_ThreadState *rt) {
	for (int i = 0; i < rt->f->x; i ++) {
		elf_valfpf(stdout,elf_getval(rt,i),lfalse);
	}
	fprintf(stdout,"\n");
	return 0;
}


elf_api int elflib_sleep(elf_ThreadState *rt) {
	elf_ensure(rt->f->x == 1);
	sys_sleep(elf_getint(rt,0));
	return 0;
}


elf_api int elflib_clocktime(elf_ThreadState *rt) {
	elf_locint(rt,sys_clocktime());
	return 1;
}


elf_api int elflib_timediffs(elf_ThreadState *rt) {
	elf_ensure(rt->f->x == 1);
	elf_int i = elf_getint(rt,0);
	elf_locnum(rt,(sys_clocktime() - i) / (elf_num) sys_clockhz());
	return 1;
}


elf_bool isvirtual(char const *fn) {
	while (*fn == '.') ++ fn;
	return *fn == 0;
}


elf_globaldecl elf_String *enumdir_keyname;
elf_globaldecl elf_String *enumdir_keypath;
elf_globaldecl elf_String *enumdir_isdir;


void elflib_enumdir_(elf_ThreadState *R, elf_String *dir, elf_Closure *cls) {
#if defined(PLATFORM_DESKTOP)
	WIN32_FIND_DATAA f;
	HANDLE h = FindFirstFileA(elf_tpf("%s\\*",dir->c),&f);
	if (h != INVALID_HANDLE_VALUE) do {
		if (isvirtual(f.cFileName)) continue;
		int isdir = 0 != (f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
		elf_Value *top = elf_gettop(R);
		elf_String *name = elf_newlocstr(R,f.cFileName);
		elf_String *path = elf_newlocstr(R,elf_tpf("%s\\%s",dir->c,f.cFileName));
		llocalid base = elf_loccls(R,cls);
		elf_Table *file = elf_newloctab(R);
		elf_tabsetstrfld(file,enumdir_keyname,name);
		elf_tabsetstrfld(file,enumdir_keypath,path);
		elf_tabsetintfld(file,enumdir_isdir,isdir);
		int r = elf_callfn(R,base,1,1);
		if ((r > 0) && isdir && elf_getint(R,base)) {
			elflib_enumdir_(R,path,cls);
		}
		elf_settop(R,top);
	} while (FindNextFileA(h,&f));
#elif defined(PLATFORM_WEB)
	DIR *dir = opendir(d->c);
	if (dir != lnil) {
		struct dirent *entry;
		while ((entry = readdir(dir)) != lnil) {
			if (isvirtual(entry->d_name)) {
			 	continue;
			}
			elf_bool isdir = (entry->d_type & DT_DIR) != lfalse;
			elf_Value *top = elf_gettop(R);
			elf_String *name = elf_newlocstr(R,entry->d_name);
			elf_String *path = elf_newlocstr(R,elf_tpf("%s/%s",d->c,entry->d_name));
			llocalid base = elf_loccls(R,cl);
			elf_Table *file = elf_newloctab(R);
			elf_tabsetstrfld(file,enumdir_keyname,name);
			elf_tabsetstrfld(file,enumdir_keypath,path);
			elf_tabsetintfld(file,enumdir_isdir,isdir);
			int r = elf_callfn(R,base,1,1);
			if ((r > 0) && isdir && elf_getint(R,base)) {
				elflib_enumdir_(R,path,cls);
			}
			elf_settop(R,top);
		}
		closedir(dir);
	}
#endif
}


elf_api int elflib_enumdir(elf_ThreadState *R) {
	elf_ensure(R->frame->x == 2);
	/* push these keys temporarily so they won't
	be gc'd and also to to avoid creating them so often  */
	enumdir_keyname = elf_newlocstr(R,"name");
	enumdir_keypath = elf_newlocstr(R,"path");
	enumdir_isdir = elf_newlocstr(R,"isdir");
	elf_String *dir = elf_getstr(R,0);
	elf_Closure *cls = elf_getcls(R,1);
	elflib_enumdir_(R,dir,cls);
	return 1;
}


elf_api void elflib_load(elf_ThreadState *R) {
	elf_registerint(R,"elf.VERSION",0);
#if defined(PLATFORM_WEB)
	elf_registerstr(R,"elf.PLATFORM","WEB");
	elf_registerstr(R,"elf.OS","UNKNOWN");
#else
	elf_registerstr(R,"elf.PLATFORM","DESKTOP");
	#if defined(_WIN32)
		elf_registerstr(R,"elf.OS","WINDOWS");
	#else
		elf_registerstr(R,"elf.OS","UNKNOWN");
	#endif
#endif

	elf_register(R,"elf.bytelogging",elflib_bytelogging);
	elf_register(R,"elf.globalbytelogging",elflib_globalbytelogging);

	elf_register(R,"elf.log",elflib_log);
	elf_register(R,"elf.err",elflib_err);

	/* todo: these should be intrinsic */
	elf_register(R,"ntoi",elflib_ntoi);
	elf_register(R,"iton",elflib_iton);

	elf_register(R,"elf.clocktime",elflib_clocktime);
	elf_register(R,"elf.timediffs",elflib_timediffs);

	elf_register(R,"elf.loadlib",elflib_loadlib);
	elf_register(R,"elf.libfn",elflib_libfn);

	elf_register(R,"elf.loadcode",elflib_loadcode);
	elf_register(R,"elf.loadexpr",elflib_loadexpr);
	elf_register(R,"elf.loadfile",elflib_loadfile);

	elf_register(R,"elf.GCN",elflib_GCN);
	elf_register(R,"elf.GCT",elflib_GCT);
	elf_register(R,"elf.GCM",elflib_GCM);


	elf_register(R,"elf.pf",elflib_pf);
	elf_register(R,"elf.lpf",elflib_lpf);


	elf_registersys(R,"elf.ferr",stderr);
	elf_registersys(R,"elf.fout",stdout);
	elf_registersys(R,"elf.fin",stdin);
	elf_register(R,"elf.mydir",elflib_mydir);
	elf_register(R,"elf.enumdir",elflib_enumdir);
	elf_register(R,"elf.pf",elflib_pf);
	elf_register(R,"elf.fpf",elflib_fpf);
	elf_register(R,"elf.fload",elflib_fload);
	elf_register(R,"elf.ftemp",elflib_ftemp);
	elf_register(R,"elf.fopen",elflib_fopen);
	elf_register(R,"elf.fclose",elflib_fclose);
	elf_register(R,"elf.fsize",elflib_fsize);

	elf_register(R,"floor",elflib_floor);
	elf_register(R,"sqrt",elflib_sqrt);
	elf_register(R,"sin",elflib_sin);
	elf_register(R,"cos",elflib_cos);
	elf_register(R,"acos",elflib_acos);
	elf_register(R,"tan",elflib_tan);
	elf_register(R,"atan2",elflib_atan2);

	elf_register(R,"elf.sleep",elflib_sleep);
	elf_register(R,"elf.exec",elflib_exec);
}
