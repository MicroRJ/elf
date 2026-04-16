//
// See Copyright Notice In elf.h
//



static int findfileforinstr(elf_State *S, int byte) {
	FOR_ARRAY(i, S->files)
	{
		if (byte >= S->files[i]->bytepos && byte < S->files[i]->byteend) {
			return i;
		}
	}
	return -1;
}



static void printsourcelocator(char *name, char *source, char *cursor, BCPos instr, Bytecode byte) {
	char *line_start;
	int line_number = get_source_info(source,cursor,&line_start);

	while (*line_start == '\t' || *line_start == ' ') {
		line_start += 1;
	}

	char underline_buf[64];

	int underline = MIN(sizeof(underline_buf), cursor - line_start);
	line_start = cursor - underline;

	int line_length = 0;
	for (; line_length < underline+32; ++ line_length) {
		if (line_start[line_length] == '\0') break;
		if (line_start[line_length] == '\r') break;
		if (line_start[line_length] == '\n') break;
	}

	// mimic tabs because we don't know how large tabs look,
	// alternatively we could replace tabs in the source...
	for (int i = 0; i < underline; ++ i) {
		underline_buf[i] = line_start[i] == '\t' ? '\t' : '-';
	}
	underline_buf[underline]='^';

	int char_index = 1 + cursor - line_start;

	printf("%s [%i:%i] [%i](%s): \n"
	, name
	, line_number
	, char_index
	, instr
	, Static_StrFromBytecode[BYTECODE_TYPE(byte)]);

	printf("|\n");
	printf("| %.*s\n",line_length,line_start);
	printf("| %.*s\n",underline+1,underline_buf);
	printf("|\n");
}

void printcallstack(elf_State *S) {

	for(int i=1; i<S->frame_index; i++)
	{
		StackFrame *frame = & S->frame_stack[i];
		BCPos instr = frame->bytes + frame->nextinstr;

		int id = findfileforinstr(S, instr);

		if (id != -1) {

			BytecodeFile *file = S->files[id];
			Source line = S->lines[instr];

			printsourcelocator(file->name,file->data,line,instr,S->bytebuf[instr]);
		}

	}
}









void reporterrorf(elf_State *S, int instr, const char *format, ...)
{

	if (instr == NO_BYTE)
	{
		instr = S->byte;
	}

	printf("\n\n");
	printf("============= ELF-ERROR =============\n");

	printf("\n\n");
	printcallstack(S);

	printf("\n\n");


	// todo: instead of keeping source code in memory, which is
	// kinda weird, make a hash of the file we loaded the code from,
	// and get a path to it, if the hash didn't change we reload the
	// file, and always make sure to print a notice saying the error
	// report could be inaccurate if the source file changed.
	//
	// Whether we store the contents of the file in our own memory
	// can be done, but it should be separate, only necessary in moments
	// like this...
	//
	Source line = S->lines ? S->lines[instr] : 0;

	int fidi = findfileforinstr(S, instr);

	va_list vargs;
	va_start(vargs, format);
	char *error = thread_format_v(format, vargs);
	va_end(vargs);

	if (fidi != -1) {

		BytecodeFile *file = S->files[fidi];
		printsourcelocator(file->name, file->data, line, instr, S->bytebuf[instr]);
	}
	else
	{
		printf("source information could not be found, file id: %i\n", fidi);
	}

	printf("[ERROR]: %s\n", error);

	printf("\n\n");




	printf("\n\n");

#if defined(_DEBUG)
	__debugbreak();
#else
	printf("elf is exiting...\n");
	sys_exit_this_process(0);
#endif
}


void reporterror(elf_State *inter, int byte, const char *message)
{
	reporterrorf(inter, byte, message);
}



#if 0


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
		int id = findfileforinstr(S, entry.address);
		fpf_byte(stdout,S,id,entry.address,entry.bytecode);
	}
	for(elf_i32 i = range_x1; i < range_y1; i ++) {
		elf_trail_entry entry = S->exec_trail[i];
		int id = findfileforinstr(S, entry.address);
		fpf_byte(stdout,S,id,entry.address,entry.bytecode);
	}
}

static void fpf_byte(FILE *io, elf_Module *M, elf_Integer fid, BCPos id, Bytecode b) {
	if (fid != -1) {
		BytecodeFile file = M->files[fid];
		int linenum;
		get_source_info(file.contents->text,M->lines[id],&linenum,0);
		fprintf(io,"%s %04i: \t",file.name->text,linenum);
	}

	fprintf(io,"%04i\t%s"
	, id,Static_StrFromBytecode[BYTECODE_TYPE(b)]);

	if (get_byte_class(BYTECODE_TYPE(b)) == BYTECODE_CLASS_XYZ) {
		fprintf(io,"(x=%i,y=%i,z=%i)",BYTECODE_ARGX(b),BYTECODE_ARGY(b),BYTECODE_ARGZ(b));
	} else if (get_byte_class(BYTECODE_TYPE(b)) == BYTECODE_CLASS_XY) {
		fprintf(io,"(x=%i,y=%i)",BYTECODE_ARGX(b),BYTECODE_ARGY(b));
	} else {
		fprintf(io,"(x=%i)",BYTECODE_ARGX(b));
	}

	if (BYTECODE_TYPE(b) == BYTECODE_TYPEGUARD) {
		fprintf(io," #%s",tag2s[BYTECODE_ARGY(b)]);
	} else
	if (BYTECODE_TYPE(b) == BYTECODE_LOADKINT) {
		fprintf(io," #%lli",M->integers[BYTECODE_ARGY(b)]);
	} else
	if (BYTECODE_TYPE(b) == BYTECODE_LOADKNUM) {
		fprintf(io," #%f",M->numbers[BYTECODE_ARGY(b)]);
	} else
	if (BYTECODE_TYPE(b) == BYTECODE_GETGLOBAL) {
		elf_Value val = M->globals->array[BYTECODE_ARGY(b)];
		fprintf(io,"  // %s ",tag2s[val.tag]);
		/* todo: just pass in a flag to val fpf that tells
		it to shorten the thing for printing purposes */
		if ((val.tag==ELF_VALUE_TYPE_STRING)||(val.tag==ELF_VALUE_TYPE_NUMBER)||(val.tag==ELF_VALUE_TYPE_INTEGER)) {
			// fpf_value(io,val,1);
		}
	}
	fprintf(io,"\n");
}
#endif


#if 0
void get_source_info(char *q, char *p, int *linenum, char **lineloc);


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
		BytecodeFunction ff = md->files[i];
		fprintf(io,"- FILE (%s):\n",ff.name->text);
		fprintf(io,"INDEX INSTRUCTION\n");
		for (BCPos j = 0; j < ff.nbytes; ++j) {
			Bytecode b = md->bytes[ff.bytes+j];
			// int linenum;
			// char *lineloc;
			// get_source_info(md->file,md->lines[j],&linenum,&lineloc);
			// fprintf(file,"%-3i:%-3i",linenum,(int)(md->lines[j]-lineloc));
			fpf_byte(io,md,i,j,b);
		}
	}
#if 0
	FOR_ARRAY(md->p) {
		BytecodeFunction p = md->p[i];
		fprintf(file,"FUNC: [%i] %i,%i (%i:%i):\n",(int)i,p.bytes,p.nbytes,p.x,p.nlocals);
		for (BCPos j = 0; j < p.nbytes; ++j) {
			Bytecode b = md->bytes[p.bytes+j];
			fpf_byte(md,file,j,b);
		}
		fprintf(file,"end\n");
	}
#endif
}
#endif
