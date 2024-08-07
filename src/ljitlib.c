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
elCFunction jit(elModule *md, elFileProto fn);
int jitlib_jit(elState *rt) {
	elValue v = elf_parse_file_fs(rt,0);
	elCFunction b = jit(rt->md,v.f->fn);
	elf_pushbinding(rt,b);
	// __debugbreak();
	(void) v;
	return 1;
}


elAPI void jitlib_load(elState *rt) {
	elModule *md = rt->md;
	elf_add_global_value(md,elf_add_new_string(rt,"jit"),elCFN(jitlib_jit));
}
