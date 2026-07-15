//
// See Copyright Notice In elf.h
//

#include <stdarg.h>
#include <string.h>

#include "elf.h"
#include "base.h"
#include "system.h"
#include "core.h"
#include "helpers.h"
#include "compiler.h"


void elf_error(elf_State *S, int error, const char *format, ...) {
	va_list vargs;
	va_start(vargs, format);
	Scratch scratch = get_scratch();
	char *message = arena_pushfv(scratch.arena, format, vargs);
	va_end(vargs);
	arena_push_zero(scratch.arena, 1);
	report_runtime_error(S, RUNTIME_ERROR_GENERIC, -1, "%s", message);
	end_scratch(scratch);
}



ELF_ValueType elf_loadtype(elf_State *S, int x) { return value_type(load_value(S, x)); }

elf_Number elf_load_num(elf_State *S, int x)
{
	elf_Value value = load_value(S, x);
	check_value_type_rule(S, value, TRULE_NUMERIC);
	return value_to_number(value);
}

elf_Integer elf_loadint(elf_State *S, int x)
{
	elf_Value value = load_value(S, x);
	check_value_type_rule(S, value, TRULE_NUMERIC);
	return value_to_integer(value);
}

elf_Handle elf_loadsys(elf_State *S, int x)
{
	elf_Value value = load_value(S, x);
	check_value_type(S, value, ELF_VALUE_TYPE_HANDLE);
	return value_as_handle(value);
}



elf_StrSlice elf_load_atom_copy(elf_State *S, int x, elf_Arena *arena)
{
	elf_Value value = load_value(S, x);
	check_value_type(S, value, ELF_VALUE_TYPE_ATOM);
	return elf_atom_copy_text(arena, value_as_atom(value));
}



void elf_push_nil(elf_State *S)                  { push_value(S, value_nil());    }
void elf_pushint(elf_State *S, elf_Integer   x) { push_value(S, value_from_integer(x)); }
void elf_push_num(elf_State *S, elf_Number    x) { push_value(S, value_from_number(x)); }
void elf_pushfun(elf_State *S, elf_Function  x) { push_value(S, value_from_function(x)); }
void elf_pushsys(elf_State *S, elf_Handle    x) { push_value(S, value_from_handle(x)); }



elf_Table *elf_push_new_table(elf_State *S) {
	elf_Table *table = elf_table_new_unrooted(S);
	push_table(S, table);
	return table;
}




void elf_push_atom_text(elf_State *S, const char *text) {
	push_value(S, value_from_atom(elf_atom_from_data(S, text)));
}

void elf_push_atom_text_size(elf_State *S, const char *text, int len)
{
	push_value(S, value_from_atom(elf_atom_from_data_size(S, text, len)));
}

static elf_StrSlice source_buffer_from_file(elf_State *state, const char *name)
{
	elf_StrSlice source = {};
	elf_Handle file = elf_platform_access_file(name, SYS_OPEN_READ, SYS_OPEN_EXISTING);
	if (!ELF_IS_HANDLE_INVALID(file))
	{
		u64 size = elf_platform_get_file_size(file);
		char *data = arena_push(&state->arena, size + 16);
		zero_memory(data + size, 16);
		elf_platform_read_file(file, data, (u32)size);
		elf_platform_close_file(file);

		source.data = data;
		source.size = size;
	}
	return source;
}

int elf_push_code_source(elf_State *state, const char *name, elf_StrSlice source)
{
	ASSERT(name);
	ASSERT(source.data);
	BytecodeFunction function = elf_compile_source(state, name, source);

	elf_Closure * closure = gc_alloc(state, ELF_OBJECT_CLOSURE, sizeof(*closure));
	closure->function = function;
	push_value(state, value_from_closure(closure));

	return true;
}

int elf_push_code_file(elf_State *state, const char *name)
{
	ASSERT(name);

	elf_StrSlice source = source_buffer_from_file(state, name);
	if (!source.data)
	{
		push_value(state, value_nil());
		return false;
	}

	BytecodeFunction function = elf_compile_source(state, name, source);

	elf_Closure * closure = gc_alloc(state, ELF_OBJECT_CLOSURE, sizeof(*closure));
	closure->function = function;
	push_value(state, value_from_closure(closure));

	return true;
}

int elf_push_constant_expr(elf_State *state, const char *name, elf_StrSlice source)
{
	ASSERT(name);
	ASSERT(source.data);
	return elf_push_constant_expr_source(state, name, source);
}

int elf_push_json(elf_State *state, const char *name, elf_StrSlice source)
{
	ASSERT(name);
	ASSERT(source.data);
	return elf_push_json_source(state, name, source);
}

void elf_pushglobals(elf_State *S) {
	push_table(S, S->globals);
}

// Todo, remove this!
void elf_setfield(elf_State *S) {
	elf_Value tab = S->stack_ptr[-3];
	elf_Value key = S->stack_ptr[-2];
	elf_Value value = S->stack_ptr[-1];
	check_value_type(S, tab, ELF_VALUE_TYPE_TABLE);

	elf_table_set(S, value_as_table(tab), key, value);
	S->stack_ptr -= 2;
}

// Todo, remove this!
void elf_arrayadd(elf_State *S) {
	elf_Value tab = S->stack_ptr[-2];
	elf_Value value = S->stack_ptr[-1];
	check_value_type(S, tab, ELF_VALUE_TYPE_TABLE);

	elf_array_add(S, value_as_table(tab), value);
	S->stack_ptr -= 1;
}

// Todo, remove this!
void elf_arrayget(elf_State *S) {
	elf_Value tab = S->stack_ptr[-2];
	elf_Value idx = S->stack_ptr[-1];
	check_value_type(S, tab, ELF_VALUE_TYPE_TABLE);
	check_value_type(S, idx, ELF_VALUE_TYPE_INTEGER);

	elf_Value value = elf_array_get(S, value_as_table(tab), value_as_integer(idx));
	S->stack_ptr[-1] = value;
}
