/*
** See Copyright Notice In elf.h
** elf-aux.c
** Auxiliary Stuff
*/


int elf_add_const_int(elf_State *S, elf_Int i) {
	int index = ARRAY_GROW(S->M->integers,1);
	S->M->integers[index] = i;
	return index;
}
int elf_add_const_num(elf_State *S, elf_Num i) {
	int index = ARRAY_GROW(S->M->numbers,1);
	S->M->numbers[index] = i;
	return index;
}
int elf_add_proto(elf_State *S) {
	int index = ARRAY_GROW(S->M->protos,1);
	return index;
}

int elf_get_global(BC_Module *M, elf_String *name) {
	if (name != 0) return elf_tget_ornew(M->globals,VSTR(name));
	return ARRAY_GROW(M->globals->array,1);
}


int elf_set_global(BC_Module *M, elf_String *name, elf_Value value) {
	int id = elf_get_global(M,name);
	M->globals->array[id] = value;
	return id;
}



static char const *get_byte_label(int op);
static int get_byte_class(int op);


void elf_debugger(char *message) {
	sys_console_print(LOG_KDEBUG,"debugger: ");
	sys_console_print(LOG_KDEBUG,message);
	sys_console_print(LOG_KDEBUG,"end");
	sys_debugger();
}


elf_Int elf_get_clock_time() {
	return sys_get_clock_time();
}


elf_Num elf_time_diff_s(elf_Int begin) {
	return (sys_get_clock_time() - begin) / (elf_Num) sys_get_clock_freq();
}


elf_Num elf_time_diff_ms(elf_Int begin) {
	return elf_time_diff_s(begin) * 1000.;
}

int elf_get_instr_file(BC_Module *M, Instr byte) {
	elf_Proto *files;
	elf_Proto file;

	files=M->files;
	FOR_ARRAY(i,files) {
		file=files[i];
		if (WITHIN(byte,file.bytes,file.bytes+file.nbytes)) {
			return i;
		}
	}
	return -1;
}


char *elf_get_instr_line(BC_Module *M, Instr byte) {
	ASSERT(WITHIN(byte,0,ARRAY_LENGTH(M->lines)));
	if (M->lines) {
		return M->lines[byte];
	}
	return "";
}


/* todo: this is so generic, it could just be part of
the text api */
void elf_get_line_source_info(char *q, char *loc, int *linenum, char **lineloc) {
	char *c = q;
	int n = 0;
	while (q < loc) {
		while (((*q != '\r') && (*q != '\n') && (*q != '\0')) && (q < loc)) q ++;
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


elf_Proto elf_get_running_file(elf_State *S) {
	elf_Proto fi = {0};
	int id = elf_get_instr_file(S->M,S->byte);
	if (id != -1) fi = S->M->files[id];
	return fi;
}


void elf_line_dialog(char *filename, char *contents, char *loc, Instr byte_loc, Bytecode byte, char const *fmt, ...) {
	int linenum;
	char *lineloc;
	elf_get_line_source_info(contents,loc,&linenum,&lineloc);

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
		printf("%s [%i:%lli] [%i](%s): %s\n",filename,linenum,(elf_Int)(1+loc-lineloc),byte_loc,get_byte_label(BC_OP(byte)),b);
	}
	printf("| %.*s\n",linelen,lineloc);
	printf("| %.*s\n",underline+1,u);
}


void elf_dump_byte_trace(elf_State *S, elf_StackFrame *call, int level) {
	ASSERT(level > -1);

	/* Don't show the first root call frame
	(which is the one without a caller) because that'll
	just be the first instruction that executed for that
	function/file, which is irrelevant */
	if (call->caller == 0) return;

	ASSERT(level > 0);

	elf_dump_byte_trace(S,call->caller,level-1);

	BC_Module *M = S->M;
	int fileid = elf_get_instr_file(M,call->origin);
	if (fileid != -1) {
		elf_Proto *file = &M->files[fileid];
		Source line = elf_get_instr_line(M,call->origin);
		elf_line_dialog(file->name->text,file->contents->text,line,call->origin,M->bytes[call->origin],call->closure != 0 ? "(elf-function)" : "(c-function)");
	}
}

void elf_fail_(elf_State *R, int byte, const char *error) {
	/* Alternatively, do proper coloring... */
	printf("\n\n");
	printf("\txxxxxxxxxx:\n");
	printf("\txx FAIL xx:\n");
	printf("\txxxxxxxxxx:\n\n");

	BC_Module *M = R->M;
	if (byte == NO_BYTE) byte = R->byte;
	char *line = elf_get_instr_line(M,byte);
	int fileid = elf_get_instr_file(M,byte);
	if (fileid != -1) {
		elf_Proto *file = &M->files[fileid];
		elf_line_dialog(file->name->text,file->contents->text,line,byte,M->bytes[byte],error);
	} else {
		printf("error: %s\n",error);
	}

	printf(" -- BYTE TRACE:\n");
	elf_dump_byte_trace(R,GET_FRAME(R),R->nframe);
	elf_debugger("runtime throw");
}


void elf_check_args(elf_State *R, char *fnname, int n, char *usage) {
	if (elf_get_num_args(R) != n) {
		elf_fail(R,R->byte,elf_tpf("'%s': expects %i argument(s), you gave %i, usage: %s",fnname,n,elf_get_num_args(R),usage));
	}
}


int elf_type_check(elf_State *R, Instr id, elf_StackId loc, elf_ValueTag x, elf_ValueTag y) {
	if (x != y) {
		elf_fail(R,id,elf_tpf("$%i, expected %s, instead got %s",loc,tag2s[x],tag2s[y]));
	}
	return x == y;
}


static int fpf_value(FILE *file, elf_Value v, elf_Bool flags) {
	switch (v.tag) {
		case elf_TAG_NIL: return fprintf(file,"nil");
		case elf_TAG_SYS: return fprintf(file,"h%llX",v.x_int);
		case elf_TAG_INT: return fprintf(file,"%lli",v.x_int);
		case elf_TAG_NUM: return fprintf(file,"%f",v.x_num);
		case elf_TAG_CLS: return fprintf(file,"F()");
		case elf_TAG_CFN: return fprintf(file,"C()");
		case elf_TAG_FLOAT2: return fprintf(file,"float2(%f,%f)",v.x_f32,v.y_f32);
		case elf_TAG_TAB: {
			/* todo: this is slow! */
			int wrote = 0;
			elf_Table *tab = v.x_tab;
			wrote += fprintf(file,"{");
			elf_Int i,j,n;
			for (i=0;i<ARRAY_LENGTH(tab->array);++i) {
				if (i != 0) wrote += fprintf(file,", ");
				for (j=0,n=0;j<tab->ntotal;++j) {
					elf_Entry it = tab->slots[j];
					if (it.key.tag==elf_TAG_NIL) continue;
					if (it.idx!=i) continue;
					if (n ++ != 0) wrote += fprintf(file,", ");
					wrote += fpf_value(file,it.key,1);
				}
				if (n != 0) wrote += fprintf(file," = ");
				wrote += fpf_value(file,tab->array[i],1);
			}
			// for (i=0,n=0;i<tab->nslots;++i) {
			// 	elf_Entry it = tab->slots[i];
			// 	if (it.key.tag == elf_TAG_NIL) continue;
			// 	if (n ++ != 0) wrote += fprintf(file,", ");
			// 	wrote += fpf_value(file,it.key,1);
			// 	wrote += fprintf(file," = ");
			// 	wrote += fpf_value(file,tab->array[it.i],1);
			// }
			// FOR_ARRAY(t->v) {
			// 	if (i != 0) wrote += fprintf(file,", ");
			// 	wrote += fpf_value(file,t->v[i],1);
			// }
			wrote += fprintf(file,"}");
			return wrote;
		} break;
		case elf_TAG_STR: {
			if (flags) {
				return fprintf(file,"\"%s\"",v.x_str->text);
			} else {
				return fprintf(file,"%s",v.x_str->text);
			}
		} break;
		default: return fprintf(file,"(?)");
	}
}


static void fpf_byte(FILE *io, BC_Module *M, elf_Int fid, Instr id, Bytecode b) {
	if (fid != -1) {
		elf_Proto file = M->files[fid];
		int linenum;
		elf_get_line_source_info(file.contents->text,M->lines[id],&linenum,0);
		fprintf(io,"%s %04i: \t",file.name->text,linenum);
	}

	fprintf(io,"%08i %04i\t%s"
	, M->track[id],id,get_byte_label(BC_OP(b)));

	if (get_byte_class(BC_OP(b)) == BC_CLASS_XYZ) {
		fprintf(io,"(x=%i,y=%i,z=%i)",BC_ARGX(b),BC_ARGY(b),BC_ARGZ(b));
	} else if (get_byte_class(BC_OP(b)) == BC_CLASS_XY) {
		fprintf(io,"(x=%i,y=%i)",BC_ARGX(b),BC_ARGY(b));
	} else {
		fprintf(io,"(x=%i)",BC_ARGX(b));
	}

	if (BC_OP(b) == BC_TYPEGUARD) {
		fprintf(io," #%s",tag2s[BC_ARGY(b)]);
	} else
	if (BC_OP(b) == BC_GETKINT) {
		fprintf(io," #%lli",M->integers[BC_ARGY(b)]);
	} else
	if (BC_OP(b) == BC_GETKNUM) {
		fprintf(io," #%f",M->numbers[BC_ARGY(b)]);
	} else
	if (BC_OP(b) == BC_GETGLOBAL) {
		elf_Value val = M->globals->array[BC_ARGY(b)];
		fprintf(io,"  // %s ",tag2s[val.tag]);
		/* todo: just pass in a flag to val fpf that tells
		it to shorten the thing for printing purposes */
		if ((val.tag==elf_TAG_STR)||(val.tag==elf_TAG_NUM)||(val.tag==elf_TAG_INT)) {
			fpf_value(io,val,1);
		}
	}
	fprintf(io,"\n");
}


void elf_get_line_source_info(char *q, char *p, int *linenum, char **lineloc);


#if 0
void lang_dumpmodule(BC_Module *md, elf_Handle io) {
	fprintf(file,"BC_Module:\n");
	fprintf(file,"Globals:\n");
	FOR_ARRAY(md->g->v) {
		fprintf(file,"%04llX: ", i);
		elf_fpf_value(file,md->g->v[i],1);
		fprintf(file,"\n");
	}
	fprintf(io,"-- BYTECODE --\n");
	fprintf(io,"- INSTR: %i\n",md->nbytes);
	fprintf(io,"- PID: %i\n",sys_get_my_pid());
	FOR_ARRAY(i,md->files) {
		elf_Proto ff = md->files[i];
		fprintf(io,"- FILE (%s):\n",ff.name->text);
		fprintf(io,"INDEX INSTRUCTION\n");
		for (Instr j = 0; j < ff.nbytes; ++j) {
			Bytecode b = md->bytes[ff.bytes+j];
			// int linenum;
			// char *lineloc;
			// elf_get_line_source_info(md->file,md->lines[j],&linenum,&lineloc);
			// fprintf(file,"%-3i:%-3i",linenum,(int)(md->lines[j]-lineloc));
			fpf_byte(io,md,i,j,b);
		}
	}
#if 0
	FOR_ARRAY(md->p) {
		elf_Proto p = md->p[i];
		fprintf(file,"FUNC: [%i] %i,%i (%i:%i):\n",(int)i,p.bytes,p.nbytes,p.x,p.nlocals);
		for (Instr j = 0; j < p.nbytes; ++j) {
			Bytecode b = md->bytes[p.bytes+j];
			fpf_byte(md,file,j,b);
		}
		fprintf(file,"end\n");
	}
#endif
}
#endif


int get_byte_class(int k) {
#define BCITEM(NAME,FMT,__) case XFUSE(BC_,NAME): return XFUSE(BC_CLASS_,FMT);
	switch (k) {
		BCDEF(BCITEM)
		default: NO_CODE;
	}
#undef BCITEM
	return -1;
}


char const *get_byte_label(int k) {
#define BCITEM(NAME,_,SYM) case XFUSE(BC_,NAME): return SYM;
	switch (k) {
		BCDEF(BCITEM)
		default: NO_CODE;
	}
#undef BCITEM
	return 0;
}
