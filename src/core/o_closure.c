//
// See Copyright Notice In elf.h
//


#include "o_closure.h"


elf_Closure *elf_alloc_closure(elf_State *S, elf_Proto proto) {
	elf_Closure *closure = (elf_Closure *) elf_gc_alloc(S, GC_CLS, sizeof(elf_Closure) + sizeof(elf_Value) * proto.stacksize);
	closure->proto = proto;
	return closure;
}

elf_Closure *elf_new_closure(elf_State *R, elf_Proto proto) {
	elf_Closure *closure = elf_alloc_closure(R, proto);
	elf_push_closure(R, closure);
	return closure;
}