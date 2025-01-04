/*
** See Copyright Notice In elf.h
** elf-web.c
*/

#define ELF_KEEPWARNINGS
#include "elf.c"


#if 0
int main(int n, char **c) {
	sys_console_print(LOG_KDEBUG,"WEB!");

	#if defined(_DEBUG)
	sys_console_print(LOG_KDEBUG,"COMPILER CHECK:");
	int *var = {0};
	ARRAY_ADD(var,1);
	if (var[0] != 1) sys_console_print(LOG_KERROR,"FAILED: var.add!\n");
	if (ARRAY_LENGTH(var) != 1) sys_console_print(LOG_KERROR,"FAILED: 'var.len!\n");
	int arr[1] = {1};
	if (arr[0] != 1) sys_console_print(LOG_KERROR,"FAILED: arr!\n");
	#endif
}
#endif


struct {
	elf_State R;
	BC_Module M;
	elf_StackFrame C;
} GLOBAL elf = {{&elf.M}};


elAPI void elf_global_initialize() {
	elf_init(&elf.R,&elf.M);
}

#if 0
elAPI int elf_global_loadcode(char *filename, char *contents) {
	elf_Value *top = GET_TOP(&elf.R);
	elf_String *name = elf_new_string(&elf.R,filename);
	elf_String *string = elf_new_string(&elf.R,contents);
	Parser fs = {0};
	int nyield = elf_load_code_closure(&elf.R,&fs,name,0,string);
	SET_TOP(&elf.R,top);
	return nyield;
}
#endif
