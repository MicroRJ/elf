/*
** See Copyright Notice In elf.h
** elf.c
** this is the builtin loader
** that comes elf, and it provides
** the core runtime library
*/


#define ELF_KEEPWARNINGS
#include "elf.h"

#include "src/elf-cli.c"

int main(int n, char **c) {
	(void) n;
	elf_cliopts cli = {0};
	if (elf_loadcliopts(&cli,n,c)) return 0;

	elModule M = {0};
	elState R = {0};
	elf_runini(&R,&M);
	if (cli.logging) R.bytelogging = ltrue;

	elf_CallFrame frame = {0};
	frame.base = R.top;
	R.frame = &frame;

	if (cli.filename != elNIL) {
		elString *filename = elf_pushnewstr(&R,cli.filename);
		filename->obj.gccolor = GC_PINK;
		elFileState fs = {0};
		elf_loadfilefs(&R,&fs,filename,0,0);
	}
	if (cli.dump) {
		FILE *dumpf = stdout;
		if (strcmp(cli.dumpfilename,"stdout")) {
			dumpf = fopen(elf_tpf("%s.module.ignore",cli.dumpfilename),"wb");
		}
		if (dumpf == elNIL) {
			printf("error: could open specified dump file for writting");
		} else {
			lang_dumpmodule(&M,dumpf);
			if (dumpf != stdout) fclose(dumpf);
		}
	}
	sys_consolelog(ELF_LOGINFO,"exited");
	return 0;
}

