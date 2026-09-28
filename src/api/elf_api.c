//
// See Copyright Notice In elf.h
//

#include "elf.h"
#include "base.h"
#include "core.h"
#include "helpers.h"
#include "compiler.h"
#include "value_text.h"

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

void elf_error(elf_State *state, const char *message)
{
	elf_report_runtime_error(state, RUNTIME_ERROR_GENERIC, -1, "%s",
		message ? message : "host error");
}

elf_Bool elf_get_diagnostic(elf_State *state, elf_Diagnostic *diagnostic)
{
	ASSERT(state);
	if (!diagnostic) return false;
	*diagnostic = state->diagnostic;
	return diagnostic->code != ELF_ERROR_NONE;
}

void elf_push_nil(elf_State *state)                 { push_value(state, value_nil());            }
void elf_push_int(elf_State *state, elf_Int x)      { push_value(state, value_from_integer(x));  }
void elf_push_num(elf_State *state, elf_Num x)      { push_value(state, value_from_number(x));   }
void elf_push_fun(elf_State *state, elf_Function x) { push_value(state, value_from_function(x)); }

void elf_push_cstr(elf_State *state, const char *data)
{
	push_value(state, value_from_string(elf_string_from_data(state, data)));
}

void elf_push_str(elf_State *state, const char *data, elf_Size size)
{
	push_value(state, value_from_string(elf_string_from_data_size(state, data, size)));
}

static elf_ErrorCode source_error_or(elf_State *state, elf_ErrorCode fallback)
{
	return state->diagnostic.code != ELF_ERROR_NONE ? state->diagnostic.code : fallback;
}

elf_ErrorCode elf_push_code_source(elf_State *state, const char *name, elf_StrSlice source)
{
	if (!name || !source.data) {
		elf_diagnostic_clear(state);
		return ELF_ERROR_INVALID_ARGUMENT;
	}
	BcFunctionRef function = elf_compile_source(state, name, source);
	if (!bc_function_ref_is_valid(function)) return source_error_or(state, ELF_ERROR_PARSE);
	ASSERT(bc_function_from_ref(function)->captures == 0);

	elf_Closure *closure = elf_gc_alloc(state, ELF_OBJECT_CLOSURE, sizeof(*closure));
	closure->function = function;
	push_value(state, value_from_closure(closure));
	return ELF_ERROR_NONE;
}

elf_ErrorCode elf_push_constant_expr(elf_State *state, const char *name, elf_StrSlice source)
{
	if (!name || !source.data) {
		elf_diagnostic_clear(state);
		return ELF_ERROR_INVALID_ARGUMENT;
	}
	if (!elf_push_constant_expr_source(state, name, source)) {
		return source_error_or(state, ELF_ERROR_EVALUATION);
	}
	return ELF_ERROR_NONE;
}

elf_ErrorCode elf_push_json(elf_State *state, const char *name, elf_StrSlice source)
{
	if (!name || !source.data) {
		elf_diagnostic_clear(state);
		return ELF_ERROR_INVALID_ARGUMENT;
	}
	if (!elf_push_json_source(state, name, source)) {
		return source_error_or(state, ELF_ERROR_PARSE);
	}
	return ELF_ERROR_NONE;
}

static elf_Value *value_at(elf_State *state, elf_Index index)
{
	elf_Index top = elf_get_top(state);
	elf_Index absolute = index < 0 ? top + index : index;
	if (absolute < 0 || absolute >= top) return 0;
	return state->frame->framebase + absolute;
}

static elf_Bool table_at(elf_State *state, elf_Index index, elf_Table **table)
{
	elf_Value *value = value_at(state, index);
	if (!value || !value_is_table(*value)) return false;
	*table = value_as_table(*value);
	return true;
}

static elf_ErrorCode table_at_for_write(elf_State *state, elf_Index index, elf_Table **table)
{
	elf_Value *value = value_at(state, index);
	if (!value) return ELF_ERROR_INVALID_INDEX;
	if (!value_is_table(*value)) return ELF_ERROR_TYPE_MISMATCH;
	*table = value_as_table(*value);
	return ELF_ERROR_NONE;
}

static elf_Bool table_is_readonly(elf_Table *table)
{
	return (table->obj.status & ELF_OBJECT_READONLY) != 0;
}

static elf_Bool values_equal(elf_Value left, elf_Value right)
{
	if (value_is_numeric(left) && value_is_numeric(right)) {
		return value_to_number(left) == value_to_number(right);
	}
	if (value_type(left) != value_type(right)) return false;
	if (value_is_string(left)) return strings_equal(value_as_string(left), value_as_string(right));
	return left.x_i64 == right.x_i64;
}

elf_Index elf_get_top(elf_State *state)
{
	return (elf_Index)(state->stack_ptr - state->frame->framebase);
}

elf_u32 elf_arg_count(elf_State *state)
{
	return state->frame->nargs;
}

elf_Index elf_abs_index(elf_State *state, elf_Index index)
{
	elf_Index absolute = index < 0 ? elf_get_top(state) + index : index;
	return elf_is_valid(state, absolute) ? absolute : -1;
}

elf_Bool elf_is_valid(elf_State *state, elf_Index index)
{
	return value_at(state, index) != 0;
}

elf_ErrorCode elf_set_top(elf_State *state, elf_Index top)
{
	elf_Index current = elf_get_top(state);
	elf_Index capacity = (elf_Index)(state->stack + state->stack_size - state->frame->framebase);
	if (top < frame_size(state) || top > capacity) return ELF_ERROR_OUT_OF_RANGE;
	while (current < top) {
		push_value(state, value_nil());
		current += 1;
	}
	if (current > top) {
		value_zero_many(state->frame->framebase + top, current - top);
		state->stack_ptr = state->frame->framebase + top;
	}
	return ELF_ERROR_NONE;
}

elf_ErrorCode elf_pop(elf_State *state, elf_u32 count)
{
	elf_Index top = elf_get_top(state);
	if (count > (elf_u32)(top - frame_size(state))) return ELF_ERROR_STACK_UNDERFLOW;
	return elf_set_top(state, top - (elf_Index) count);
}

elf_ErrorCode elf_push_value(elf_State *state, elf_Index index)
{
	elf_Value *value = value_at(state, index);
	if (!value) return ELF_ERROR_INVALID_INDEX;
	push_value(state, *value);
	return ELF_ERROR_NONE;
}

elf_ValueType elf_type(elf_State *state, elf_Index index)
{
	elf_Value *value = value_at(state, index);
	return value ? value_type(*value) : ELF_VALUE_TYPE_COUNT_;
}

elf_Bool elf_is_nil(elf_State *state, elf_Index index)
{
	elf_Value *value = value_at(state, index);
	return value && value_is_nil(*value);
}

elf_Bool elf_is_numeric(elf_State *state, elf_Index index)
{
	elf_Value *value = value_at(state, index);
	return value && value_is_numeric(*value);
}

elf_Bool elf_is_callable(elf_State *state, elf_Index index)
{
	elf_Value *value = value_at(state, index);
	return value && value_is_callable(*value);
}

elf_Bool elf_to_int(elf_State *state, elf_Index index, elf_Int *result)
{
	elf_Value *value = value_at(state, index);
	if (!value || !value_is_integer(*value) || !result) return false;
	*result = value_as_integer(*value);
	return true;
}

elf_Bool elf_to_num(elf_State *state, elf_Index index, elf_Num *result)
{
	elf_Value *value = value_at(state, index);
	if (!value || !value_is_numeric(*value) || !result) return false;
	*result = value_to_number(*value);
	return true;
}

elf_Bool elf_to_str(elf_State *state, elf_Index index, elf_StrSlice *result)
{
	elf_Value *value = value_at(state, index);
	if (!value || !value_is_string(*value) || !result) return false;
	elf_String *string = value_as_string(*value);
	result->data = string->data;
	result->size = string_size(string);
	return true;
}

elf_Bool elf_to_cstr(elf_State *state, elf_Index index, const char **result)
{
	elf_StrSlice string;
	if (!result || !elf_to_str(state, index, &string)) return false;
	*result = string.data;
	return true;
}

elf_Bool elf_push_value_text(elf_State *state, elf_Index index)
{
	elf_Value *value = value_at(state, index);
	if (!value) return false;
	elf_Scratch scratch = elf_begin_scratch();
	char *begin = elf_arena_push(scratch.arena, 0);
	elf_print_value(scratch.arena, *value);
	char *end = elf_arena_push(scratch.arena, 0);
	elf_push_str(state, begin, (int)(end - begin));
	elf_end_scratch(scratch);
	return true;
}

elf_Bool elf_push_value_source(elf_State *state, elf_Index index)
{
	elf_Value *value = value_at(state, index);
	if (!value) return false;

	elf_Scratch scratch = elf_begin_scratch();
	char *begin = elf_arena_push(scratch.arena, 0);
	b32 result = elf_unparse_value(state, scratch.arena, *value, 0);
	if (result)
	{
		char *end = elf_arena_push(scratch.arena, 0);
		ASSERT(end - begin <= 0x7fffffff);
		elf_push_str(state, begin, (int)(end - begin));
	}
	elf_end_scratch(scratch);
	return result;
}

void elf_new_table(elf_State *state)
{
	push_new_table(state);
}

elf_Bool elf_length(elf_State *state, elf_Index index, elf_u32 *length)
{
	elf_Table *table;
	if (!length || !table_at(state, index, &table)) return false;
	*length = elf_array_length(table);
	return true;
}

elf_Bool elf_get_field(elf_State *state, elf_Index index, const char *field)
{
	elf_Table *table;
	if (!field || !table_at(state, index, &table)) return false;
	elf_Value key = value_from_string(elf_string_from_data(state, field));
	push_value(state, elf_table_get_or_nil(state, table, key));
	return true;
}

elf_ErrorCode elf_set_field(elf_State *state, elf_Index index, const char *field)
{
	elf_Table *table;
	if (!field) return ELF_ERROR_INVALID_ARGUMENT;
	elf_ErrorCode error = table_at_for_write(state, index, &table);
	if (error != ELF_ERROR_NONE) return error;
	if (state->stack_ptr <= state->frame->framebase + frame_size(state)) return ELF_ERROR_STACK_UNDERFLOW;
	if (table_is_readonly(table)) return ELF_ERROR_READONLY;
	elf_Value key = value_from_string(elf_string_from_data(state, field));
	elf_Value value = pop_value(state);
	elf_table_set(state, table, key, value);
	return ELF_ERROR_NONE;
}

elf_Bool elf_get_index(elf_State *state, elf_Index index, elf_u32 element)
{
	elf_Table *table;
	if (!table_at(state, index, &table)) return false;
	push_value(state, element < table->count ? table->array[element] : value_nil());
	return true;
}

elf_ErrorCode elf_set_index(elf_State *state, elf_Index index, elf_u32 element)
{
	elf_Table *table;
	elf_ErrorCode error = table_at_for_write(state, index, &table);
	if (error != ELF_ERROR_NONE) return error;
	if (state->stack_ptr <= state->frame->framebase + frame_size(state)) return ELF_ERROR_STACK_UNDERFLOW;
	if (element > table->count) return ELF_ERROR_OUT_OF_RANGE;
	if (table_is_readonly(table)) return ELF_ERROR_READONLY;
	elf_Value value = pop_value(state);
	if (element == table->count) elf_array_add(state, table, value);
	else elf_array_set(state, table, element, value);
	return ELF_ERROR_NONE;
}

elf_ErrorCode elf_append(elf_State *state, elf_Index index)
{
	elf_Table *table;
	elf_ErrorCode error = table_at_for_write(state, index, &table);
	if (error != ELF_ERROR_NONE) return error;
	if (state->stack_ptr <= state->frame->framebase + frame_size(state)) return ELF_ERROR_STACK_UNDERFLOW;
	if (table_is_readonly(table)) return ELF_ERROR_READONLY;
	elf_array_add(state, table, pop_value(state));
	return ELF_ERROR_NONE;
}

elf_Bool elf_next(elf_State *state, elf_Index index, elf_u32 *cursor)
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

elf_Bool elf_equal(elf_State *state, elf_Index left, elf_Index right)
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
	elf_Value key = value_from_string(elf_string_from_data(state, name));
	push_value(state, elf_table_get_or_nil(state, state->globals, key));
}

elf_ErrorCode elf_set_global(elf_State *state, const char *name)
{
	if (!name) return ELF_ERROR_INVALID_ARGUMENT;
	if (state->stack_ptr <= state->frame->framebase + frame_size(state)) return ELF_ERROR_STACK_UNDERFLOW;
	if (table_is_readonly(state->globals)) return ELF_ERROR_READONLY;
	elf_Value key = value_from_string(elf_string_from_data(state, name));
	elf_table_set(state, state->globals, key, pop_value(state));
	return ELF_ERROR_NONE;
}

elf_Ref elf_create_ref(elf_State *state, elf_Index index)
{
	elf_Value *value = value_at(state, index);
	if (!value || value_is_nil(*value) || state->next_ref == 0) return ELF_NO_REF;
	elf_Ref reference = state->next_ref++;
	elf_table_set(state, state->ref_table, value_from_integer(reference), *value);
	return reference;
}

elf_Bool elf_push_ref(elf_State *state, elf_Ref reference)
{
	if (reference == ELF_NO_REF) return false;
	elf_Value key = value_from_integer(reference);
	if (!elf_table_contains(state, state->ref_table, key)) return false;
	push_value(state, elf_table_get_or_nil(state, state->ref_table, key));
	return true;
}

elf_ErrorCode elf_release_ref(elf_State *state, elf_Ref reference)
{
	if (reference == ELF_NO_REF) return ELF_ERROR_INVALID_REFERENCE;
	if (!elf_table_delete(state, state->ref_table, value_from_integer(reference), 0)) {
		return ELF_ERROR_INVALID_REFERENCE;
	}
	return ELF_ERROR_NONE;
}
