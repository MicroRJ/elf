/*
** See Copyright Notice In elf.h
** elf-lib.c
** elf lib
*/


/* math */
int elflib_floor(elf_State *R) {
	elf_pushnum(R,floor(elf_getnum(R,0)));
	return 1;
}


int elflib_sqrt(elf_State *R) {
	elf_pushnum(R,sqrt(elf_getnum(R,0)));
	return 1;
}


int elflib_sin(elf_State *R) {
	elf_pushnum(R,sin(elf_getnum(R,0)));
	return 1;
}


int elflib_cos(elf_State *R) {
	elf_pushnum(R,cos(elf_getnum(R,0)));
	return 1;
}


int elflib_acos(elf_State *R) {
	elf_pushnum(R,acos(elf_getnum(R,0)));
	return 1;
}


int elflib_tan(elf_State *R) {
	elf_pushnum(R,tan(elf_getnum(R,0)));
	return 1;
}


int elflib_atan2(elf_State *R) {
	elf_pushnum(R,atan2(elf_getnum(R,0),elf_getnum(R,1)));
	return 1;
}


/*  */
int elflib_abort(elf_State *R) {
	if(1) abort();
	return 0;
}


int elflib_exit(elf_State *R) {
	if(1) exit(elf_getint(R,0));
	return 0;
}


/* debugging */
int elflib_bytelogging(elf_State *R) {
	elf_pushint(R,R->call->caller->logging);
	R->call->caller->logging = elf_getint(R,0);
	return 0;
}


int elflib_globalbytelogging(elf_State *R) {
	elf_pushint(R,R->bytelogging);
	R->bytelogging = elf_getint(R,0);
	return 0;
}


int elflib_debugger(elf_State *R) {
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
int elflib_log(elf_State *R) {
	for (int i = 0; i < R->call->nx; i ++) {
		elf_valfpf(stdout,elf_getany(R,i),lfalse);
	} printf("\n");
	return 0;
}


int elflib_err(elf_State *R) {
	for (int i = 0; i < R->call->nx; i ++) {
		elf_valfpf(stdout,elf_getany(R,i),lfalse);
	} printf("\n");
	return 0;
}


/* the include function merges or includes
** the given symbol directory into the
** global symbol directory.
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
** and you should use especial care when
** using this function.
** MORE: the include function finds
** all the symbols within the given
** directory and for each target symbol it
** creates a new one in the root directory
** that points to the \value of the
** target symbol.
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
** If the symbol is a single word there's
** no need to add the '.' because the
** loader would know, but there are some
** drawbacks and edge cases that come
** with this, for instance:
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


int elflib_include(elf_State *R) {
	elf_checkargs(R,".include",1,"(the directory to include to add to the global directory)");
	char *dir = elf_getcstr(R,0);
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

	elf_Table *globals = R->M->globals;
	for (int i = 0; i < globals->ntotal; ++ i) {
		elf_tabslot slot = globals->slots[i];
		if (slot.k.tag == TAG_STR) {
			char *sym = elf_insymdir(dir,slot.k.x_str->c);
			if (*sym != '.') continue;
			elf_String *ref = elf_newstr(R,sym);
			elf_tabset(globals,elf_valstr(ref),globals->array[slot.i]);
			// elf_logdebug("added %s <- %s",ref->c,slot.k.x_str->c);
		}
	}
	return 0;
}


int elflib_loadexpr(elf_State *R) {
	elf_String *filename = lnil,*contents = lnil;
	if (R->call->nx == 2) {
		filename = elf_getstr(R,0);
		contents = elf_getstr(R,1);
	} else if (R->call->nx == 1) {
		filename = elf_pushnewstr(R,"unnamed");
		contents = elf_getstr(R,0);
	} else elf_unreachable;
	elf_loadexpr(R,filename,R->call->ry,R->call->ny,contents->c);
	/* no need to do hoisting */
	return 0;
}


int elflib_loadcode(elf_State *R) {
	elf_String *filename = lnil,*contents = lnil;
	if (R->call->nx == 2) {
		filename = elf_getstr(R,0);
		contents = elf_getstr(R,1);
	} else if (R->call->nx == 1) {
		filename = elf_pushnewstr(R,"unnamed");
		contents = elf_getstr(R,0);
	} else elf_unreachable;
	elf_loadcode(R,filename,R->call->ry,R->call->ny,contents->c);
	/* no need to do hoisting */
	return 0;
}


int elflib_loadfile(elf_State *R) {
	elf_String *filename = elf_getstr(R,0);
	elf_loadfile(R,filename,R->call->ry,R->call->ny);
	/* no need to do hoisting */
	return 0;
}



int elflib_GCM(elf_State *R) {
	elf_pushint(R,R->gcmemory);
	return 1;
}


int elflib_GCT(elf_State *R) {
	elf_pushint(R,R->gcthreshold);
	return 1;
}


int elflib_GCN(elf_State *R) {
	elf_pushint(R,elf_varlen(R->gc));
	return 1;
}


int elflib_iton(elf_State *R) {
	elf_Value v = elf_getany(R,0);
	if (v.tag == TAG_INT) {
		elf_pushnum(R,(elf_num)v.i);
	} else elf_pushnum(R,v.n);
	return 1;
}


int elflib_ntoi(elf_State *R) {
	elf_Value v = elf_getany(R,0);
	if (v.tag == TAG_NUM) {
		elf_pushint(R,(elf_int)v.n);
	} else elf_pushint(R,v.i);
	return 1;
}


// typedef void (*em_dlopen_callback)(void* handle, void* user_data);
// void emscripten_dlopen(const char *filename, int flags, void* user_data, em_dlopen_callback onsuccess, em_arg_callback_func onerror);
int elflib_libfn(elf_State *rt) {
	elf_Handle lib = elf_getsys(rt,0);
	elf_String *name = elf_getstr(rt,1);
	lBinding fn = (lBinding) sys_libfn(lib,name->c);
	if (fn != lnil) {
		elf_pushbinding(rt,fn);
	} else {
		elf_pushnil(rt);
	}
	return 1;
}


int elflib_loadlib(elf_State *R) {
	elf_String *name = elf_getstr(R,0);
	elf_Closure *callback = elf_getcls(R,1);
	elf_Handle lib = sys_loadlib(name->c);
	if (lib != lnil) elf_pushsys(R,lib);
	else elf_pushnil(R);
	return 1;
}


int elflib_exec(elf_State *R) {
#if defined(_WIN32)
	elf_String *cmd = elf_getstr(R,0);
	STARTUPINFO si = {sizeof(si)};
	PROCESS_INFORMATION pi = {0};
	int result = CreateProcess(NULL,cmd->c,NULL,NULL,FALSE,0,NULL,NULL,&si,&pi);
	elf_pushint(R,result);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
#else
	elf_pushint(R,0);
#endif
	return 1;
}


int elflib_fopen(elf_State *R) {
	elf_ensure(R->call->nx == 2);
	elf_String *name = elf_getstr(R,0);
	elf_String *flags = elf_getstr(R,1);
	FILE *file = fopen(name->c,flags->c);
	elf_pushsys(R,(elf_Handle)file);
	return 1;
}


int elflib_fclose(elf_State *R) {
	elf_ensure(R->call->nx == 1);
	elf_Handle file = elf_getsys(R,0);
	fclose(file);
	return 0;
}


int elflib_fsize(elf_State *R) {
	elf_Handle file = (FILE*) elf_getsys(R,0);
	fseek(file,0,SEEK_END);
	elf_pushint(R,ftell(file));
	return 1;
}


int elflib_fload(elf_State *R) {
	FILE *file = (FILE*) elf_getsys(R,0);
	if (file != 0) {
		fseek(file,0,SEEK_END);
		long size = ftell(file);
		fseek(file,0,SEEK_SET);
		elf_String *buf = elf_newstrlen(R,size);
		fread(buf->c,1,size,file);
		elf_pushstr(R,buf);
	} else elf_pushnil(R);
	return 1;
}


int elflib_ftemp(elf_State *R) {
	FILE *file = {0};
#if defined(PLATFORM_WEB)
	file = tmpfile();
#else
	tmpfile_s(&file);
#endif
	elf_pushsys(R,(elf_Handle)file);
	return 1;
}


int elflib_mydir(elf_State *R) {
	char buf[MAX_PATH];
	sys_pwd(sizeof(buf),buf);
	elf_pushnewstr(R,buf);
	if (R->call->nx == 1) {
		sys_setpwd(elf_getcstr(R,0));
	}
	return 1;
}


int elflib_fpf(elf_State *rt) {
	elf_Handle file = elf_getsys(rt,0);
	int wrote = 0;
	for (int i = 1; i < rt->f->x; i ++) {
		wrote += elf_valfpf(file,elf_getany(rt,i),lfalse);
	}
	elf_pushint(rt,wrote);
	return 1;
}


int elflib_lpf(elf_State *rt) {
	for (int i = 0; i < rt->f->x; i ++) {
		if (i != 0) fprintf(stdout,"\n");
		elf_valfpf(stdout,elf_getany(rt,i),lfalse);
	}
	fprintf(stdout,"\n");
	return 0;
}


int elflib_pf(elf_State *rt) {
	for (int i = 0; i < rt->f->x; i ++) {
		elf_valfpf(stdout,elf_getany(rt,i),lfalse);
	}
	fprintf(stdout,"\n");
	return 0;
}


elf_api int elflib_sleep(elf_State *rt) {
	elf_ensure(rt->f->x == 1);
	sys_sleep(elf_getint(rt,0));
	return 0;
}


elf_api int elflib_clocktime(elf_State *rt) {
	elf_pushint(rt,sys_clocktime());
	return 1;
}


elf_api int elflib_timediffs(elf_State *rt) {
	elf_ensure(rt->f->x == 1);
	elf_int i = elf_getint(rt,0);
	elf_pushnum(rt,(sys_clocktime() - i) / (elf_num) sys_clockhz());
	return 1;
}


elf_bool isvirtual(char const *fn) {
	while (*fn == '.') ++ fn;
	return *fn == 0;
}


elf_globaldecl elf_String *enumdir_keyname;
elf_globaldecl elf_String *enumdir_keypath;
elf_globaldecl elf_String *enumdir_isdir;


void elflib_enumdir_(elf_State *R, elf_String *dir, elf_Closure *cls) {
#if defined(PLATFORM_DESKTOP)
	WIN32_FIND_DATAA f;
	HANDLE h = FindFirstFileA(elf_tpf("%s\\*",dir->c),&f);
	if (h != INVALID_HANDLE_VALUE) do {
		if (isvirtual(f.cFileName)) continue;
		int isdir = 0 != (f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
		elf_Value *top = elf_gettop(R);
		elf_String *name = elf_pushnewstr(R,f.cFileName);
		elf_String *path = elf_pushnewstr(R,elf_tpf("%s\\%s",dir->c,f.cFileName));
		elf_localid base = elf_pushcls(R,cls);
		elf_Table *file = elf_pushnewtab(R);
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
	DIR *dirfd = opendir(dir->c);
	if (dirfd != lnil) {
		struct dirent *entry;
		while ((entry = readdir(dirfd)) != lnil) {
			if (isvirtual(entry->d_name)) {
				continue;
			}
			elf_bool isdir = (entry->d_type & DT_DIR) != lfalse;
			elf_Value *top = elf_gettop(R);

			elf_String *name = elf_pushnewstr(R,entry->d_name);
			elf_String *path = elf_pushnewstr(R,elf_tpf("%s/%s",dir->c,entry->d_name));
			elf_localid base = elf_pushcls(R,cls);
			elf_Table *file = elf_pushnewtab(R);

			elf_tabsetstrfld(file,enumdir_keyname,name);
			elf_tabsetstrfld(file,enumdir_keypath,path);
			elf_tabsetintfld(file,enumdir_isdir,isdir);
			int r = elf_callfn(R,base,1,1);
			if ((r > 0) && isdir && elf_getint(R,base)) {
				elflib_enumdir_(R,path,cls);
			}
			elf_settop(R,top);
		}
		closedir(dirfd);
	}
#endif
}


elf_api int elflib_enumdir(elf_State *R) {
	elf_ensure(R->frame->x == 2);
	/* push these keys temporarily so they won't
	be gc'd and also to to avoid creating them so often  */
	enumdir_keyname = elf_pushnewstr(R,"name");
	enumdir_keypath = elf_pushnewstr(R,"path");
	enumdir_isdir = elf_pushnewstr(R,"isdir");
	elf_String *dir = elf_getstr(R,0);
	elf_Closure *cls = elf_getcls(R,1);
	elflib_enumdir_(R,dir,cls);
	return 1;
}


elf_api void elflib_load(elf_State *R) {
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

	elf_register(R,"elf.debugger",elflib_debugger);
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

	elf_register(R,"elf.include",elflib_include);
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
