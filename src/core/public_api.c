//
// See Copyright Notice In elf.h
//

#define TOP(S) (S->stack_ptr)


#include "elf.h"
#include "internal_utils.h"
#include "subsystem.h"
#include "logging.c"
#include "internal_types.h"
#include "internal_api.h"
#include "internal_helpers.h"
#include "internal_helpers.c"
#include "elf_compiler.h"




elf_pubapi
bool elf_readfileh(elf_State *inter, elf_Handle file, int size) {
	if (!file) {
		elf_lerror("invalid file handle");
		elf_pushnil(inter);
		return false;
	}

	if (size == -1) {
		size = sys_size_file(file);
	}

	// todo: we need a dedicated object for this!
	elf_String *contents = elf_alloc_string2(inter, size);
	vsetstr(inter->stack_ptr, contents);
	pushstacksafe(inter);

	sys_read_file(file, contents->text, size);

	return true;
}



elf_pubapi
bool elf_readfilen(elf_State *inter, const char *name, int size)
{
	elf_Handle file = sys_open_file(name, SYS_OPEN_READ, SYS_OPEN_EXISTING);

	bool noerr = elf_readfileh(inter, file, size);

	sys_close_file(file);

	return noerr;
}


elf_pubapi
bool elf_readfile(elf_State *inter, int stk, int size) {
	bool noerr = false;

	if (tisstr(elf_gettag(inter, stk))) {

		const char *name = elf_tostr(inter, stk);

		elf_Handle file = sys_open_file(name, SYS_OPEN_READ, SYS_OPEN_EXISTING);

		noerr = elf_readfileh(inter, file, size);

		sys_close_file(file);
	}
	else if (tissys(elf_gettag(inter, stk))) {

		elf_Handle file = elf_tosys(inter, stk);

		noerr = elf_readfileh(inter, file, size);
	}
	else {

		elf_errorf(inter, -1
		, "'%s': 'readfile' expected handle or file name", tag2s[elf_gettag(inter, stk)]);

		elf_pushnil(inter);
	}
	return noerr;
}



/* todo: remove the asexpr thing? */
elf_pubapi
int elf_loadcodefile(elf_State *S, const char *name) {
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
void elf_getmetatab(elf_State *inter, int objstk) {
	elf_Value v = inter->stack[objstk];
	if (!isobj(v)) {
		elf_errorf(inter, -1, "expected object at %i", objstk);
	}
	vsettab(inter->stack_ptr, vgetobj(v)->meta);
	pushstacksafe(inter);
}


elf_pubapi
void elf_setmetatab(elf_State *inter, int objstk, int tabstk) {
	elf_Value obj = inter->stack[objstk];
	elf_Value tab = inter->stack[tabstk];
	if (!isobj(obj)) {
		elf_errorf(inter, -1, "expected object at %i", objstk);
	}
	if (!vistab(tab)) {
		elf_errorf(inter, -1, "expected table at %i", tabstk);
	}
	vgetobj(obj)->meta = vgettab(tab);
}







elf_pubapi
elf_Tag elf_gettag(elf_State *inter, int x) {
	return inter->stack[x].tag;
}


static inline void tagerror(elf_State *inter, int want, int got, int idx) {

}


// for all these to** functions, assert that x doesn't exceed the
// current framebase!
elf_pubapi
elf_Number elf_tonum(elf_State *inter, int x) {
	elf_Value v = inter->stack[x];
	if (isint(v)) return vgetint(v); // cast
	if (isnum(v)) return vgetnum(v);
	tagerror(inter, ELF_TNUMBER, v.tag,x);
	return 0;
}


elf_pubapi
elf_Integer elf_toint(elf_State *inter, int x) {
	elf_Value v = inter->stack[x];
	if (isnum(v)) return vgetnum(v); // cast
	if (isint(v)) return vgetint(v);
	tagerror(inter, ELF_TNUMBER, v.tag,x);
	return 0;
}


elf_pubapi
elf_Handle elf_tosys(elf_State *inter, int x) {
	elf_Value v = inter->stack[x];
	if (issys(v)) return vgetsys(v);
	tagerror(inter, ELF_THANDLE, v.tag, x);
	return 0;
}


elf_pubapi
const char *elf_tostr(elf_State *inter, int x) {
	elf_Value v = inter->stack[x];
	if (visstr(v)) return vgetstr(v)->text;
	if (visnil(v)) return 0;
	tagerror(inter, ELF_TSTRING, v.tag, x);
	return 0;
}

elf_pubapi
elf_State *elf_new() {
	elf_State *inter = calloc(1, sizeof(*inter));
	elf_init_raw(inter);
	return inter;
}





elf_pubapi
void elf_getglobals(elf_State *S) {
	settoptab(S, S->globals);
	pushstacksafe(S);
}


void elf_pushnil(elf_State *S)                  { pushnil(S);    }
void elf_pushint(elf_State *S, elf_Integer   x) { pushint(S, x); }
void elf_pushnum(elf_State *S, elf_Number    x) { pushnum(S, x); }
void elf_pushfun(elf_State *S, elf_Function  x) { pushfun(S, x); }
void elf_pushsys(elf_State *S, elf_Handle    x) { pushsys(S, x); }


void elf_pushtab(elf_State *S) {
	settoptab(S, elf_alloc_table(S));
	pushstacksafe(S);
}

void elf_pushstr(elf_State *S, const char *text) {
	settopstr(S, elf_alloc_string(S, text));
	pushstacksafe(S);
}

void elf_pushstrl(elf_State *S, const char *text, int len) {
	settopstr(S, elf_alloc_string3(S, text, len));
	pushstacksafe(S);
}


elf_pubapi
void elf_setfield(elf_State *inter) {
	elf_Value tab = inter->stack_ptr[-3];
	elf_Value key = inter->stack_ptr[-2];
	elf_Value val = inter->stack_ptr[-1];

	if (tab.tag != ELF_TTABLE) {
		elf_error(inter, NO_BYTE, "Not A Table!");
	}

	elf_raw_table_set(vgettab(tab), key, val);
	inter->stack_ptr -= 2;
}


elf_pubapi
void elf_arrayadd(elf_State *inter) {
	elf_Value tab = inter->stack_ptr[-2];
	elf_Value val = inter->stack_ptr[-1];

	if (tab.tag != ELF_TTABLE) {
		elf_error(inter, NO_BYTE, "Not A Table!");
	}

	elf_raw_array_add(vgettab(tab), val);
	inter->stack_ptr -= 1;
}


elf_pubapi
void elf_arrayget(elf_State *inter) {
	elf_Value tab = inter->stack_ptr[-2];
	elf_Value idx = inter->stack_ptr[-1];

	if (!vistab(tab))  elf_error(inter, NO_BYTE, "Not A Table!");
	if (!isint(idx))  elf_error(inter, NO_BYTE, "Not A Table!");

	elf_Value v = vgettab(tab)->array[vgetint(idx)];
	inter->stack_ptr[-1] = v;
}

