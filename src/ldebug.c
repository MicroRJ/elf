/*
** See Copyright Notice In elf.h
** ldebug.c
** Debug Tools
*/


elGLOBAL int (*lang_globalassertionhook)(elSourceInfo);


void lang_setasserthook(int (*hook)(elSourceInfo)) {
	lang_globalassertionhook = hook;
}


void lang_assertfn(elSourceInfo ind, char const *name, elBool expr) {
	if (!expr) {
		printf("%s[%i] %s(): '%s' triggered assertion\n",ind.fileName,ind.lineNumber,ind.func,name);
		if (lang_globalassertionhook != 0) {
			lang_globalassertionhook(ind);
		} else elf_debugger("assertion triggered");
	}
}
