//
// See Copyright Notice In elf.h
//

#include "elf.h"
#include "base.h"
#include "platform.h"
#include "core.h"
#include "helpers.h"
#include "compiler.h"

const char *elf_version(void)
{
	return ELF_VERSION;
}

void elf_set_user_data(elf_State *state, void *user_data)
{
	ASSERT(state);
	state->user_data = user_data;
}

void *elf_get_user_data(elf_State *state)
{
	ASSERT(state);
	return state->user_data;
}

void elf_push_nil(elf_State *S)                  { push_value(S, value_nil());    }
void elf_push_int(elf_State *S, elf_Integer   x) { push_value(S, value_from_integer(x)); }
void elf_push_num(elf_State *S, elf_Number    x) { push_value(S, value_from_number(x)); }
void elf_push_fun(elf_State *S, elf_Function  x) { push_value(S, value_from_function(x)); }

elf_Table *elf_push_new_table(elf_State *S) {
	elf_Table *table = elf_new_table_rogue(S);
	push_table(S, table);
	return table;
}

void elf_push_cstr(elf_State *S, const char *text) {
	push_value(S, value_from_atom(elf_atom_from_data(S, text)));
}

void elf_push_str(elf_State *S, const char *text, int len)
{
	push_value(S, value_from_atom(elf_atom_from_data_size(S, text, len)));
}

static elf_StrSlice source_buffer_from_file(elf_State *state, const char *name)
{
	elf_StrSlice source = {};
	elf_PlatformFile file = elf_platform_open_file(name, ELF_PLATFORM_OPEN_READ, ELF_PLATFORM_OPEN_EXISTING);
	if (!ELF_IS_HANDLE_INVALID(file))
	{
		u64 size = elf_platform_file_size(file);
		char *data = elf_arena_push(&state->arena, size + 16);
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

	elf_Closure * closure = elf_gc_alloc(state, ELF_OBJECT_CLOSURE, sizeof(*closure));
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

	elf_Closure * closure = elf_gc_alloc(state, ELF_OBJECT_CLOSURE, sizeof(*closure));
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
