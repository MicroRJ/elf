/*
** See Copyright Notice In elf.h
** closure.c
*/


elf_Closure *elf_alloc_closure(elf_State *S, elf_protoT proto) {
	elf_Closure *cls = (elf_Closure *) elf_alloc_object(S,GC_CLS,sizeof(elf_Closure) + sizeof(elf_Value) * (proto.nlocals-1));
	cls->proto = proto;
	return cls;
}