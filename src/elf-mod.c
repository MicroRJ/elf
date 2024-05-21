/*
** See Copyright Notice In elf.h
** elf-mod.c
** Module
*/


/* todo: ensure that we don't have to replace symbols */
elf_globalid elf_getsymbol(elModule *M, elString *name) {
	if (name != 0) {
		return elf_tabtake(M->g,elf_valstr(name));
	} else return elf_varaddi(M->globals->array,1);
}


elf_globalid lang_addglobal(elModule *M, elString *name, elValue v) {
	elf_globalid i = elf_getsymbol(M,name);
	M->globals->array[i] = v;
	return i;
}


elf_globalid lang_addproto(elModule *M, elProto p) {
	elf_globalid i = elf_varaddi(M->p,1);
	M->p[i] = p;
	return i;
}



int elf_valfpf(FILE *file, elValue v, elBool quotes);
void elf_bytefpf(FILE *io, elModule *md, elInteger fid, elf_byteid id, elf_Bytecode b) {

	if (fid != -1) {
		elFileInfo file = md->files[fid];
		int linenum;
		elf_getlinelocinfo(file.lines,md->lines[id],&linenum,0);
		fprintf(io,"%s %04i: \t",file.name,linenum);
	}

	fprintf(io,"%08i %04i\t%s"
	, md->track[id],	id, lang_bytename(b.k));
	if (lang_byteclass(b.k) == BC_CLASS_XYZ) {
		fprintf(io,"(x=%i,y=%i,z=%i)",b.x,b.y,b.z);
	} else
	if (lang_byteclass(b.k) == BC_CLASS_XY) {
		fprintf(io,"(x=%i,y=%i)",b.x,b.y);
	} else {
		fprintf(io,"(x=%lli)",b.i);
	}
	if (b.k == BC_TYPEGUARD) {
		fprintf(io," #%s",tag2s[b.y]);
	} else
	if (b.k == BC_LOADINT) {
		fprintf(io," #%lli",md->ki[b.y]);
	} else
	if (b.k == BC_LOADNUM) {
		fprintf(io," #%f",md->kn[b.y]);
	} else
	if (b.k == BC_LOADGLOBAL) {
		elValue val = md->globals->array[b.y];
		fprintf(io,"  // %s ",tag2s[val.tag]);
		/* todo: just pass in a flag to val fpf that tells
		it to shorten the thing for printing purposes */
		if ((val.tag == TAG_STR) || (val.tag == TAG_NUM) || (val.tag == TAG_INT)) {
			elf_valfpf(io,val,ltrue);
		}
	}
	fprintf(io,"\n");
}


void elf_getlinelocinfo(char *q, char *p, int *linenum, char **lineloc);


void lang_dumpmodule(elModule *md, elHandle io) {
#if 0
	fprintf(file,"elModule:\n");
	fprintf(file,"Globals:\n");
	elf_arrfori(md->g->v) {
		fprintf(file,"%04llX: ", i);
		elf_valfpf(file,md->g->v[i],ltrue);
		fprintf(file,"\n");
	}
#endif
	fprintf(io,"-- BYTECODE --\n");
	fprintf(io,"- INSTR: %i\n",md->nbytes);
	fprintf(io,"- PID: %i\n",sys_getmypid());
	elf_arrfori(md->files) {
		elFileInfo ff = md->files[i];
		fprintf(io,"- FILE (%s):\n",ff.name);
		fprintf(io,"INDEX INSTRUCTION\n");
		for (elf_byteid j = 0; j < ff.nbytes; ++j) {
			elf_Bytecode b = md->bytes[ff.bytes+j];
			// int linenum;
			// char *lineloc;
			// elf_getlinelocinfo(md->file,md->lines[j],&linenum,&lineloc);
			// fprintf(file,"%-3i:%-3i",linenum,(int)(md->lines[j]-lineloc));
			elf_bytefpf(io,md,i,j,b);
		}
	}
#if 0
	elf_arrfori(md->p) {
		elProto p = md->p[i];
		fprintf(file,"FUNC: [%i] %i,%i (%i:%i):\n",(int)i,p.bytes,p.nbytes,p.x,p.nlocals);
		for (elf_byteid j = 0; j < p.nbytes; ++j) {
			elf_Bytecode b = md->bytes[p.bytes+j];
			elf_bytefpf(md,file,j,b);
		}
		fprintf(file,"end\n");
	}
#endif
}