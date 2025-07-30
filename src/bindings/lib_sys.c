//
// See Copyright Notice In elf.h
//


// todo: remove!
static elf_String *elf_read_file_string(elf_State *S, char *name, FILE *io, int size, int pos) {
	elf_String *contents=0;

	int autoclose=false;
	if (!io){
		ASSERT(name);
		io=fopen(name,"rb");
		autoclose=true;
		if(!io) {
			elf_error_log("'%s': could not read",name);
			goto esc;
		}
	}

	ASSERT(io != 0);
	if (pos != -1) {
		fseek(io,pos,SEEK_SET);
	}
	if (size == -1) {
		fseek(io,0,SEEK_END);
		size = ftell(io);
		fseek(io,0,SEEK_SET);
	}

	contents = elf_new_string2(S,size);
	// fseek(io,0,SEEK_SET);
	fread(contents->text,1,size,io);

	if(autoclose){
		fclose(io);
		elf_debug_log("'%s': file read successfully",name);
	}
	esc:
	return contents;
}

// todo: remove!
static int _write_file_io(elf_State *S, FILE *io, char *text) {
	ASSERT(io != 0);
	return fwrite(text,1,strlen(text),io);
}

// todo: remove!
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
	char *name = elf_get_text(S, 0);

	elf_Handle lib = sys_load_dll(name);
	if (lib != 0) elf_push_handle(S, lib);
	else          elf_push_nil(S);
	return 1;
}


ELF_FUNCTION(l_sys_get_dll_fn) {
	elf_Handle lib = elf_get_sysobj(S, 0);
	char *name = elf_get_text(S, 1);

	elf_Function fn = (elf_Function) sys_get_dll_fn(lib, name);
	if (fn != 0) elf_push_function(S,fn);
	else         elf_push_nil(S);
	return 1;
}

ELF_FUNCTION(l_sys_list_folder) {
	char *path = elf_get_text(S,0);

	elf_push_table(S);

	// todo: speed! we can pre-push all these strings and reference
	// them by stack address instead!
	FILE_VISITOR visitor;
	if (sys_opendir(&visitor, path)) do {
		if (visitor.type == FILE_TYPE_SYMLINK) continue;

		elf_push_table(S);
		{
			elf_push_string(S, "name");
			elf_push_string(S, visitor.name);
			elf_table_set(S);

			elf_push_string(S, "path");
			elf_push_string(S, elf_tpf("%s\\%s",path,visitor.name));
			elf_table_set(S);

			char * const type2s[] = {
				[FILE_TYPE_FILE] = "folder",
				[FILE_TYPE_FOLDER] = "folder",
				[FILE_TYPE_SYMLINK] = "symlink",
			};
			elf_push_string(S, "type");
			elf_push_string(S, type2s[visitor.type]);
			elf_table_set(S);

			elf_push_string(S, "size");
			elf_push_int(S, visitor.size);
			elf_table_set(S);
		}
		elf_array_add(S);

	} while (sys_readdir(&visitor));

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
	char *name = elf_get_text(S,0);
	char *flags = elf_get_text(S,1);
	FILE *file = fopen(name,flags);
	elf_push_handle(S,(elf_Handle)file);
	return 1;
}

ELF_FUNCTION(l_sys_close_file) {
	ASSERT(elf_get_num_args(S)==1);
	FILE *file=(FILE *)elf_get_sysobj(S,0);
	if(file){
		fclose(file);
	}
	return 0;
}

ELF_FUNCTION(l_sys_get_file_size) {
	FILE *file;
	if(elf_get_tag(S,0) == elf_tag_String) {
		file = fopen(elf_get_text(S,0),"rb");
	} else {
		file = (FILE *) elf_get_sysobj(S,0);
	}
	fseek(file,0,SEEK_END);
	int size = ftell(file);
	fseek(file,0,SEEK_SET);
	elf_push_int(S,size);
	return 1;
}

ELF_FUNCTION(l_sys_read_file) {
	elf_String *contents=0;
	int pos=-1,size=-1;
	char*name="noname";
	FILE*file=0;
	if(elf_get_tag(S,0)==elf_tag_String){
		name=elf_get_text(S,0);
	} else {
		file=elf_get_sysobj(S,0);
	}
	contents=elf_read_file_string(S,name,file,size,pos);
	elf_push_string_raw(S,contents);
	return 1;
}

ELF_FUNCTION(l_sys_write_file) {
	if(elf_get_tag(S,0) == elf_tag_String) {
		_write_file(S,elf_get_text(S,0),elf_get_text(S,1));
	} else {
		_write_file_io(S,elf_get_sysobj(S,0),elf_get_text(S,1));
	}
	return 0;
}

ELF_FUNCTION(l_sys_write_file_to_file) {
	FILE *dst,*src;
	if(elf_get_tag(S,0) == elf_tag_String) {
		dst = fopen(elf_get_text(S,0),"wb");
	} else {
		dst = (FILE *) elf_get_sysobj(S,0);
	}
	if(elf_get_tag(S,1) == elf_tag_String) {
		src = fopen(elf_get_text(S,1),"rb");
	} else {
		src = (FILE *) elf_get_sysobj(S,1);
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
	int ok = sys_set_work_dir(elf_get_text(S,0));
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
	{"list_folder", l_sys_list_folder},
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