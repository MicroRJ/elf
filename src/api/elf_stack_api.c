//
// See Copyright Notice In elf.h
//

#include "elf.h"
#include "core.h"
#include "helpers.h"

static elf_i32 stack_floor(elf_State *state)
{
	return state->frame.framesize;
}

static elf_Value *stack_slot(elf_State *state, elf_i32 index)
{
	elf_i32 top = elf_stack_get_top(state);
	elf_i32 absolute = index < 0 ? top + index : index;
	if (absolute < 0 || absolute >= top) return 0;
	return state->frame.framebase + absolute;
}

static elf_b32 stack_table(elf_State *state, elf_i32 index, elf_Table **table)
{
	elf_Value *value = stack_slot(state, index);
	if (!value || !value_is_table(*value)) return false;
	*table = value_as_table(*value);
	return true;
}

static elf_b32 stack_values_equal(elf_Value left, elf_Value right)
{
	if (value_is_numeric(left) && value_is_numeric(right)) {
		return value_to_number(left) == value_to_number(right);
	}
	if (value_type(left) != value_type(right)) return false;
	if (value_is_atom(left)) return atoms_equal(value_as_atom(left), value_as_atom(right));
	return left.x_i64 == right.x_i64;
}

elf_i32 elf_stack_get_top(elf_State *state)
{
	return (elf_i32)(state->stack_ptr - state->frame.framebase);
}

elf_i32 elf_stack_arg_count(elf_State *state)
{
	return state->frame.nargs;
}

elf_i32 elf_stack_abs_index(elf_State *state, elf_i32 index)
{
	elf_i32 absolute = index < 0 ? elf_stack_get_top(state) + index : index;
	return elf_stack_is_valid(state, absolute) ? absolute : -1;
}

elf_b32 elf_stack_is_valid(elf_State *state, elf_i32 index)
{
	return stack_slot(state, index) != 0;
}

elf_b32 elf_stack_set_top(elf_State *state, elf_i32 top)
{
	elf_i32 current = elf_stack_get_top(state);
	if (top < stack_floor(state) || state->frame.framebase + top > state->stack + state->stack_size) {
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

elf_b32 elf_stack_pop(elf_State *state, elf_u32 count)
{
	elf_i32 top = elf_stack_get_top(state);
	if (count > (elf_u32)(top - stack_floor(state))) return false;
	return elf_stack_set_top(state, top - (elf_i32)count);
}

elf_b32 elf_stack_push_value(elf_State *state, elf_i32 index)
{
	elf_Value *value = stack_slot(state, index);
	if (!value) return false;
	push_value(state, *value);
	return true;
}

elf_ValueType elf_stack_type(elf_State *state, elf_i32 index)
{
	elf_Value *value = stack_slot(state, index);
	return value ? value_type(*value) : ELF_VALUE_TYPE_COUNT_;
}

elf_b32 elf_stack_is_nil(elf_State *state, elf_i32 index)
{
	elf_Value *value = stack_slot(state, index);
	return value && value_is_nil(*value);
}

elf_b32 elf_stack_is_numeric(elf_State *state, elf_i32 index)
{
	elf_Value *value = stack_slot(state, index);
	return value && value_is_numeric(*value);
}

elf_b32 elf_stack_is_callable(elf_State *state, elf_i32 index)
{
	elf_Value *value = stack_slot(state, index);
	return value && value_is_callable(*value);
}

elf_b32 elf_stack_to_int(elf_State *state, elf_i32 index, elf_Integer *result)
{
	elf_Value *value = stack_slot(state, index);
	if (!value || !value_is_integer(*value) || !result) return false;
	*result = value_as_integer(*value);
	return true;
}

elf_b32 elf_stack_to_num(elf_State *state, elf_i32 index, elf_Number *result)
{
	elf_Value *value = stack_slot(state, index);
	if (!value || !value_is_numeric(*value) || !result) return false;
	*result = value_to_number(*value);
	return true;
}

elf_b32 elf_stack_to_str(elf_State *state, elf_i32 index, elf_StrSlice *result)
{
	elf_Value *value = stack_slot(state, index);
	if (!value || !value_is_atom(*value) || !result) return false;
	elf_String *string = value_as_atom(*value);
	result->data = string->data;
	result->size = string->size;
	return true;
}

void elf_stack_new_table(elf_State *state)
{
	elf_push_new_table(state);
}

elf_b32 elf_stack_length(elf_State *state, elf_i32 index, elf_u32 *length)
{
	elf_Table *table;
	if (!length || !stack_table(state, index, &table)) return false;
	*length = elf_array_length(table);
	return true;
}

elf_b32 elf_stack_get_field(elf_State *state, elf_i32 index, const char *field)
{
	elf_Table *table;
	if (!field || !stack_table(state, index, &table)) return false;
	elf_Value key = value_from_atom(elf_atom_from_data(state, field));
	push_value(state, elf_table_get_or_nil(state, table, key));
	return true;
}

elf_b32 elf_stack_set_field(elf_State *state, elf_i32 index, const char *field)
{
	elf_Table *table;
	if (!field || state->stack_ptr <= state->frame.framebase + stack_floor(state)
	|| !stack_table(state, index, &table)) return false;
	elf_Value key = value_from_atom(elf_atom_from_data(state, field));
	elf_Value value = pop_value(state);
	elf_table_set(state, table, key, value);
	return true;
}

elf_b32 elf_stack_get_index(elf_State *state, elf_i32 index, elf_u32 element)
{
	elf_Table *table;
	if (!stack_table(state, index, &table)) return false;
	push_value(state, element < table->count ? table->array[element] : value_nil());
	return true;
}

elf_b32 elf_stack_set_index(elf_State *state, elf_i32 index, elf_u32 element)
{
	elf_Table *table;
	if (state->stack_ptr <= state->frame.framebase + stack_floor(state)
	|| !stack_table(state, index, &table) || element > table->count) return false;
	elf_Value value = pop_value(state);
	if (element == table->count) elf_array_add(state, table, value);
	else elf_array_set(state, table, element, value);
	return true;
}

elf_b32 elf_stack_add(elf_State *state, elf_i32 index)
{
	elf_Table *table;
	if (state->stack_ptr <= state->frame.framebase + stack_floor(state)
	|| !stack_table(state, index, &table)) return false;
	elf_array_add(state, table, pop_value(state));
	return true;
}

elf_b32 elf_stack_next(elf_State *state, elf_i32 index, elf_u32 *cursor)
{
	elf_Table *table;
	if (!cursor || !stack_table(state, index, &table) || *cursor >= table->count) return false;

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

elf_b32 elf_stack_equal(elf_State *state, elf_i32 left, elf_i32 right)
{
	elf_Value *left_value = stack_slot(state, left);
	elf_Value *right_value = stack_slot(state, right);
	return left_value && right_value && stack_values_equal(*left_value, *right_value);
}

void elf_stack_get_global(elf_State *state, const char *name)
{
	if (!name) {
		push_value(state, value_nil());
		return;
	}
	elf_Value key = value_from_atom(elf_atom_from_data(state, name));
	push_value(state, elf_table_get_or_nil(state, state->globals, key));
}

elf_b32 elf_stack_set_global(elf_State *state, const char *name)
{
	if (!name || state->stack_ptr <= state->frame.framebase + stack_floor(state)) return false;
	elf_Value key = value_from_atom(elf_atom_from_data(state, name));
	elf_table_set(state, state->globals, key, pop_value(state));
	return true;
}

elf_Ref elf_create_ref(elf_State *state, elf_i32 index)
{
	elf_Value *value = stack_slot(state, index);
	if (!value || value_is_nil(*value) || state->api_next_reference == 0) return ELF_NO_REF;
	elf_Ref reference = state->api_next_reference++;
	elf_table_set(state, state->api_references, value_from_integer(reference), *value);
	return reference;
}

elf_b32 elf_push_ref(elf_State *state, elf_Ref reference)
{
	if (reference == ELF_NO_REF) return false;
	elf_Value key = value_from_integer(reference);
	if (!elf_table_contains(state, state->api_references, key)) return false;
	push_value(state, elf_table_get_or_nil(state, state->api_references, key));
	return true;
}

elf_b32 elf_release_ref(elf_State *state, elf_Ref reference)
{
	if (reference == ELF_NO_REF) return false;
	return elf_table_delete(state, state->api_references, value_from_integer(reference), 0);
}
