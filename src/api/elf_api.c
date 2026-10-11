//
// See Copyright Notice In elf.h
//

#include "elf.h"
#include "base.h"
#include "core.h"
#include "helpers.h"
#include "compiler.h"
#include "value_text.h"

static elf_Value *value_at(elf_State *state, elf_Index index);

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

void elf_push_error(elf_State *state, const char *message)
{
	elf_Table *info = elf_new_table_rogue(state);
	push_value(state, value_from_error(info));
	elf_push_cstr(state, message ? message : "error");
	elf_Value key = value_from_string(elf_string_from_data(state, "message"));
	elf_table_set(state, info, key, pop_value(state));
}

elf_ErrorCode elf_push_buffer(elf_State *state, elf_Size capacity)
{
	if (!state) return ELF_ERROR_INVALID_ARGUMENT;
	if (capacity > UINT32_MAX) return ELF_ERROR_OUT_OF_RANGE;
	push_value(state, value_from_buffer(elf_buffer_new_object(state, (u32)capacity)));
	return ELF_ERROR_NONE;
}

elf_ErrorCode elf_push_user_object(elf_State *state, const elf_UserType *type,
	elf_Index metatable, elf_Size size, void **data)
{
	if (data) *data = 0;
	if (!state || !type || !type->name || !data) return ELF_ERROR_INVALID_ARGUMENT;
	elf_Value *metatable_value = value_at(state, metatable);
	if (!metatable_value) return ELF_ERROR_INVALID_INDEX;
	if (!value_is_table(*metatable_value)) return ELF_ERROR_TYPE_MISMATCH;
	if (size > UINT32_MAX - offsetof(elf_UserObject, data)) return ELF_ERROR_OUT_OF_RANGE;
	elf_UserObject *user = elf_user_object_new(state, type,
		value_as_table(*metatable_value), (u32)size);
	push_value(state, value_from_user(user));
	*data = user->data;
	return ELF_ERROR_NONE;
}

static void clear_compile_report(elf_CompileReport *report)
{
	if (report) *report = (elf_CompileReport) {};
}

void elf_destroy_compile_report(elf_CompileReport *report)
{
	if (!report) return;
	elf_Diagnostic *diagnostics = (elf_Diagnostic *)report->diagnostics;
	for (elf_Size i = 0; i < report->diagnostic_count; ++i) {
		elf_free(0, diagnostics[i].source_name.data,
			diagnostics[i].source_name.size + 1);
		elf_free(0, diagnostics[i].message.data,
			diagnostics[i].message.size + 1);
	}
	elf_free(0, diagnostics,
		report->diagnostic_count * sizeof(*diagnostics));
	*report = (elf_CompileReport) {};
}

elf_ErrorCode elf_push_code_source(elf_State *state, const char *name, elf_StrSlice source, elf_CompileReport *report)
{
	if (!name || !source.data) {
		clear_compile_report(report);
		return ELF_ERROR_INVALID_ARGUMENT;
	}
	BcFunctionRef function = elf_compile_source(state, name, source, report);
	if (!bc_function_ref_is_valid(function)) return ELF_ERROR_COMPILATION_FAILED;
	ASSERT(bc_function_from_ref(function)->captures == 0);

	elf_Closure *closure = elf_gc_alloc(state, ELF_OBJECT_CLOSURE, sizeof(*closure));
	closure->function = function;
	push_value(state, value_from_closure(closure));
	return ELF_ERROR_NONE;
}

elf_ErrorCode elf_push_constant_expr(elf_State *state, const char *name, elf_StrSlice source, elf_CompileReport *report)
{
	if (!name || !source.data) {
		clear_compile_report(report);
		return ELF_ERROR_INVALID_ARGUMENT;
	}
	if (!elf_push_constant_expr_source(state, name, source, report)) {
		return ELF_ERROR_COMPILATION_FAILED;
	}
	return ELF_ERROR_NONE;
}

elf_ErrorCode elf_push_json(elf_State *state, const char *name, elf_StrSlice source, elf_CompileReport *report)
{
	if (!name || !source.data) {
		clear_compile_report(report);
		return ELF_ERROR_INVALID_ARGUMENT;
	}
	if (!elf_push_json_source(state, name, source, report)) {
		return ELF_ERROR_COMPILATION_FAILED;
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

static elf_Bool field_table_at(elf_State *state, elf_Index index, elf_Table **table)
{
	elf_Value *value = value_at(state, index);
	if (!value) return false;
	if (value_is_table(*value)) *table = value_as_table(*value);
	else if (value_is_error(*value)) *table = value_as_error(*value);
	else return false;
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

static elf_ErrorCode field_table_at_for_write(elf_State *state, elf_Index index, elf_Table **table)
{
	elf_Value *value = value_at(state, index);
	if (!value) return ELF_ERROR_INVALID_INDEX;
	if (value_is_table(*value)) *table = value_as_table(*value);
	else if (value_is_error(*value)) *table = value_as_error(*value);
	else return ELF_ERROR_TYPE_MISMATCH;
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

elf_Bool elf_is_error(elf_State *state, elf_Index index)
{
	elf_Value *value = value_at(state, index);
	return value && value_is_error(*value);
}

elf_Bool elf_is_buffer(elf_State *state, elf_Index index)
{
	elf_Value *value = value_at(state, index);
	return value && value_is_buffer(*value);
}

elf_Bool elf_is_user_object(elf_State *state, elf_Index index, const elf_UserType *type)
{
	elf_Value *value = value_at(state, index);
	return value && value_is_user(*value) && (!type || value_as_user(*value)->type == type);
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

elf_Bool elf_to_buffer(elf_State *state, elf_Index index, elf_BufferView *result)
{
	elf_Value *value = value_at(state, index);
	if (!value || !value_is_buffer(*value) || !result) return false;
	elf_ByteBuffer *buffer = value_as_buffer(*value);
	result->data = buffer->data;
	result->size = buffer->size;
	result->capacity = buffer->capacity;
	return true;
}

void *elf_to_user_object(elf_State *state, elf_Index index, const elf_UserType *type)
{
	elf_Value *value = value_at(state, index);
	if (!value || !value_is_user(*value)) return 0;
	elf_UserObject *user = value_as_user(*value);
	if (type && user->type != type) return 0;
	return user->data;
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
	elf_Value *value = value_at(state, index);
	elf_Bool result = false;
	if (length && value)
	{
		result = value_length(*value, length);
	}
	return result;
}

elf_Bool elf_get_field(elf_State *state, elf_Index index, const char *field)
{
	elf_Table *table;
	if (!field || !field_table_at(state, index, &table)) return false;
	elf_Value key = value_from_string(elf_string_from_data(state, field));
	push_value(state, elf_table_get_or_nil(state, table, key));
	return true;
}

elf_ErrorCode elf_set_field(elf_State *state, elf_Index index, const char *field)
{
	elf_Table *table;
	if (!field) return ELF_ERROR_INVALID_ARGUMENT;
	elf_ErrorCode error = field_table_at_for_write(state, index, &table);
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
	elf_Value *value = value_at(state, index);
	if (!value) return false;
	if (value_is_table(*value))
	{
		elf_Table *table = value_as_table(*value);
		push_value(state, element < table->count ? table->array[element] : value_nil());
	}
	else if (value_is_buffer(*value))
	{
		elf_ByteBuffer *buffer = value_as_buffer(*value);
		if (element >= buffer->size) return false;
		push_value(state, value_from_integer(buffer->data[element]));
	}
	else return false;
	return true;
}

elf_ErrorCode elf_set_index(elf_State *state, elf_Index index, elf_u32 element)
{
	elf_Value *target = value_at(state, index);
	if (!target) return ELF_ERROR_INVALID_INDEX;
	if (state->stack_ptr <= state->frame->framebase + frame_size(state)) return ELF_ERROR_STACK_UNDERFLOW;
	if (value_is_table(*target))
	{
		elf_Table *table = value_as_table(*target);
		if (element > table->count) return ELF_ERROR_OUT_OF_RANGE;
		if (table_is_readonly(table)) return ELF_ERROR_READONLY;
		elf_Value value = pop_value(state);
		if (element == table->count) elf_array_add(state, table, value);
		else elf_array_set(state, table, element, value);
	}
	else if (value_is_buffer(*target))
	{
		elf_ByteBuffer *buffer = value_as_buffer(*target);
		elf_Value value = state->stack_ptr[-1];
		if (element >= buffer->size) return ELF_ERROR_OUT_OF_RANGE;
		if (!value_is_integer(value)) return ELF_ERROR_TYPE_MISMATCH;
		if (value_as_integer(value) < 0 || value_as_integer(value) > UINT8_MAX)
			return ELF_ERROR_OUT_OF_RANGE;
		if (buffer->obj.status & ELF_OBJECT_READONLY) return ELF_ERROR_READONLY;
		pop_value(state);
		buffer->data[element] = (u8)value_as_integer(value);
	}
	else return ELF_ERROR_TYPE_MISMATCH;
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

static elf_ErrorCode mutable_buffer_at(elf_State *state, elf_Index index,
	elf_ByteBuffer **result)
{
	elf_Value *value = value_at(state, index);
	if (!value) return ELF_ERROR_INVALID_INDEX;
	if (!value_is_buffer(*value)) return ELF_ERROR_TYPE_MISMATCH;
	elf_ByteBuffer *buffer = value_as_buffer(*value);
	if (buffer->obj.status & ELF_OBJECT_READONLY) return ELF_ERROR_READONLY;
	*result = buffer;
	return ELF_ERROR_NONE;
}

elf_ErrorCode elf_buffer_reserve(elf_State *state, elf_Index index, elf_Size capacity)
{
	elf_ByteBuffer *buffer;
	if (capacity > UINT32_MAX) return ELF_ERROR_OUT_OF_RANGE;
	elf_ErrorCode error = mutable_buffer_at(state, index, &buffer);
	if (error != ELF_ERROR_NONE) return error;
	return elf_buffer_reserve_object(state, buffer, (u32)capacity) ?
		ELF_ERROR_NONE : ELF_ERROR_OUT_OF_RANGE;
}

elf_ErrorCode elf_buffer_resize(elf_State *state, elf_Index index, elf_Size size)
{
	elf_ByteBuffer *buffer;
	if (size > UINT32_MAX) return ELF_ERROR_OUT_OF_RANGE;
	elf_ErrorCode error = mutable_buffer_at(state, index, &buffer);
	if (error != ELF_ERROR_NONE) return error;
	return elf_buffer_resize_object(state, buffer, (u32)size) ?
		ELF_ERROR_NONE : ELF_ERROR_OUT_OF_RANGE;
}

elf_ErrorCode elf_buffer_append(elf_State *state, elf_Index index,
	const void *data, elf_Size size)
{
	elf_ByteBuffer *buffer;
	if (!data && size) return ELF_ERROR_INVALID_ARGUMENT;
	if (size > UINT32_MAX) return ELF_ERROR_OUT_OF_RANGE;
	elf_ErrorCode error = mutable_buffer_at(state, index, &buffer);
	if (error != ELF_ERROR_NONE) return error;
	if (size > UINT32_MAX - buffer->size) return ELF_ERROR_OUT_OF_RANGE;
	return elf_buffer_append_object(state, buffer, data, (u32)size) ?
		ELF_ERROR_NONE : ELF_ERROR_OUT_OF_RANGE;
}

elf_ErrorCode elf_buffer_patch(elf_State *state, elf_Index index, elf_Size position,
	elf_Size delete_size, const void *data, elf_Size insert_size)
{
	elf_ByteBuffer *buffer;
	if (!data && insert_size) return ELF_ERROR_INVALID_ARGUMENT;
	if (position > UINT32_MAX || delete_size > UINT32_MAX || insert_size > UINT32_MAX)
		return ELF_ERROR_OUT_OF_RANGE;
	elf_ErrorCode error = mutable_buffer_at(state, index, &buffer);
	if (error != ELF_ERROR_NONE) return error;
	if (position > buffer->size || delete_size > buffer->size - position ||
		insert_size > UINT32_MAX - (buffer->size - delete_size))
		return ELF_ERROR_OUT_OF_RANGE;
	return elf_buffer_patch_object(state, buffer, (u32)position, (u32)delete_size,
		data, (u32)insert_size) ? ELF_ERROR_NONE : ELF_ERROR_OUT_OF_RANGE;
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
