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
	ARRAY_ADD(var,1);
	if (var[0] != 1) sys_consolelog(ELF_LOGERROR,"FAILED: var.add!\n");
	if (ARRAY_LENGTH(var) != 1) sys_consolelog(ELF_LOGERROR,"FAILED: 'var.len!\n");
	int arr[1] = {1};
	if (arr[0] != 1) sys_consolelog(ELF_LOGERROR,"FAILED: arr!\n");
	#endif
}
#endif


struct {
	elState R;
	elModule M;
	elStackFrame C;
} elGLOBAL elf = {{&elf.M}};


elAPI void elf_global_initialize() {
	elf_begin(&elf.R,&elf.M);
}


elAPI int elf_global_loadcode(char *filename, char *contents) {
	elValue *top = elf.R.top;
	elString *name = elf_add_new_string(&elf.R,filename);
	elString *string = elf_add_new_string(&elf.R,contents);
	elFileState fs = {0};
	int nyield = elf_load_code_fs(&elf.R,&fs,name,0,0,string);
	elf.R.top = top;
	return nyield;
}
