//
// See Copyright Notice In elf.h
//

elf_Closure *elf_alloc_closure(elf_State *S, Proto proto) {
	elf_Closure *closure = (elf_Closure *) elf_gc_alloc(S, GC_CLS, sizeof(elf_Closure) + sizeof(elf_Value) * proto.stacksize);
	closure->proto = proto;
	return closure;
}

