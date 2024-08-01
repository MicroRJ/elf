/*
** See Copyright Notice In elf.h
** elf-run.c
** Auxiliary Functions
*/


void elf_debugger(char *message) {
	sys_consolelog(ELF_LOGDBUG,"debugger: ");
	sys_consolelog(ELF_LOGDBUG,message);
	sys_consolelog(ELF_LOGDBUG,"end");
	sys_debugger();
}


void elf_register_handle(elState *R, char *name, elHandle val) {
	elf_add_global_value(R->M,elf_add_new_string(R,name),elf_handle_value(val));
}


void elf_register_integer(elState *R, char *name, elInteger val) {
	elf_add_global_value(R->M,elf_add_new_string(R,name),elf_integer_value(val));
}


void elf_registertab(elState *R, char *name, elTable *val) {
	elf_add_global_value(R->M,elf_add_new_string(R,name),elf_tab(val));
}


void elf_register_string(elState *R, char *name, char *val) {
	elf_add_global_value(R->M,elf_add_new_string(R,name),elf_string_value(elf_add_new_string(R,val)));
}


void elf_register_binding(elState *R, char *name, elBinding fn) {
	elf_add_global_value(R->M,elf_add_new_string(R,name),elf_binding_value(fn));
}


elInteger elf_clocktime() {
	return sys_clocktime();
}


/* todo: clockhz can be cached */
elNumber elf_timediffs(elInteger begin) {
	return (sys_clocktime() - begin) / (elNumber) sys_clockhz();
}


elNumber elf_timediffms(elInteger begin) {
	return elf_timediffs(begin) * 1000.;
}


int elf_find_file_info_by_byte(elModule *md, elByteId byte) {
	elFileInfo *files = md->files;
	int nfiles = array_length(files);
	for (int x = 0; x < nfiles; ++ x) {
		elFileInfo file = files[x];
		if ((elInteger)(byte - file.bytes) < file.nbytes) {
			return x;
		}
	}
	return -1;
}


elFileLine elf_get_line_for_byte(elModule *M, elByteId byte) {
	return M->lines[byte];
}


/* finds line number and line loc from single source location */
void elf_get_line_location_info(char *q, char *loc, int *linenum, char **lineloc) {
	char *c = q;
	int n = 0;
	while (q < loc) {
		while ((*q != '\r' && *q != '\n' && *q != '\0') && q < loc) q ++;
		if (*q == 0) break;
		if ((*q != '\n') || (c = ++ q, n ++, 1)) {
			if ((*q == '\r') && (c = ++ q, n ++, 1)) {
				if (*q == '\n') c = ++ q;
			} else q ++;
		}
	}
	if (linenum != 0) *linenum = n + 1;
	if (lineloc != 0) *lineloc = c;
}


elFileInfo elf_getrunningfile(elState *S) {
	elFileInfo fi = {0};
	int id = elf_find_file_info_by_byte(S->M,S->byte);
	if (id != -1) fi = S->M->files[id];
	return fi;
}


elFileLine elf_getrunningline(elState *S) {
	return S->M->lines[S->byte];
}


void elf_line_dialog(char *filename, char *contents, char *loc, elByteId byte_loc, elBytecode byte, char const *fmt, ...) {
	int linenum;
	char *lineloc;
	elf_get_line_location_info(contents,loc,&linenum,&lineloc);

	/* skip initial blank characters for optimal gimmicky */
	while (*lineloc == '\t' || *lineloc == ' ') {
		lineloc += 1;
	}

	char u[0x40];

	int underline = loc - lineloc;
	if (underline >= sizeof(u)) {
		underline = sizeof(u)-1;
		lineloc = loc - underline;
	}

	int linelen = 0;
	for (; linelen < underline+32; ++ linelen) {
		if (lineloc[linelen] == '\0') break;
		if (lineloc[linelen] == '\r') break;
		if (lineloc[linelen] == '\n') break;
	}

	for (int i = 0; i < underline; ++ i) {
		u[i] = lineloc[i] == '\t' ? '\t' : '-';
	}
	u[underline]='^';

	if (fmt !=  0) {
		char b[0x1000];
		va_list v;
		va_start(v,fmt);
		stbsp_vsnprintf(b,sizeof(b),fmt,v);
		va_end(v);
		printf("%s [%i:%lli] [%i](%s): %s\n",filename,linenum,(elInteger)(1+loc-lineloc),byte_loc,elf_get_byte_label(byte.k),b);
	}
	printf("| %.*s\n",linelen,lineloc);
	printf("| %.*s\n",underline+1,u);
}


void elf_dump_byte_trace(elState *S, elStackFrame *call, int level) {

	elASSERT(level > -1);

	/* Don't show the first root call frame
	(which is the one without a caller) because that'll
	just be the first instruction that executed for that
	function/file, which is irrelevant */
	if (call->caller == elNil) return;

	elASSERT(level > 0);

	elf_dump_byte_trace(S,call->caller,level-1);

	elModule *M = S->M;
	int fileid = elf_find_file_info_by_byte(M,call->head);
	if (fileid != -1) {
		elFileInfo *file = &M->files[fileid];
		elFileLine line = M->lines[call->head];
		elf_line_dialog(file->name,file->lines,line,
		call->head,M->bytes[call->head],call->cl != elNil ? "(bytecode function)" : "(binding)");
	}
}


void elf_throw(elState *R, elByteId byte, char *error) {
	elModule *M = R->M;
	if (byte == NO_BYTE) byte = R->byte;
	elFileLine line = M->lines[byte];
	int fileid = elf_find_file_info_by_byte(M,byte);
	if (fileid != -1) {
		elFileInfo *file = &M->files[fileid];
		elf_line_dialog(file->name,file->lines,line,R->byte,
		M->bytes[R->byte],error);
	}

	printf(" -- BYTE TRACE:\n");
	elf_dump_byte_trace(R,R->call,R->call_level);
	elf_debugger("runtime throw");
}


void elf_check_args(elState *R, char *fnname, int n, char *usage) {
	if (R->call->nx != n) {
		elf_throw(R,R->byte,elf_tpf("'%s': expects %i argument(s), you gave %i, usage: %s",fnname,n,R->call->nx,usage));
	}
}


int elf_type_check(elState *R, elByteId id, elRegId loc, elValueTag x, elValueTag y) {
	if (x != y) {
		elf_throw(R,id,elf_tpf("$%i, expected %s, instead got %s",loc,tag2s[x],tag2s[y]));
	}
	return x == y;
}