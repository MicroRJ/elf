//
// See Copyright Notice In elf.h
//


#include "elf.h"
#include "internal_utils.h"
#include "subsystem.h"
#include "logging.c"
#include "internal_types.h"
#include "internal_helpers.h"
#include "compiler.h"
#include "arena.c"




void elf_error(elf_State *S, int error, const char *format, ...) {
	va_list vargs;
	va_start(vargs, format);
	char *message = temporary_format_v(format, vargs);
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
	GCStr str = loadstr(S, x);
	if (l) *l = str->length;
	return str->text;
}



void elf_push_nil(elf_State *S)                  { push_nil(S);    }
void elf_pushint(elf_State *S, elf_Integer   x) { pushint(S, x); }
void elf_push_num(elf_State *S, elf_Number    x) { pushnum(S, x); }
void elf_pushfun(elf_State *S, elf_Function  x) { pushfun(S, x); }
void elf_pushsys(elf_State *S, elf_Handle    x) { pushsys(S, x); }



void elf_pushtab(elf_State *S) {
	push_new_table(S);
}




void elf_pushtext(elf_State *S, const char *text) {
	pushtext(S, text);
}

void elf_push_textl(elf_State *S, const char *text, int len)
{
	pushtext2(S, text, len);
}

// Todo, we need to remove file io from here entirely ...
int elf_pushcodefile(elf_State *state, const char *name, const char *text)
{
	ASSERT(name);

	BytecodeFunction function = elf_makefile(state, name);

	Closure closure = collector_alloc(state, GC_CLOSURE, sizeof(*closure));
	closure->function = function;
	push_closure(state, closure);

	return 0;
}

// Todo, remove this!
ELF_PUBLIC
void elf_pushglobals(elf_State *S) {
	pushtab(S, S->globals);
}

// Todo, remove this!
void elf_setfield(elf_State *S) {
	V tab = S->stack_ptr[-3];
	V key = S->stack_ptr[-2];
	V val = S->stack_ptr[-1];
	typecheck(S, tab, ELF_VALUE_TYPE_TABLE);

	tableset(S, table_from_value(tab), key, val);
	S->stack_ptr -= 2;
}

// Todo, remove this!
void elf_arrayadd(elf_State *S) {
	V tab = S->stack_ptr[-2];
	V val = S->stack_ptr[-1];
	typecheck(S, tab, ELF_VALUE_TYPE_TABLE);

	_table_arrayadd(S, table_from_value(tab), val);
	S->stack_ptr -= 1;
}

// Todo, remove this!
void elf_arrayget(elf_State *S) {
	V tab = S->stack_ptr[-2];
	V idx = S->stack_ptr[-1];
	typecheck(S, tab, ELF_VALUE_TYPE_TABLE);
	typecheck(S, idx, ELF_VALUE_TYPE_INTEGER);

	V v = table_from_value(tab)->array[as_int(idx)];
	S->stack_ptr[-1] = v;
}

