/*
** See Copyright Notice In elf.h
** elf-aux.c
** Auxiliary Stuff
*/


static void fpf_byte(FILE *io, elf_Module *M, elf_Int fid, Instr id, elf_bytecode b);


// todo: remove, this is not core functionality
void elf_add_lib(elf_State *S, char *prefix, NameFunctionPair *bindings, int num) {
	// todo: ensure the symbol name is valid
	for(int i = 0; i < num; i ++) {
		char *temp = bindings[i].name;
		if(prefix) temp = elf_tpf("%s.%s",prefix,temp);

		elf_String *name = elf_alloc_string(S,temp);
		// elf_debug_log("	lib: %s",name->text);
		elf_table_set(S->M->globals, VALUE_STRING(name), VALUE_FUNCTION(bindings[i].fn));
	}
}

static int get_byte_class(int op);

void elf_debugger(char *message) {
	sys_console_print(LOG_KDEBUG,"debugger: ");
	sys_console_print(LOG_KDEBUG,message);
	sys_console_print(LOG_KDEBUG,"end");
	sys_debugger();
}

int elf_query_file_for_instr(elf_Module *M, int byte) {
	FOR_ARRAY(i,M->files) {
		elf_File file = M->files[i];
		if (WITHIN(byte,file.pos,file.end)) {
			return i;
		}
	}
	return -1;
}


char *elf_get_instr_line(elf_Module *M, Instr byte) {
	ASSERT(WITHIN(byte,0,ARRAY_LENGTH(M->lines)));
	if (M->lines) {
		return M->lines[byte];
	}
	return "";
}

void elf_get_line_source_info(char *q, char *loc, int *linenum, char **lineloc) {
	//
	// get line number and starting address
	// of the line for the given address within
	// the file q
	//

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

void elf_line_dialog(char *filename, char *contents, char *loc, Instr byte_loc, elf_bytecode byte, char const *fmt, ...) {
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
		printf("%s [%i:%lli] [%i](%s): %s\n",filename,linenum,(elf_Int)(1+loc-lineloc),byte_loc,byte2s[BC_OP(byte)],b);
	}
	printf("| %.*s\n",linelen,lineloc);
	printf("| %.*s\n",underline+1,u);
}

void elf_dump_byte_trace(elf_State *S) {
	elf_Module *M = S->M;
	S->frame_stack[S->frame_index] = S->frame;
	for(int i=1; i<=S->frame_index; i++){
		elf_Stack_Frame *frame = & S->frame_stack[i];
		int id = elf_query_file_for_instr(M,frame->bytecounter);
		if (id != -1) {
			elf_File *file = &M->files[id];
			Source line = elf_get_instr_line(M,frame->bytecounter);
			elf_line_dialog(file->name->text,file->contents->text,line,frame->bytecounter,M->bytes[frame->bytecounter],frame->closure != 0 ? "(elf-function)" : "(c-function)");
		}
	}
}

// todo:
// for this to work properly we'd have to construct
// some sort of dependency graph to figure out why
// something happened
// also, ideally it would only print the associated
// instructions, or at least highlight them
void elf_analyze_exec_trail(elf_State *S) {
	elf_i32 range_x0 = 0;
	elf_i32 range_y0 = 0;
	elf_i32 range_x1 = 0;
	elf_i32 range_y1 = 0;
	elf_i32 mask = S->exec_trail_capacity - 1;
	// todo: make this neater
	if (S->exec_trail_index > S->exec_trail_capacity) {
		range_x0 = S->exec_trail_index & mask;
		range_y0 = S->exec_trail_capacity;
		if (range_x0) {
			range_x1 = 0;
			range_y1 = range_x0 + 1;
		}
	} else {
		range_x0 = 0;
		range_y0 = S->exec_trail_index;
	}
	for(elf_i32 i = range_x0; i < range_y0; i ++) {
		elf_trail_entry entry = S->exec_trail[i];
		int id = elf_query_file_for_instr(S->M,entry.address);
		fpf_byte(stdout,S->M,id,entry.address,entry.bytecode);
	}
	for(elf_i32 i = range_x1; i < range_y1; i ++) {
		elf_trail_entry entry = S->exec_trail[i];
		int id = elf_query_file_for_instr(S->M,entry.address);
		fpf_byte(stdout,S->M,id,entry.address,entry.bytecode);
	}
}

void elf_error(elf_State *R, int byte, const char *error) {
	/* Alternatively, do proper coloring... */
	printf("\n\n");
	printf("\txxxxxxxxxx:\n");
	printf("\txx FAIL xx:\n");
	printf("\txxxxxxxxxx:\n\n");

	elf_Module *M = R->M;
	if (byte == NO_BYTE) byte = R->byte;
	char *line = elf_get_instr_line(M,byte);
	int fileid = elf_query_file_for_instr(M,byte);
	if (fileid != -1) {
		elf_File *file = &M->files[fileid];
		elf_line_dialog(file->name->text,file->contents->text,line,byte,M->bytes[byte],error);
	} else {
		printf("error: %s\n",error);
		printf("source information could not be found, file id: %i\n",fileid);
	}

	printf(" -- BYTE TRACE:\n");
	elf_dump_byte_trace(R);
	elf_analyze_exec_trail(R);
	elf_debugger("error");
}

int elf_type_check(elf_State *R, Instr id, elf_StackId loc, elf_tag_enum x, elf_tag_enum y) {
	if (x != y) {
		elf_error(R,id,elf_tpf("$%i, expected %s, instead got %s",loc,tag2s[x],tag2s[y]));
	}
	return x == y;
}


static int fpf_value(FILE *file, elf_Value v, elf_Bool flags) {
	switch (v.tag) {
		case elf_tag_nil: return fprintf(file,"nil");
		case elf_tag_sysobj: return fprintf(file,"h%llX",v.x_int);
		case elf_tag_int: return fprintf(file,"%lli",v.x_int);
		case elf_tag_num: return fprintf(file,"%f",v.x_num);
		case elf_tag_closure: return fprintf(file,"F()");
		case elf_tag_proc: return fprintf(file,"C()");
		case elf_tag_tab: {
			/* todo: this is slow! */
			int wrote = 0;
			elf_Table *tab = v.x_tab;
			wrote += fprintf(file,"{");
			elf_Int i,j,n;
			for (i=0;i<ARRAY_LENGTH(tab->array);++i) {
				if (i != 0) wrote += fprintf(file,", ");
				for (j=0,n=0;j<tab->ntotal;++j) {
					elf_table_entry it = tab->slots[j];
					if (it.key.tag==elf_tag_nil) continue;
					if (it.idx!=i) continue;
					if (n ++ != 0) wrote += fprintf(file,", ");
					wrote += fpf_value(file,it.key,1);
				}
				if (n != 0) wrote += fprintf(file," = ");
				wrote += fpf_value(file,tab->array[i],1);
			}
			// for (i=0,n=0;i<tab->nslots;++i) {
			// 	elf_table_entry it = tab->slots[i];
			// 	if (it.key.tag == elf_tag_nil) continue;
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
		case elf_tag_str: {
			if (flags) {
				return fprintf(file,"\"%s\"",v.x_str->text);
			} else {
				return fprintf(file,"%s",v.x_str->text);
			}
		} break;
		default: return fprintf(file,"(?)");
	}
}


static void fpf_byte(FILE *io, elf_Module *M, elf_Int fid, Instr id, elf_bytecode b) {
	if (fid != -1) {
		elf_File file = M->files[fid];
		int linenum;
		elf_get_line_source_info(file.contents->text,M->lines[id],&linenum,0);
		fprintf(io,"%s %04i: \t",file.name->text,linenum);
	}

	fprintf(io,"%04i\t%s"
	, id,byte2s[BC_OP(b)]);

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
		if ((val.tag==elf_tag_str)||(val.tag==elf_tag_num)||(val.tag==elf_tag_int)) {
			fpf_value(io,val,1);
		}
	}
	fprintf(io,"\n");
}



#if 0
void elf_get_line_source_info(char *q, char *p, int *linenum, char **lineloc);


// holy... this function is old... is not even the same name,
// is not even the same coding style...
void lang_dumpmodule(elf_Module *md, elf_Handle io) {
	fprintf(file,"elf_Module:\n");
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
			elf_bytecode b = md->bytes[ff.bytes+j];
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
			elf_bytecode b = md->bytes[p.bytes+j];
			fpf_byte(md,file,j,b);
		}
		fprintf(file,"end\n");
	}
#endif
}
#endif

// todo: remove this!
int get_byte_class(int k) {
#define BCITEM(NAME,FMT,__) case XFUSE(BC_,NAME): return XFUSE(BC_CLASS_,FMT);
	switch (k) {
		BCDEF(BCITEM)
		default: NO_CODE;
	}
#undef BCITEM
	return -1;
}