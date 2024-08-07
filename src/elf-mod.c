/*
** See Copyright Notice In elf.h
** elf-mod.c
** Module
*/


/* todo: ensure that we don't have to replace symbols */
elSymbolId elf_get_global_symbol(elModule *M, elString *name) {
	if (name != 0) {
		return elf_table_lookup_index(M->globals,elSTR(name));
	} else return ARRAY_GROW(M->globals->array,1);
}


elSymbolId elf_add_global_value(elModule *M, elString *name, elValue v) {
	elSymbolId i = elf_get_global_symbol(M,name);
	M->globals->array[i] = v;
	return i;
}


elSymbolId elf_add_proto(elModule *M, elFileProto p) {
	elSymbolId i = ARRAY_GROW(M->p,1);
	M->p[i] = p;
	return i;
}



int elf_fpf_value(FILE *file, elValue v, elBool quotes);

void elf_bytefpf(FILE *io, elModule *M, elInteger fid, elByteId id, elBytecode b) {

	if (fid != -1) {
		elFileProto file = M->files[fid];
		int linenum;
		elf_get_line_location_info(file.contents->contents,M->lines[id],&linenum,0);
		fprintf(io,"%s %04i: \t",file.name->contents,linenum);
	}

	fprintf(io,"%08i %04i\t%s"
	, M->track[id],	id, elf_get_byte_label(b.k));
	if (elf_get_byte_class(b.k) == BC_CLASS_XYZ) {
		fprintf(io,"(x=%i,y=%i,z=%i)",b.x,b.y,b.z);
	} else
	if (elf_get_byte_class(b.k) == BC_CLASS_XY) {
		fprintf(io,"(x=%i,y=%i)",b.x,b.y);
	} else {
		fprintf(io,"(x=%lli)",b.i);
	}
	if (b.k == BC_TYPEGUARD) {
		fprintf(io," #%s",tag2s[b.y]);
	} else
	if (b.k == BC_LOADINT) {
		fprintf(io," #%lli",M->ki[b.y]);
	} else
	if (b.k == BC_LOADNUM) {
		fprintf(io," #%f",M->kn[b.y]);
	} else
	if (b.k == BC_LOADGLOBAL) {
		elValue val = M->globals->array[b.y];
		fprintf(io,"  // %s ",tag2s[val.tag]);
		/* todo: just pass in a flag to val fpf that tells
		it to shorten the thing for printing purposes */
		if ((val.tag == TAG_STR) || (val.tag == TAG_NUM) || (val.tag == TAG_INT)) {
			elf_fpf_value(io,val,1);
		}
	}
	fprintf(io,"\n");
}


void elf_get_line_location_info(char *q, char *p, int *linenum, char **lineloc);


void lang_dumpmodule(elModule *md, elHandle io) {
#if 0
	fprintf(file,"elModule:\n");
	fprintf(file,"Globals:\n");
	FOR_ARRAY(md->g->v) {
		fprintf(file,"%04llX: ", i);
		elf_fpf_value(file,md->g->v[i],1);
		fprintf(file,"\n");
	}
#endif
	fprintf(io,"-- BYTECODE --\n");
	fprintf(io,"- INSTR: %i\n",md->nbytes);
	fprintf(io,"- PID: %i\n",sys_getmypid());
	FOR_ARRAY(i,md->files) {
		elFileProto ff = md->files[i];
		fprintf(io,"- FILE (%s):\n",ff.name->contents);
		fprintf(io,"INDEX INSTRUCTION\n");
		for (elByteId j = 0; j < ff.nbytes; ++j) {
			elBytecode b = md->bytes[ff.bytes+j];
			// int linenum;
			// char *lineloc;
			// elf_get_line_location_info(md->file,md->lines[j],&linenum,&lineloc);
			// fprintf(file,"%-3i:%-3i",linenum,(int)(md->lines[j]-lineloc));
			elf_bytefpf(io,md,i,j,b);
		}
	}
#if 0
	FOR_ARRAY(md->p) {
		elFileProto p = md->p[i];
		fprintf(file,"FUNC: [%i] %i,%i (%i:%i):\n",(int)i,p.bytes,p.nbytes,p.x,p.nlocals);
		for (elByteId j = 0; j < p.nbytes; ++j) {
			elBytecode b = md->bytes[p.bytes+j];
			elf_bytefpf(md,file,j,b);
		}
		fprintf(file,"end\n");
	}
#endif
}