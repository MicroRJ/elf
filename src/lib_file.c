//
// See Copyright Notice In elf.h
//

static elf_String *elf_read_file_string(elf_State *R, char *name, FILE *io, int size, int pos) {
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

	contents = elf_new_string2(R,size);
	// fseek(io,0,SEEK_SET);
	fread(contents->text,1,size,io);

	if(autoclose){
		fclose(io);
		elf_debug_log("'%s': file read successfully",name);
	}
	esc:
	return contents;
}

static int _write_file_io(elf_State *R, FILE *io, char *text) {
	ASSERT(io != 0);
	return fwrite(text,1,strlen(text),io);
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
	ASSERT(elf_get_num_args(R)==1);
	FILE *file=(FILE *)elf_get_sysobj(R,0);
	if(file){
		fclose(file);
	}
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
	fseek(file,0,SEEK_SET);
	elf_add_int(R,size);
	return 1;
}

int core_lib_read_file(elf_State *R) {
	elf_String *contents=0;
	int pos=-1,size=-1;
	char*name="noname";
	FILE*file=0;
	if(elf_get_tag(R,0)==elf_tag_str){
		name=elf_get_text(R,0);
	} else {
		file=elf_get_sysobj(R,0);
	}
	contents=elf_read_file_string(R,name,file,size,pos);

	// FILE *io;
	// if(elf_get_tag(R,0)==elf_tag_str){
	// 	io=fopen(elf_get_text(R,0),"rb");
	// } else {
	// 	io=elf_get_sysobj(R,0);
	// }
	// if(io){
	// 	contents=elf_read_file_string(R,io,size,pos);
	// 	if(elf_get_tag(R,0)==elf_tag_str) {
	// 		fclose(io);
	// 	}
	// }
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

	elf_add_int(R,wrote);
	return 1;
}



int core_lib_change_work_dir(elf_State *R) {
	int ok = sys_set_work_dir(elf_get_text(R,0));
	elf_add_int(R,ok);
	return 1;
}


int core_lib_get_work_dir(elf_State *R) {
	char buf[MAX_PATH];
	sys_get_work_dir(sizeof(buf),buf);
	elf_new_string(R,buf);
	return 1;
}