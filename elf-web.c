/*
** See Copyright Notice In elf.h
** elf-web.c
** Same idea as elf.c, but tailored
** for the web, this is only an
** example.
*/

#define ELF_KEEPWARNINGS
#include "elf.h"


#if !defined(ELF_NOMAIN)
int main(int n, char **c) {
	sys_consolelog(ELF_LOGDBUG,"WEB!");

	#if defined(_DEBUG)
	sys_consolelog(ELF_LOGDBUG,"COMPILER CHECK:");
	int *var = {0};
	elf_varadd(var,1);
	if (var[0] != 1) sys_consolelog(ELF_LOGERROR,"FAILED: var.add!\n");
	if (elf_varlen(var) != 1) sys_consolelog(ELF_LOGERROR,"FAILED: 'var.len!\n");
	int arr[1] = {1};
	if (arr[0] != 1) sys_consolelog(ELF_LOGERROR,"FAILED: arr!\n");
	#endif
}
#endif


struct {
	elState R;
	elModule M;
	elf_CallFrame C;
} elf_globaldecl elf = {{&elf.M}};


elf_api void elfweb_ini() {
	elf_runini(&elf.R,&elf.M);
}


/* todo: copy the contents */
elf_api int elfweb_loadcode(char *codename, char *contents) {
	/* run in a separate call frame, this isn't needed
	though... */
	elf_CallFrame call = {0};
	call.caller = elf.R.call;
	call.base = elf.R.top;
	call.top = elf.R.top;
	elf.R.call = &call;
	elString *name = elf_pushnewstr(&elf.R,codename);
	elFileState fs = {0};
	int result = elf_loadcodefs(&elf.R,&fs,name,0,0,contents);
	elf.R.call = call.caller;
	return result;
}
