/*
** See Copyright Notice In elf.h
** lfunc.c
** ?
*/


elf_api elClosure *elf_newcls(elState *rt, elProto fn) {
	elf_ensure(fn.ncaches >= 0);

	elInteger length = sizeof(elClosure) + sizeof(elValue) * (fn.ncaches-1);

	elClosure *cl = elf_newobj(rt,OBJ_CLOSURE,length);
	cl->fn = fn;
	return cl;
}
