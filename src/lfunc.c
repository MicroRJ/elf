/*
** See Copyright Notice In elf.h
** lfunc.c
** ?
*/


elAPI elClosure *elf_new_closure(elState *S, elProto fn) {
	elf_ensure(fn.zcache >= 0);

	elInteger length = sizeof(elClosure) + sizeof(elValue) * (fn.zcache-1);

	elClosure *cl = elf_new_object(S,OBJ_CLOSURE,length);
	cl->fn = fn;
	return cl;
}
