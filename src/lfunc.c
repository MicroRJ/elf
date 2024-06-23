/*
** See Copyright Notice In elf.h
** lfunc.c
** ?
*/


elf_api elClosure *elf_new_closure(elState *rt, elProto fn) {
	elf_ensure(fn.zcache >= 0);

	elInteger length = sizeof(elClosure) + sizeof(elValue) * (fn.zcache-1);

	elClosure *cl = elf_allocate_new_object(rt,OBJ_CLOSURE,length);
	cl->fn = fn;
	return cl;
}
