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

void elf_push_nil(elf_State *state)                 { push_value(state, value_nil()); }
void elf_push_int(elf_State *state, elf_Integer x)  { push_value(state, value_from_integer(x)); }
void elf_push_num(elf_State *state, elf_Number x)   { push_value(state, value_from_number(x)); }
void elf_push_fun(elf_State *state, elf_Function x) { push_value(state, value_from_function(x)); }

void elf_push_cstr(elf_State *state, const char *text)
{
	push_value(state, value_from_atom(elf_atom_from_data(state, text)));
}

void elf_push_str(elf_State *state, const char *text, int length)
{
	push_value(state, value_from_atom(elf_atom_from_data_size(state, text, length)));
}

int elf_push_code_source(elf_State *state, const char *name, elf_StrSlice source)
{
	ASSERT(name);
	ASSERT(source.data);
	BcFunction function = elf_compile_source(state, name, source);

	elf_Closure *closure = elf_gc_alloc(state, ELF_OBJECT_CLOSURE, sizeof(*closure));
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

static elf_i32 frame_floor(elf_State *state)
{
	return state->frame.framesize;
}

static elf_Value *value_at(elf_State *state, elf_i32 index)
{
	elf_i32 top = elf_get_top(state);
	elf_i32 absolute = index < 0 ? top + index : index;
	if (absolute < 0 || absolute >= top) return 0;
	return state->frame.framebase + absolute;
}

static elf_b32 table_at(elf_State *state, elf_i32 index, elf_Table **table)
{
	elf_Value *value = value_at(state, index);
	if (!value || !value_is_table(*value)) return false;
	*table = value_as_table(*value);
	return true;
}

static elf_b32 values_equal(elf_Value left, elf_Value right)
{
	if (value_is_numeric(left) && value_is_numeric(right)) {
		return value_to_number(left) == value_to_number(right);
	}
	if (value_type(left) != value_type(right)) return false;
	if (value_is_atom(left)) return atoms_equal(value_as_atom(left), value_as_atom(right));
	return left.x_i64 == right.x_i64;
}

elf_i32 elf_get_top(elf_State *state)
{
	return (elf_i32)(state->stack_ptr - state->frame.framebase);
}

elf_i32 elf_arg_count(elf_State *state)
{
	return state->frame.nargs;
}

elf_i32 elf_abs_index(elf_State *state, elf_i32 index)
{
	elf_i32 absolute = index < 0 ? elf_get_top(state) + index : index;
	return elf_is_valid(state, absolute) ? absolute : -1;
}

elf_b32 elf_is_valid(elf_State *state, elf_i32 index)
{
	return value_at(state, index) != 0;
}

elf_b32 elf_set_top(elf_State *state, elf_i32 top)
{
	elf_i32 current = elf_get_top(state);
	if (top < frame_floor(state) || state->frame.framebase + top > state->stack + state->stack_size) {
		return false;
	}
	while (current < top) {
		push_value(state, value_nil());
		current += 1;
	}
	if (current > top) {
		value_zero_many(state->frame.framebase + top, current - top);
		state->stack_ptr = state->frame.framebase + top;
	}
	return true;
}

elf_b32 elf_pop(elf_State *state, elf_u32 count)
{
	elf_i32 top = elf_get_top(state);
	if (count > (elf_u32)(top - frame_floor(state))) return false;
	return elf_set_top(state, top - (elf_i32)count);
}

elf_b32 elf_push_value(elf_State *state, elf_i32 index)
{
	elf_Value *value = value_at(state, index);
	if (!value) return false;
	push_value(state, *value);
	return true;
}

elf_ValueType elf_type(elf_State *state, elf_i32 index)
{
	elf_Value *value = value_at(state, index);
	return value ? value_type(*value) : ELF_VALUE_TYPE_COUNT_;
}

elf_b32 elf_is_nil(elf_State *state, elf_i32 index)
{
	elf_Value *value = value_at(state, index);
	return value && value_is_nil(*value);
}

elf_b32 elf_is_numeric(elf_State *state, elf_i32 index)
{
	elf_Value *value = value_at(state, index);
	return value && value_is_numeric(*value);
}

elf_b32 elf_is_callable(elf_State *state, elf_i32 index)
{
	elf_Value *value = value_at(state, index);
	return value && value_is_callable(*value);
}

elf_b32 elf_to_int(elf_State *state, elf_i32 index, elf_Integer *result)
{
	elf_Value *value = value_at(state, index);
	if (!value || !value_is_integer(*value) || !result) return false;
	*result = value_as_integer(*value);
	return true;
}

elf_b32 elf_to_num(elf_State *state, elf_i32 index, elf_Number *result)
{
	elf_Value *value = value_at(state, index);
	if (!value || !value_is_numeric(*value) || !result) return false;
	*result = value_to_number(*value);
	return true;
}

elf_b32 elf_to_str(elf_State *state, elf_i32 index, elf_StrSlice *result)
{
	elf_Value *value = value_at(state, index);
	if (!value || !value_is_atom(*value) || !result) return false;
	elf_String *string = value_as_atom(*value);
	result->data = string->data;
	result->size = string->size;
	return true;
}

void elf_new_table(elf_State *state)
{
	elf_push_new_table(state);
}

elf_b32 elf_length(elf_State *state, elf_i32 index, elf_u32 *length)
{
	elf_Table *table;
	if (!length || !table_at(state, index, &table)) return false;
	*length = elf_array_length(table);
	return true;
}

elf_b32 elf_get_field(elf_State *state, elf_i32 index, const char *field)
{
	elf_Table *table;
	if (!field || !table_at(state, index, &table)) return false;
	elf_Value key = value_from_atom(elf_atom_from_data(state, field));
	push_value(state, elf_table_get_or_nil(state, table, key));
	return true;
}

elf_b32 elf_set_field(elf_State *state, elf_i32 index, const char *field)
{
	elf_Table *table;
	if (!field || state->stack_ptr <= state->frame.framebase + frame_floor(state)
	|| !table_at(state, index, &table)) return false;
	elf_Value key = value_from_atom(elf_atom_from_data(state, field));
	elf_Value value = pop_value(state);
	elf_table_set(state, table, key, value);
	return true;
}

elf_b32 elf_get_index(elf_State *state, elf_i32 index, elf_u32 element)
{
	elf_Table *table;
	if (!table_at(state, index, &table)) return false;
	push_value(state, element < table->count ? table->array[element] : value_nil());
	return true;
}

elf_b32 elf_set_index(elf_State *state, elf_i32 index, elf_u32 element)
{
	elf_Table *table;
	if (state->stack_ptr <= state->frame.framebase + frame_floor(state)
	|| !table_at(state, index, &table) || element > table->count) return false;
	elf_Value value = pop_value(state);
	if (element == table->count) elf_array_add(state, table, value);
	else elf_array_set(state, table, element, value);
	return true;
}

elf_b32 elf_append(elf_State *state, elf_i32 index)
{
	elf_Table *table;
	if (state->stack_ptr <= state->frame.framebase + frame_floor(state)
	|| !table_at(state, index, &table)) return false;
	elf_array_add(state, table, pop_value(state));
	return true;
}

elf_b32 elf_next(elf_State *state, elf_i32 index, elf_u32 *cursor)
{
	elf_Table *table;
	if (!cursor || !table_at(state, index, &table) || *cursor >= table->count) return false;

	elf_u32 value_index = (*cursor)++;
	elf_Value key = value_from_integer(value_index);
	for (elf_u32 i = 0; i < table->nentries; ++i) {
		Entry entry = table->entries[i];
		if (entry_is_key(entry) && entry_index(entry) == value_index) {
			key = entry_key_value(entry);
			break;
		}
	}
	push_value(state, key);
	push_value(state, table->array[value_index]);
	return true;
}

elf_b32 elf_equal(elf_State *state, elf_i32 left, elf_i32 right)
{
	elf_Value *left_value = value_at(state, left);
	elf_Value *right_value = value_at(state, right);
	return left_value && right_value && values_equal(*left_value, *right_value);
}

void elf_get_global(elf_State *state, const char *name)
{
	if (!name) {
		push_value(state, value_nil());
		return;
	}
	elf_Value key = value_from_atom(elf_atom_from_data(state, name));
	push_value(state, elf_table_get_or_nil(state, state->globals, key));
}

elf_b32 elf_set_global(elf_State *state, const char *name)
{
	if (!name || state->stack_ptr <= state->frame.framebase + frame_floor(state)) return false;
	elf_Value key = value_from_atom(elf_atom_from_data(state, name));
	elf_table_set(state, state->globals, key, pop_value(state));
	return true;
}

elf_Ref elf_create_ref(elf_State *state, elf_i32 index)
{
	elf_Value *value = value_at(state, index);
	if (!value || value_is_nil(*value) || state->next_ref == 0) return ELF_NO_REF;
	elf_Ref reference = state->next_ref++;
	elf_table_set(state, state->ref_table, value_from_integer(reference), *value);
	return reference;
}

elf_b32 elf_push_ref(elf_State *state, elf_Ref reference)
{
	if (reference == ELF_NO_REF) return false;
	elf_Value key = value_from_integer(reference);
	if (!elf_table_contains(state, state->ref_table, key)) return false;
	push_value(state, elf_table_get_or_nil(state, state->ref_table, key));
	return true;
}

elf_b32 elf_release_ref(elf_State *state, elf_Ref reference)
{
	if (reference == ELF_NO_REF) return false;
	return elf_table_delete(state, state->ref_table, value_from_integer(reference), 0);
}
