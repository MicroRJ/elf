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


elf_ValueType elf_arg_type(elf_State *S, int x) { return value_type(load_value(S, x)); }

elf_Number elf_arg_num(elf_State *S, int x)
{
	elf_Value value = load_value(S, x);
	check_value_type_rule(S, value, TRULE_NUMERIC);
	return value_to_number(value);
}

elf_Integer elf_arg_int(elf_State *S, int x)
{
	elf_Value value = load_value(S, x);
	check_value_type_rule(S, value, TRULE_NUMERIC);
	return value_to_integer(value);
}

elf_Handle elf_arg_hnd(elf_State *S, int x)
{
	elf_Value value = load_value(S, x);
	check_value_type(S, value, ELF_VALUE_TYPE_HANDLE);
	return value_as_handle(value);
}

elf_String *elf_arg_str(elf_State *S, int x)
{
	elf_Value value = load_value(S, x);
	check_value_type(S, value, ELF_VALUE_TYPE_ATOM);
	return value_as_atom(value);
}

void elf_push_nil(elf_State *S)                  { push_value(S, value_nil());    }
void elf_push_int(elf_State *S, elf_Integer   x) { push_value(S, value_from_integer(x)); }
void elf_push_num(elf_State *S, elf_Number    x) { push_value(S, value_from_number(x)); }
void elf_push_fun(elf_State *S, elf_Function  x) { push_value(S, value_from_function(x)); }
void elf_push_hnd(elf_State *S, elf_Handle    x) { push_value(S, value_from_handle(x)); }

elf_Table *elf_push_new_table(elf_State *S) {
	elf_Table *table = elf_table_new_unrooted(S);
	push_table(S, table);
	return table;
}

elf_Table *elf_retain_table(elf_Table *table)
{
	if (!table) return 0;
	ASSERT(table->obj.type == ELF_OBJECT_TABLE);
	ASSERT(table->obj.external_refs != (u8)-1);
	table->obj.external_refs += 1;
	return table;
}

void elf_release_table(elf_Table *table)
{
	if (!table) return;
	ASSERT(table->obj.type == ELF_OBJECT_TABLE);
	ASSERT(table->obj.external_refs > 0);
	table->obj.external_refs -= 1;
}

elf_String *elf_retain_str(elf_String *string)
{
	if (!string) return 0;
	ASSERT(string->obj.type == ELF_OBJECT_ATOM);
	ASSERT(string->obj.external_refs != (u8)-1);
	string->obj.external_refs += 1;
	return string;
}

void elf_release_str(elf_String *string)
{
	if (!string) return;
	ASSERT(string->obj.type == ELF_OBJECT_ATOM);
	ASSERT(string->obj.external_refs > 0);
	string->obj.external_refs -= 1;
}

const char *elf_str_data(const elf_String *string)
{
	ASSERT(string);
	return string->data;
}

elf_u32 elf_str_size(const elf_String *string)
{
	ASSERT(string);
	return string->size;
}




void elf_push_cstr(elf_State *S, const char *text) {
	push_value(S, value_from_atom(elf_atom_from_data(S, text)));
}

void elf_push_str(elf_State *S, const char *text, int len)
{
	push_value(S, value_from_atom(elf_atom_from_data_size(S, text, len)));
}

static elf_ValueView value_view(elf_Value value)
{
	elf_ValueView view = {};
	view.type = value_type(value);
	switch (view.type)
	{
		case ELF_VALUE_TYPE_INTEGER: view.as.integer = value_as_integer(value); break;
		case ELF_VALUE_TYPE_NUMBER:  view.as.number = value_as_number(value); break;
		case ELF_VALUE_TYPE_HANDLE:  view.as.handle = value_as_handle(value); break;
		case ELF_VALUE_TYPE_TABLE:   view.as.table = value_as_table(value); break;
		case ELF_VALUE_TYPE_ATOM:
			view.as.string = value_as_atom(value);
			break;
		default: break;
	}
	return view;
}

elf_ValueView elf_peek_value(elf_State *state, elf_u32 depth)
{
	ASSERT(state);
	ASSERT((elf_u64)(state->stack_ptr - state->stack) > depth);
	return value_view(state->stack_ptr[-1 - (i32)depth]);
}

void elf_pop_values(elf_State *state, elf_u32 count)
{
	ASSERT(state);
	ASSERT((elf_u64)(state->stack_ptr - state->stack) >= count);
	state->stack_ptr -= count;
}

elf_u32 elf_table_length(const elf_Table *table)
{
	ASSERT(table);
	return table->count;
}

elf_ValueView elf_get_field(elf_State *state, elf_Table *table,
	const char *field)
{
	ASSERT(state);
	ASSERT(table);
	ASSERT(field);
	elf_Value key = value_from_atom(elf_atom_from_data(state, field));
	return value_view(elf_table_get_or_nil(state, table, key));
}

elf_ValueView elf_get_index(elf_State *state, elf_Table *table,
	elf_u32 index)
{
	ASSERT(state);
	ASSERT(table);
	return value_view(elf_array_get(state, table, index));
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
	BcFunction function = elf_compile_source(state, name, source);

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

	BcFunction function = elf_compile_source(state, name, source);

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

void elf_push_env(elf_State *S) {
	push_table(S, S->globals);
}

// Todo, remove this!
void elf_tab_set(elf_State *S) {
	elf_Value tab = S->stack_ptr[-3];
	elf_Value key = S->stack_ptr[-2];
	elf_Value value = S->stack_ptr[-1];
	check_value_type(S, tab, ELF_VALUE_TYPE_TABLE);

	elf_table_set(S, value_as_table(tab), key, value);
	S->stack_ptr -= 2;
}

// Todo, remove this!
void elf_arr_add(elf_State *S) {
	elf_Value tab = S->stack_ptr[-2];
	elf_Value value = S->stack_ptr[-1];
	check_value_type(S, tab, ELF_VALUE_TYPE_TABLE);

	elf_array_add(S, value_as_table(tab), value);
	S->stack_ptr -= 1;
}

// Todo, remove this!
void elf_arr_get(elf_State *S) {
	elf_Value tab = S->stack_ptr[-2];
	elf_Value idx = S->stack_ptr[-1];
	check_value_type(S, tab, ELF_VALUE_TYPE_TABLE);
	check_value_type(S, idx, ELF_VALUE_TYPE_INTEGER);

	elf_Value value = elf_array_get(S, value_as_table(tab), value_as_integer(idx));
	S->stack_ptr[-1] = value;
}
