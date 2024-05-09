/*
** See Copyright Notice In elf.h
** elf.c
** Compiles and runs .elf files
** using the core runtime.
*/


#define ELF_KEEPWARNINGS
#include "elf.h"

#include "src/elf-cli.c"

int main(int n, char **c) {
	(void) n;
	cli_t cli = {0};
	if (parsecli(&cli,n,c)) return 0;

	elf_Module M = {0};
	elf_State R = {0};
	elf_runini(&R,&M);
	if (cli.logging) R.bytelogging = ltrue;

	elf_CallFrame frame = {0};
	frame.base = R.top;
	R.frame = &frame;

	if (cli.filename != lnil) {
		elf_String *filename = elf_pushnewstr(&R,cli.filename);
		filename->obj.gccolor = GC_PINK;
		elf_FileState fs = {0};
		elf_loadfilefs(&R,&fs,filename,0,0);
	}
	if (cli.dump) {
		FILE *dumpf = stdout;
		if (strcmp(cli.dumpfilename,"stdout")) {
			dumpf = fopen(elf_tpf("%s.module.ignore",cli.dumpfilename),"wb");
		}
		if (dumpf == lnil) {
			printf("error: could open specified dump file for writting");
		} else {
			lang_dumpmodule(&M,dumpf);
			if (dumpf != stdout) fclose(dumpf);
		}
	}
	sys_consolelog(ELF_LOGINFO,"exited");
	return 0;
}

