//
// See Copyright Notice In elf.h
//


#include "elf.h"
#include "internal_utils.h"
#include "subsystem.h"
#include "logging.c"
#include "internal_types.h"
#include "internal_helpers.h"
#include "elf_compiler.h"




void elf_error(elf_State *S, int error, const char *format, ...) {
	va_list vargs;
	va_start(vargs, format);
	char *message = tempvpf(format, vargs);
	va_end(vargs);
	reporterror(S, -1, message);
}



Tag elf_loadtype(elf_State *S, int x) { return loadtype(S, x); }
Num elf_load_num(elf_State *S, int x) { return loadnum(S, x); }
Int elf_loadint(elf_State *S, int x) { return loadint(S, x); }
Handle elf_loadsys(elf_State *S, int x) { return loadsys(S, x); }



const char *elf_loadtext(elf_State *S, int x) {
	return loadtext(S, x);
}



const char *elf_loadtextl(elf_State *S, int x, int *l) {
	Str str = loadstr(S, x);
	if (l) *l = str->length;
	return str->text;
}



void elf_push_nil(elf_State *S)                  { pushnil(S);    }
void elf_pushint(elf_State *S, elf_Integer   x) { pushint(S, x); }
void elf_push_num(elf_State *S, elf_Number    x) { pushnum(S, x); }
void elf_pushfun(elf_State *S, elf_Function  x) { pushfun(S, x); }
void elf_pushsys(elf_State *S, elf_Handle    x) { pushsys(S, x); }



void elf_pushtab(elf_State *S) {
	pushnewtable(S);
}




void elf_pushtext(elf_State *S, const char *text) {
	pushtext(S, text);
}






void elf_push_textl(elf_State *S, const char *text, int len) {
	pushtext2(S, text, len);
}





int elf_pushcodefile(elf_State *S, const char *name, const char *text) {
	// todo: why is make_file doing file io
	ASSERT(name);
	int proto_index = elf_makefile(S, name);

	if (proto_index >= 0) {
		Closure closure = gcalloc(S, GC_CLS, sizeof(*closure));
		closure->proto_index = proto_index;
		closure->proto = S->protos[proto_index];
		pushcls(S, closure);
	}
	else {
		pushnil(S);
	}

	return proto_index >= 0;
}






elf_pubapi
elf_State *elf_new() {
	elf_State *inter = calloc(1, sizeof(*inter));
	_initstate(inter);
	return inter;
}



elf_pubapi
void elf_pushglobals(elf_State *S) {
	pushtab(S, S->globals);
}



void elf_setfield(elf_State *S) {
	V tab = S->stack_ptr[-3];
	V key = S->stack_ptr[-2];
	V val = S->stack_ptr[-1];
	typecheck(S, tab, ELF_TTABLE);

	tableset(S, as_table(tab), key, val);
	S->stack_ptr -= 2;
}



void elf_arrayadd(elf_State *S) {
	V tab = S->stack_ptr[-2];
	V val = S->stack_ptr[-1];
	typecheck(S, tab, ELF_TTABLE);

	_table_arrayadd(S, as_table(tab), val);
	S->stack_ptr -= 1;
}



void elf_arrayget(elf_State *S) {
	V tab = S->stack_ptr[-2];
	V idx = S->stack_ptr[-1];
	typecheck(S, tab, ELF_TTABLE);
	typecheck(S, idx, ELF_TINTEGER);

	V v = as_table(tab)->array[as_int(idx)];
	S->stack_ptr[-1] = v;
}

