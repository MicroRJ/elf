//
// See Copyright Notice In elf.h
//


// todo: remove!
// todo: TOMORROW!
static int _write_file_io(elf_State *S, FILE *io, char *text) {
	ASSERT(io != 0);
	return fwrite(text,1,strlen(text),io);
}

// todo: remove!
// todo: TOMORROW!
static int _write_file(elf_State *S, char *name, char *text) {
	FILE *file = fopen(name,"wb");
	int wrote = 0;
	if (file) {
		wrote = _write_file_io(S,file,text);
		fclose(file);
		elf_debug_log("'%s': file wrote successfully",name);
	} else {
		elf_error_log("'%s': could not write",name);
	}
	return wrote;
}








//
// name: the name of the dynamic library in
// the file system
//
ELF_FUNCTION(l_sys_load_dll) {
	char *name = elf_getargtext(S, 0);

	elf_Handle lib = sys_load_dll(name);
	if (lib != 0) elf_push_handle(S, lib);
	else          elf_push_nil(S);
	return 1;
}


ELF_FUNCTION(l_sys_get_dll_fn) {
	elf_Handle lib = elf_get_sysarg(S, 0);
	char *name = elf_getargtext(S, 1);

	elf_Function fn = (elf_Function) sys_get_dll_fn(lib, name);
	if (fn != 0) elf_push_function(S,fn);
	else         elf_push_nil(S);
	return 1;
}


static int foldertree(elf_State *inter, char *path, int recurse) {
	int resstk = elf_push_table(inter);

	// todo: speed! we can pre-push all these strings and reference
	// them by stack address instead!
	FILE_VISITOR visitor;
	if (sys_opendir(&visitor, path)) do {
		if (visitor.type == FILE_TYPE_SYMLINK) continue;
		if (is_file_name_empty(visitor.name)) continue;

		// todo: can we have our cake and eat it too please?
		char *childpath = malloc(1024);
		stbsp_snprintf(childpath, 1024, "%s\\%s", path, visitor.name);

		elf_push_table(inter);
		{
			elf_push_string(inter, "name");
			elf_push_string(inter, visitor.name);
			elf_table_set(inter);

			// todo: a bit wasteful don't you think!
			elf_push_string(inter, "path");
			elf_push_string(inter, childpath);
			elf_table_set(inter);

			char * const type2s[] = {
				[FILE_TYPE_FILE] = "folder",
				[FILE_TYPE_FOLDER] = "folder",
				[FILE_TYPE_SYMLINK] = "symlink",
			};
			elf_push_string(inter, "type");
			elf_push_string(inter, type2s[visitor.type]);
			elf_table_set(inter);

			elf_push_string(inter, "size");
			elf_push_int(inter, visitor.size);
			elf_table_set(inter);

			if (recurse > 0 && visitor.type == FILE_TYPE_FOLDER) {
				elf_push_string(inter, "children");
				foldertree(inter, childpath, recurse - 1);
				elf_table_set(inter);
			}
			free(childpath);
		}
		elf_array_add(inter);

	} while (sys_readdir(&visitor));

	return resstk;
}

// NOTE: requires a table on the stack!
// TODO: use utility function to ensure this!
static void pathlist(elf_State *inter, char *path, int recurse) {
	// todo: speed! we can pre-push all these strings and reference
	// them by stack address instead!
	FILE_VISITOR visitor;
	if (sys_opendir(&visitor, path)) do {
		if (visitor.type == FILE_TYPE_SYMLINK) continue;
		if (is_file_name_empty(visitor.name)) continue;

		// todo: can we have our cake and eat it too please?
		char *childpath = malloc(1024);
		stbsp_snprintf(childpath, 1024, "%s\\%s", path, visitor.name);

		if (visitor.type == FILE_TYPE_FILE) {
			elf_push_string(inter, childpath);
			elf_array_add(inter);
		} else if (visitor.type == FILE_TYPE_FOLDER) {
			if (recurse > 0) {
				pathlist(inter, childpath, recurse - 1);
			}
		}
		free(childpath);
	} while (sys_readdir(&visitor));
}

ELF_FUNCTION(l_sys_get_file_tree) {
	char *path = elf_getargtext(S,0);
	int recursion = 0;
	if (elf_get_num_args(S) >= 2) {
		recursion = elf_get_intarg(S,1);
	}
	foldertree(S, path, recursion);
	return 1;
}

ELF_FUNCTION(l_sys_get_path_list) {
	char *path = elf_getargtext(S,0);
	int recursion = 0;
	if (elf_get_num_args(S) >= 2) {
		recursion = elf_get_intarg(S,1);
	}

	elf_push_table(S);
	pathlist(S, path, recursion);
	return 1;
}

ELF_FUNCTION(l_sys_open_temp_file) {
	FILE *file = {0};
#if defined(PLATFORM_WEB)
	file = tmpfile();
#else
	tmpfile_s(&file);
#endif
	elf_push_handle(S,(elf_Handle)file);
	return 1;
}

ELF_FUNCTION(l_sys_open_file) {
	ASSERT(elf_get_num_args(S) == 2);
	char *name = elf_getargtext(S,0);
	char *flags = elf_getargtext(S,1);
	FILE *file = fopen(name,flags);
	elf_push_handle(S,(elf_Handle)file);
	return 1;
}

ELF_FUNCTION(l_sys_close_file) {
	ASSERT(elf_get_num_args(S)==1);
	FILE *file=(FILE *)elf_get_sysarg(S,0);
	if(file){
		fclose(file);
	}
	return 0;
}

ELF_FUNCTION(l_sys_get_file_size) {
	FILE *file;
	if(elf_get_argtag(S,0) == elf_tag_String) {
		file = fopen(elf_getargtext(S,0),"rb");
	} else {
		file = (FILE *) elf_get_sysarg(S,0);
	}
	fseek(file,0,SEEK_END);
	int size = ftell(file);
	fseek(file,0,SEEK_SET);
	elf_push_int(S,size);
	return 1;
}

ELF_FUNCTION(l_sys_read_file) {
	// for this sort of stuff, we could just reposition
	// the stack pointer... ? #todo
	* S->stack_ptr ++ = elf_getarg(S, 0);
	elf_read_file(S, -1);
	return 1;
}

ELF_FUNCTION(l_sys_write_file) {
	if(elf_get_argtag(S,0) == elf_tag_String) {
		_write_file(S,elf_getargtext(S,0),elf_getargtext(S,1));
	} else {
		_write_file_io(S,elf_get_sysarg(S,0),elf_getargtext(S,1));
	}
	return 0;
}

ELF_FUNCTION(l_sys_write_file_to_file) {
	FILE *dst,*src;
	if(elf_get_argtag(S,0) == elf_tag_String) {
		dst = fopen(elf_getargtext(S,0),"wb");
	} else {
		dst = (FILE *) elf_get_sysarg(S,0);
	}
	if(elf_get_argtag(S,1) == elf_tag_String) {
		src = fopen(elf_getargtext(S,1),"rb");
	} else {
		src = (FILE *) elf_get_sysarg(S,1);
	}
	char buffer[4096];
	int read,wrote = 0;
	do {
		read = fread(buffer,1,sizeof(buffer),src);
		wrote += fwrite(buffer,1,read,dst);
	} while(read > 0);

	elf_push_int(S,wrote);
	return 1;
}



ELF_FUNCTION(l_sys_change_work_dir) {
	int ok = sys_set_work_dir(elf_getargtext(S,0));
	elf_push_int(S,ok);
	return 1;
}


ELF_FUNCTION(l_sys_get_work_dir) {
	char buf[256];
	sys_get_work_dir(sizeof(buf),buf);
	elf_new_string(S,buf);
	return 1;
}

static const elf_Binding l_sys[] = {
	{"load_dll", l_sys_load_dll},
	{"get_dll_fn", l_sys_get_dll_fn},

	{"get_file_tree", l_sys_get_file_tree},
	{"get_path_list", l_sys_get_path_list},

	{"open_temp_file", l_sys_open_temp_file},
	{"open_file", l_sys_open_file},
	{"close_file", l_sys_close_file},
	{"get_file_size", l_sys_get_file_size},
	{"read_file", l_sys_read_file},
	{"write_file", l_sys_write_file},
	{"write_file_to_file", l_sys_write_file_to_file},
	{"change_work_dir", l_sys_change_work_dir},
	{"get_work_dir", l_sys_get_work_dir},
};