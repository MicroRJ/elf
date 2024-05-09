/*
** See Copyright Notice In elf.h
** ljitlib.c
** JIT Experiments Addon Library
*/


/* this inclusion is temporary */
#include <src/ljittoy.h>
#include <src/ljittoy.c>



/* -- Clearly, this isn't how you
- do jitting, this is just me playing
- around */
lBinding jit(elf_Module *md, elf_Proto fn);
int jitlib_jit(elf_State *rt) {
	elf_Value v = elf_loadfilefs(rt,0);
	lBinding b = jit(rt->md,v.f->fn);
	elf_pushbinding(rt,b);
	// __debugbreak();
	(void) v;
	return 1;
}


elf_api void jitlib_load(elf_State *rt) {
	elf_Module *md = rt->md;
	lang_addglobal(md,elf_pushnewstr(rt,"jit"),elf_valbid(jitlib_jit));
}
