//
// See Copyright Notice In elf.h
//


#include "elf.h"
#include "internal_utils.h"
#include "subsystem.h"
#include "logging.c"
#include "internal_types.h"
#include "internal_api.h"
#include "internal_helpers.h"
#include "elf_compiler.h"


Num elf_loadnum(elf_State *S, int x) { return loadnum(S, x); }
Int elf_loadint(elf_State *S, int x) { return loadint(S, x); }
const char *elf_loadtext(elf_State *S, int x) {
	return loadtext(S, x);
}

void elf_pushnil(elf_State *S)                  { pushnil(S);    }
void elf_pushint(elf_State *S, elf_Integer   x) { pushint(S, x); }
void elf_pushnum(elf_State *S, elf_Number    x) { pushnum(S, x); }
void elf_pushfun(elf_State *S, elf_Function  x) { pushfun(S, x); }
void elf_pushsys(elf_State *S, elf_Handle    x) { pushsys(S, x); }


void elf_pushtab(elf_State *S) {
	pushtable(S);
}

void elf_pushtext(elf_State *S, const char *text) {
	pushtext(S, text);
}

void elf_pushtext2(elf_State *S, const char *text, int len) {
	pushtext2(S, text, len);
}



elf_pubapi
int elf_pushcodefile(elf_State *S, const char *name) {
	ASSERT(name);
	int iproto = elf_makefile(S, name);

	if (iproto >= 0) {
		Proto proto = S->protos[iproto];

		// todo: instead of taking the proto directly, take an index into the
		// proto array... that way we can mark whether a file is still needed.
		elf_Closure *closure = elf_alloc_closure(S, proto);
		pushcls(S, closure);

	}
	return iproto;
}



elf_pubapi
elf_State *elf_new() {
	elf_State *inter = calloc(1, sizeof(*inter));
	elf_init_raw(inter);
	return inter;
}



elf_pubapi
void elf_pullglobals(elf_State *S) {
	settoptab(S, S->globals);
	pushstacksafe(S);
}



void elf_setfield(elf_State *S) {
	V tab = S->stack_ptr[-3];
	V key = S->stack_ptr[-2];
	V val = S->stack_ptr[-1];
	vcheck(S, tab, ELF_TTABLE);

	tableset(vgettab(tab), key, val);
	S->stack_ptr -= 2;
}



void elf_arrayadd(elf_State *S) {
	V tab = S->stack_ptr[-2];
	V val = S->stack_ptr[-1];
	vcheck(S, tab, ELF_TTABLE);

	arrayadd(vgettab(tab), val);
	S->stack_ptr -= 1;
}



void elf_arrayget(elf_State *S) {
	V tab = S->stack_ptr[-2];
	V idx = S->stack_ptr[-1];
	vcheck(S, tab, ELF_TTABLE);
	vcheck(S, idx, ELF_TINTEGER);

	V v = vgettab(tab)->array[vgetint(idx)];
	S->stack_ptr[-1] = v;
}

