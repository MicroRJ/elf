//
// See Copyright Notice In elf.h
//

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
	report_runtime_error(state, ELF_ERROR_RUNTIME, "%s",
		message ? message : "host error");
}

void elf_push_nil(elf_State *state)                 { push_value(state, value_nil());            }
void elf_push_int(elf_State *state, elf_Int x)      { push_value(state, int_value(x));  }
void elf_push_num(elf_State *state, elf_Num x)      { push_value(state, num_value(x));   }
void elf_push_fun(elf_State *state, elf_Function x) { push_value(state, value_from_function(x)); }

void elf_push_cstr(elf_State *state, const char *data)
{
	push_value(state, str_value(elf_string_from_data(state, data)));
}

void elf_push_str(elf_State *state, const char *data, elf_Size size)
{
	push_value(state, str_value(elf_string_from_data_size(state, data, size)));
}

void elf_push_error(elf_State *state, elf_ErrorCode code, const char *message)
{
	elf_String *error_message = elf_string_from_data(state, message ? message : "error");
	push_value(state, str_value(error_message));
	elf_GCError *error = elf_error_new(state, code, error_message, 0);
	state->stack_top[-1] = value_from_error(error);
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

static elf_Value *value_at(elf_State *state, elf_Index index)
{
	elf_Index top = elf_get_top(state);
	elf_Index absolute = index < 0 ? top + index : index;
	if (absolute < 0 || absolute >= top) return 0;
	return frame_args(frame_current(state)) + absolute;
}

static elf_Bool table_at(elf_State *state, elf_Index index, elf_Table **table)
{
	elf_Value *value = value_at(state, index);
	if (!value || !value_is_table(*value)) return false;
	*table = value_as_table(*value);
	return true;
}

static elf_ErrorCode field_table_at_for_write(elf_State *state, elf_Index index, elf_Table **table)
{
	elf_Value *value = value_at(state, index);
	if (!value) return ELF_ERROR_INVALID_INDEX;
	if (value_is_table(*value)) *table = value_as_table(*value);
	else return ELF_ERROR_TYPE_MISMATCH;
	return ELF_ERROR_NONE;
}

static elf_Bool table_is_readonly(elf_Table *table)
{
	return (table->obj.status & ELF_OBJECT_READONLY) != 0;
}

static elf_Bool array_is_readonly(elf_Array *array)
{
	return (array->obj.status & ELF_OBJECT_READONLY) != 0;
}

static elf_Bool values_equal(elf_Value left, elf_Value right)
{
	if (value_is_numeric(left) && value_is_numeric(right)) {
		return value_to_number(left) == value_to_number(right);
	}
	if (value_type(left) != value_type(right)) return false;
	if (value_is_string(left)) return elf_string_equal(value_as_string(left), value_as_string(right));
	return left.x_i64 == right.x_i64;
}

elf_Index elf_get_top(elf_State *state)
{
	return state->stack_top - (state->frame->base + 1);
}

elf_u32 elf_nargs(elf_State *state)
{
	return frame_nargs(frame_current(state));
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

elf_ErrorCode elf_set_top(elf_State *state, elf_Index index)
{
	if (index < 0) {
		return ELF_ERROR_OUT_OF_RANGE;
	}
	StackFrame *frame = state->frame;
	elf_Value *next = frame->base + 1 + index;
	if (next < frame->base + frame->size) {
		return ELF_ERROR_OUT_OF_RANGE;
	}
	if (next > state->stack + state->stack_size) {
		return ELF_ERROR_OUT_OF_RANGE;
	}
	if (next > state->stack_top) {
		value_zero_range(state->stack_top, next);
	}
	state->stack_top = next;
	return ELF_ERROR_NONE;
}

elf_ErrorCode elf_pop(elf_State *state, elf_u32 count)
{
	StackFrame *frame = state->frame;
	elf_Value *next = state->stack_top - count;
	if (next < frame->base + frame->size) {
		return ELF_ERROR_STACK_UNDERFLOW;
	}
	state->stack_top = next;
	return ELF_ERROR_NONE;
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

elf_ErrorCode elf_print_error(elf_State *state, elf_Index index)
{
	if (!state)
	{
		return ELF_ERROR_INVALID_ARGUMENT;
	}

	elf_Value *value = value_at(state, index);
	if (!value)
	{
		return ELF_ERROR_INVALID_INDEX;
	}
	if (!value_is_error(*value))
	{
		return ELF_ERROR_TYPE_MISMATCH;
	}

	print_error_diagnostic(value_as_error(*value));
	return ELF_ERROR_NONE;
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
	result->size = elf_string_size(string);
	return true;
}

elf_Bool elf_to_cstr(elf_State *state, elf_Index index, const char **result)
{
	elf_StrSlice string;
	if (!result || !elf_to_str(state, index, &string)) return false;
	*result = string.data;
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
	Scratch scratch = scratch_begin();
	char *begin = arena_push(scratch.arena, 0, 1);
	elf_print_value(scratch.arena, *value);
	char *end = arena_push(scratch.arena, 0, 1);
	elf_push_str(state, begin, (int)(end - begin));
	scratch_end(scratch);
	return true;
}

elf_Bool elf_push_value_source(elf_State *state, elf_Index index)
{
	elf_Value *value = value_at(state, index);
	if (!value) return false;

	Scratch scratch = scratch_begin();
	char *begin = arena_push(scratch.arena, 0, 1);
	b32 result = elf_unparse_value(state, scratch.arena, *value, 0);
	if (result)
	{
		char *end = arena_push(scratch.arena, 0, 1);
		ASSERT(end - begin <= 0x7fffffff);
		elf_push_str(state, begin, (int)(end - begin));
	}
	scratch_end(scratch);
	return result;
}

void elf_new_table(elf_State *state)
{
	push_new_table(state);
}

void elf_new_array(elf_State *state)
{
	push_new_array(state);
}

elf_Bool elf_length(elf_State *state, elf_Index index, elf_u32 *length)
{
	elf_Value *value = value_at(state, index);
	elf_Bool result = false;
	if (length && value)
	{
		result = value_measure(*value, length);
	}
	return result;
}

elf_Bool elf_get_field(elf_State *state, elf_Index index, const char *field)
{
	elf_Table *table;
	if (!field || !table_at(state, index, &table)) return false;
	elf_Value key = str_value(elf_string_from_data(state, field));
	push_value(state, elf_table_get_or_nil(state, table, key));
	return true;
}

elf_ErrorCode elf_set_field(elf_State *state, elf_Index index, const char *field)
{
	if (!field) {
		return ELF_ERROR_INVALID_ARGUMENT;
	}
	elf_StrSlice slice = {(char *)field, strlen(field)};
	return elf_set_field_slice(state, index, slice);
}

elf_ErrorCode elf_set_field_slice(elf_State *state, elf_Index index, elf_StrSlice field)
{
	elf_Table *table;
	if (!field.data) {
		return ELF_ERROR_INVALID_ARGUMENT;
	}
	if (field.size > ELF_STRING_MAX_SIZE) {
		return ELF_ERROR_OUT_OF_RANGE;
	}
	elf_ErrorCode error = field_table_at_for_write(state, index, &table);
	if (error != ELF_ERROR_NONE) {
		return error;
	}
	if (state->stack_top <= frame_top(frame_current(state))) {
		return ELF_ERROR_STACK_UNDERFLOW;
	}
	if (table_is_readonly(table)) {
		return ELF_ERROR_READONLY;
	}
	elf_Value key = str_value(elf_string_from_data_size(state, field.data, (u32)field.size));
	elf_Value value = pop_value(state);
	elf_table_set(state, table, key, value);
	return ELF_ERROR_NONE;
}

elf_Bool elf_get_index(elf_State *state, elf_Index index, elf_u32 element)
{
	elf_Value *value = value_at(state, index);
	if (!value) return false;
	if (value_is_array(*value))
	{
		elf_Array *array = value_as_array(*value);
		if (element >= array->count)
		{
			return false;
		}
		push_value(state, array->items[element]);
	}
	else if (value_is_buffer(*value))
	{
		elf_ByteBuffer *buffer = value_as_buffer(*value);
		if (element >= buffer->size) return false;
		push_value(state, int_value(buffer->data[element]));
	}
	else return false;
	return true;
}

elf_ErrorCode elf_set_index(elf_State *state, elf_Index index, elf_u32 element)
{
	elf_Value *target = value_at(state, index);
	if (!target) return ELF_ERROR_INVALID_INDEX;
	if (state->stack_top <= frame_top(frame_current(state))) return ELF_ERROR_STACK_UNDERFLOW;
	if (value_is_array(*target))
	{
		elf_Array *array = value_as_array(*target);
		if (element > array->count)
		{
			return ELF_ERROR_OUT_OF_RANGE;
		}
		if (array_is_readonly(array))
		{
			return ELF_ERROR_READONLY;
		}
		elf_Value value = pop_value(state);
		if (element == array->count)
		{
			elf_array_add(state, array, value);
		}
		else
		{
			elf_array_set(state, array, element, value);
		}
	}
	else if (value_is_buffer(*target))
	{
		elf_ByteBuffer *buffer = value_as_buffer(*target);
		elf_Value value = state->stack_top[-1];
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
	elf_Value *target = value_at(state, index);
	if (!target)
	{
		return ELF_ERROR_INVALID_INDEX;
	}
	if (state->stack_top <= frame_top(frame_current(state)))
	{
		return ELF_ERROR_STACK_UNDERFLOW;
	}
	if (!value_is_array(*target))
	{
		return ELF_ERROR_TYPE_MISMATCH;
	}
	elf_Array *array = value_as_array(*target);
	if (array_is_readonly(array))
	{
		return ELF_ERROR_READONLY;
	}
	elf_array_add(state, array, pop_value(state));
	return ELF_ERROR_NONE;
}

elf_Bool elf_next(elf_State *state, elf_Index index, elf_u32 *cursor)
{
	elf_Table *table;
	if (!cursor || !table_at(state, index, &table))
	{
		return false;
	}
	while (*cursor < table->capacity)
	{
		Entry entry = table->entries[(*cursor)++];
		if (elf_table_entry_is_key(entry))
		{
			push_value(state, entry.key);
			push_value(state, entry.value);
			return true;
		}
	}
	return false;
}

elf_Bool elf_equal(elf_State *state, elf_Index left, elf_Index right)
{
	elf_Value *left_value = value_at(state, left);
	elf_Value *right_value = value_at(state, right);
	return left_value && right_value && values_equal(*left_value, *right_value);
}

elf_Ref elf_create_ref(elf_State *state, elf_Index index)
{
	elf_Value *value = value_at(state, index);
	if (!value || value_is_nil(*value) || state->next_ref == 0) return ELF_NO_REF;
	elf_Ref reference = state->next_ref++;
	elf_table_set(state, state->ref_table, int_value(reference), *value);
	return reference;
}

elf_Bool elf_push_ref(elf_State *state, elf_Ref reference)
{
	if (reference == ELF_NO_REF) return false;
	elf_Value key = int_value(reference);
	if (!elf_table_contains(state, state->ref_table, key)) return false;
	push_value(state, elf_table_get_or_nil(state, state->ref_table, key));
	return true;
}

elf_ErrorCode elf_release_ref(elf_State *state, elf_Ref reference)
{
	if (reference == ELF_NO_REF) return ELF_ERROR_INVALID_REFERENCE;
	if (!elf_table_delete(state, state->ref_table, int_value(reference), 0)) {
		return ELF_ERROR_INVALID_REFERENCE;
	}
	return ELF_ERROR_NONE;
}
