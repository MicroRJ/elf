/*
** See Copyright Notice In elf.h
** ljitlib.c
** JIT Experiments Addon Library
*/


#include <src/ljittoy.h>
#include <src/ljittoy.c>



/* -- Clearly, this isn't how you
- do jitting, this is just me playing
- around */
elf_CFunction jit(elf_Module *md, elf_Function fn);
int jitlib_jit(elf_Shell *rt) {
	elf_Value v = elf_parse_file_fs(rt,0);
	elf_CFunction b = jit(rt->md,v.f->fn);
	elf_put_cfunction(rt,b);
	// __debugbreak();
	(void) v;
	return 1;
}


elAPI void jitlib_load(elf_Shell *rt) {
	elf_Module *md = rt->md;
	elf_gset(md,elf_new_string(rt,"jit"),elCFN(jitlib_jit));
}
