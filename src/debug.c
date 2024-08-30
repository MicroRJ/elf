/*
** See Copyright Notice In elf.h
** ldebug.c
** Debug Tools
*/


static int (*global_assertion_hook)(DBGSource);


void set_assertion_hook(int (*hook)(DBGSource)) {
	global_assertion_hook = hook;
}


void assertion_function(DBGSource ind, char const *message) {
	printf("%s[%i] %s(): '%s' triggered assertion\n",ind.fileName,ind.lineNumber,ind.func,message);
	if (global_assertion_hook != 0) {
		global_assertion_hook(ind);
	} else elf_debugger("assertion triggered");
}
