/*
** See Copyright Notice In elf.h
** elf-run.c
** Auxiliary Functions / Tools
*/


/* res is how much to reserve,
and com is how much to commit */
elInteger elf_array_allocate(void **var, elInteger per, elInteger res, elInteger com) {
	elArray *arr = 0;
	elInteger max = 0, min = 0;
	if (*var != 0) {
		arr = &ARRAY(*var);
		max = arr->max;
		min = arr->min;
	}
   /* increment reserve if we attempt to commit
   more than we've got reserved */
	if (max + res < com) {
		res += (com - (max + res));
	}
	if (min + res > max) {
		max <<= 1;
		if(min + res > max) {
			max = min + res;
		}
		arr = elf_realloc(elHEAP_ALLOCATOR,sizeof(elArray)+per*max,arr);
	}
	if (arr != 0) {
		arr->max = max;
		arr->min = min + com;
	}
	*var = arr + 1;
	return min;
}


void elf_debugger(char *message) {
	sys_consolelog(ELF_LOGDBUG,"debugger: ");
	sys_consolelog(ELF_LOGDBUG,message);
	sys_consolelog(ELF_LOGDBUG,"end");
	sys_debugger();
}


void elf_register_bindings(elState *R, elTable *tab, elBinding *list, int num) {
	for (int i = 0; i < num; i += 1) {
		elf_tset(tab,elSTR(elf_new_string(R,list[i].name)),elCFN(list[i].fn));
	}
}


void elf_register_handle(elState *R, char *name, elHandle val) {
	elf_add_global_value(R->M,elf_xstr(R,name),elSYS(val));
}


void elf_register_integer(elState *R, char *name, elInteger val) {
	elf_add_global_value(R->M,elf_xstr(R,name),elINT(val));
}


void elf_registertab(elState *R, char *name, elTable *val) {
	elf_add_global_value(R->M,elf_xstr(R,name),elTAB(val));
}


void elf_register_string(elState *R, char *name, char *val) {
	elf_add_global_value(R->M,elf_xstr(R,name),elSTR(elf_xstr(R,val)));
}


void elf_register_binding(elState *R, char *name, elCFunction fn) {
	elf_add_global_value(R->M,elf_xstr(R,name),elCFN(fn));
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


int elf_get_file_for_byte(elModule *M, elByteId byte) {
	elFileProto *files = M->files;
	FOR_ARRAY(i,files) {
		elFileProto file = files[i];
		if (elWITHIN(byte,file.bytes,file.bytes+file.nbytes)) {
			return i;
		}
	}
	return -1;
}


elFileline elf_get_line_for_byte(elModule *M, elByteId byte) {
	elASSERT(elWITHIN(byte,0,ARRAY_LENGTH(M->lines)));
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


elFileProto elf_getrunningfile(elState *S) {
	elFileProto fi = {0};
	int id = elf_get_file_for_byte(S->M,S->byte);
	if (id != -1) fi = S->M->files[id];
	return fi;
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
	if (call->caller == 0) return;

	elASSERT(level > 0);

	elf_dump_byte_trace(S,call->caller,level-1);

	elModule *M = S->M;
	int fileid = elf_get_file_for_byte(M,call->origin);
	if (fileid != -1) {
		elFileProto *file = &M->files[fileid];
		elFileline line = elf_get_line_for_byte(M,call->origin);
		elf_line_dialog(file->name->text,file->contents->text,line,call->origin,M->bytes[call->origin],call->closure != 0 ? "(bytecode function)" : "(binding)");
	}
}


void elf_fail(elState *R, elByteId byte, char *error) {
	elModule *M = R->M;
	if (byte == NO_BYTE) byte = R->byte;
	char *line = elf_get_line_for_byte(M,byte);
	int fileid = elf_get_file_for_byte(M,byte);
	if (fileid != -1) {
		elFileProto *file = &M->files[fileid];
		elf_line_dialog(file->name->text,file->contents->text,line,byte,M->bytes[byte],error);
	}

	printf(" -- BYTE TRACE:\n");
	elf_dump_byte_trace(R,elGETFRAME(R),R->nframe);
	elf_debugger("runtime throw");
}


void elf_check_args(elState *R, char *fnname, int n, char *usage) {
	if (elGETNARGS(R) != n) {
		elf_fail(R,R->byte,elf_tpf("'%s': expects %i argument(s), you gave %i, usage: %s",fnname,n,elGETNARGS(R),usage));
	}
}


int elf_type_check(elState *R, elByteId id, elRegId loc, elValueTag x, elValueTag y) {
	if (x != y) {
		elf_fail(R,id,elf_tpf("$%i, expected %s, instead got %s",loc,tag2s[x],tag2s[y]));
	}
	return x == y;
}