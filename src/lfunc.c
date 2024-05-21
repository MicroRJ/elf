/*
** See Copyright Notice In elf.h
** lfunc.c
** ?
*/


elf_api elf_Closure *elf_newcls(elState *rt, elProto fn) {
	elf_ensure(fn.ncaches >= 0);

	elInteger length = sizeof(elf_Closure) + sizeof(elValue) * (fn.ncaches-1);

	elf_Closure *cl = elf_newobj(rt,OBJ_CLOSURE,length);
	cl->fn = fn;
	return cl;
}
