//
// See Copyright Notice In elf.h
//

#define RUNTIME_CHECK ASSERT

// reduce visual noise
typedef elf_Prototype Proto;
typedef elf_Value     Value;
typedef elf_State     State;
typedef StackFrame    Frame;


static inline void begin_proto(State *state, Proto *proto, u32 ndest, Value *base, u32 nargs)
{
	ASSERT(base->type == ELF_VALUE_TYPE_CLOSURE || base->type == ELF_VALUE_TYPE_PROTOTYPE);

	Frame *frame = state->frame;

	ASSERT(proto);
	// Note, must account for callee
	ASSERT(proto->stack_size >= 1);
	ASSERT(proto->arity <= proto->stack_size);
	ASSERT(ndest < 256);
	ASSERT(nargs < 256);

	// Todo, remove this wide zero
	zero_memory(frame, sizeof(* frame));

	frame->size = proto->stack_size;
	frame->base = base;
	frame->regs = base;
	frame->nargs = nargs;
	frame->ndest = ndest;
	frame->instr = proto->bytecode;

	if (proto->variadic && nargs > proto->arity)
	{
		/* the callee is also copied */
		frame->size += nargs + 1;
		frame->regs += nargs + 1;
		value_copy_many(frame->regs, frame->base, proto->arity + 1);
		value_zero_range(frame->regs + proto->arity + 1, frame->base + frame->size);
	}
	else
	{
		frame->nargs = MIN(frame->nargs, proto->arity);
		value_zero_range(frame->regs + frame->nargs + 1, frame->base + frame->size);
	}

	state->stack_top = frame->base + frame->size;
}

static u32 vm_resume(State *state);

u32 elf_call(State *state, u32 nargs)
{
	u32 nrets = 0;
	Value *base = state->stack_top - nargs - 1;

	state->frame ++;
	Frame *frame = state->frame;
	if (base->type == ELF_VALUE_TYPE_PROTOTYPE)
	{
		begin_proto(state, base->x_proto, 1, base, nargs);
		nrets = vm_resume(state);
	}
	else if (base->type == ELF_VALUE_TYPE_CLOSURE)
	{
		begin_proto(state, base->x_closure->prototype, 1, base, nargs);
		nrets = vm_resume(state);
	}
	else if (base->type == ELF_VALUE_TYPE_CFUNCTION)
	{
		zero_memory(frame, sizeof(*frame));
		frame->base = base;
		frame->regs = base;
		frame->nargs = nargs;
		frame->size = nargs + 1;
		frame->ndest = 1;
		nrets = base->x_function(state, nargs);
		ASSERT(nrets <= state->stack_top - frame->base - frame->size);
		state->stack_top -= nrets;
		value_move_many(base, state->stack_top, nrets);
	}
	else
	{
		RUNTIME_CHECK(!"Error, not callable!");
	}
	ASSERT(frame == state->frame);
	state->frame --;
	state->stack_top = base + nrets;
	return nrets;
}

static void vm_error_invalid_operands(State *state, BcType op, Value left, Value right)
{
	report_runtime_error(state, ERROR_TYPE_MISMATCH, "%s: invalid operands '%s' and '%s'",
	bytecode_type_name(op), value_type_name(left.type), value_type_name(right.type));
}

static void vm_error_cannot_call(State *state, Value value)
{
	report_runtime_error(state, ERROR_EXPECTED_CALLABLE, "cannot call '%s'", value_type_name(value.type));
}

static void vm_error_invalid_field_arguments(State *state, Value object, Value field)
{
	report_runtime_error(state, ERROR_TYPE_MISMATCH, "cannot get field '%s' from '%s'",
	value_type_name(field.type),
	value_type_name(object.type));
}

static inline u32 vm_check_index(State *state, Value value, u32 count)
{
	check_value_type(state, value, ELF_VALUE_TYPE_INTEGER);
	return check_array_index(state, value_as_integer(value), count);
}

static inline elf_String *vm_add_values_to_str(State *state, Value left, Value right)
{
	Scratch scratch = scratch_begin();
	char *join_start = arena_push(scratch.arena, 0, 1);
	elf_print_value(scratch.arena, left);
	elf_print_value(scratch.arena, right);
	char *join_end = arena_push_zero(scratch.arena, 1, 1);
	elf_String *string = elf_string_from_data_size(state, join_start, (u32)(join_end - join_start));
	scratch_end(scratch);
	return string;
}

static inline b32 vm_values_equal(Value left, Value right)
{
	if (value_is_numeric(left) && value_is_numeric(right)) {
		if (value_is_number(left) || value_is_number(right)) {
			return value_to_number(left) == value_to_number(right);
		}

		return value_as_integer(left) == value_as_integer(right);
	}

	if (value_type(left) != value_type(right)) {
		return false;
	}

	if (value_is_string(left)) {
		return elf_string_equal(value_as_string(left), value_as_string(right));
	}

	return value_as_integer(left) == value_as_integer(right);
}

static void vm_zero_range_safe(Value *first, Value *past_last) {
	if (first < past_last) {
		value_zero_range(first, past_last);
	}
}

static u32 vm_resume(State *state)
{
	Frame *root = state->frame;
	u32 nroot = 0;

	Frame *frame = state->frame;
	for (;;)
	{
		Bytecode *const instr = frame->instr;
		Bytecode byte = * frame->instr ++;

		switch (byte.b_type) {

			case BC_CALL_DYNAMIC:
			{
				ASSERT((u32)byte.b_x < frame_proto(frame)->stack_size);

				Value *dest = frame->regs + byte.b_x;
				u32 nargs = byte.b_y;
				u32 ndest = byte.b_z;

				if (dest->type == ELF_VALUE_TYPE_PROTOTYPE)
				{
					ASSERT(state->frame + 1 < state->frame_stack + state->frame_stack_size);
					Proto *proto = dest->x_proto;
					frame = ++ state->frame;
					begin_proto(state, proto, ndest, dest, nargs);
				}
				else if (dest->type == ELF_VALUE_TYPE_CLOSURE)
				{
					ASSERT(state->frame + 1 < state->frame_stack + state->frame_stack_size);
					Proto *proto = dest->x_closure->prototype;
					frame = ++ state->frame;
					begin_proto(state, proto, ndest, dest, nargs);
				}
				else if (dest->type == ELF_VALUE_TYPE_CFUNCTION)
				{
					ASSERT(state->frame + 1 < state->frame_stack + state->frame_stack_size);
					ASSERT(frame == state->frame);
					Frame *inner = ++ state->frame;
					zero_memory(inner, sizeof(*inner));
					inner->base = dest;
					inner->regs = dest;
					inner->nargs = nargs;
					inner->size = nargs + 1;
					inner->ndest = ndest;
					state->stack_top = inner->base + inner->size;
					u32 nrets = dest->x_function(state, nargs);
					ASSERT(state->stack_top >= inner->base + inner->size);
					RUNTIME_CHECK(nrets <= state->stack_top - inner->base - inner->size);
					state->frame --;
					ASSERT(frame == state->frame);
					Value *rets = state->stack_top - nrets;

					Value *next_top = frame->base + frame->size;
					value_move_many(dest, rets, MIN(ndest, nrets));
					vm_zero_range_safe(dest + MIN(ndest, nrets), next_top);
					state->stack_top = next_top;
				}
				else
				{
					RUNTIME_CHECK(!"Error: proper error reporting");
					// vm_error_cannot_call(state, callee);
				}
			}
			break;

			case BC_RETURN:
			{
				u32 nrets = byte.b_y;
				u32 rets = byte.b_x;

				if (frame == root) {
					value_move_many(frame->base, frame->regs + rets, nrets);
					nroot = nrets;
					goto exit;
				}

				nrets = MIN(nrets, frame->ndest);
				value_move_many(frame->base, frame->regs + rets, nrets);
				Value *next_top = state->frame[-1].base + state->frame[-1].size;
				vm_zero_range_safe(frame->base + nrets, next_top);
				state->stack_top = next_top;
				frame = -- state->frame;
			}
			break;

			case BC_JUMP:
			{
				frame->instr = instr + byte.b_x;
			}
			break;

			case BC_JUMP_IF_ZERO:
			{
				if (value_as_integer(frame->regs[byte.b_y]) == 0) {
					frame->instr = instr + byte.b_x;
				}
			}
			break;

			case BC_JUMP_IF_NONZERO:
			{
				if (value_as_integer(frame->regs[byte.b_y]) != 0) {
					frame->instr = instr + byte.b_x;
				}
			}
			break;

			case BC_LOAD_NIL:
			{
				frame->regs[byte.b_x] = value_nil();
			}
			break;

			case BC_MOVE:
			{
				value_copy(&frame->regs[byte.b_x], frame->regs[byte.b_y]);
			}
			break;

			case BC_LOAD_CONSTANT_INTEGER:
			{
				frame->regs[byte.b_x] = int_value(frame_proto(frame)->int_data[byte.b_y]);
			}
			break;

			case BC_LOAD_CONSTANT_NUMBER:
			{
				frame->regs[byte.b_x] = num_value(frame_proto(frame)->num_data[byte.b_y]);
			}
			break;

			case BC_LOAD_CONSTANT_STRING:
			{
				frame->regs[byte.b_x] = str_value(frame_proto(frame)->strings[byte.b_y]);
			}
			break;

			case BC_LOAD_CAPTURE:
			{
				ASSERT(byte.b_y >= 0 && byte.b_y < frame_proto(frame)->captures);
				ASSERT(frame->base->type == ELF_VALUE_TYPE_CLOSURE);
				value_copy(&frame->regs[byte.b_x], frame->base->x_closure->captures[byte.b_y]);
			}
			break;

			// TODO(RJ): deprecate this function, the compiler knows the current closure already!
			case BC_LOAD_CURRENT_CLOSURE:
			{
				ASSERT(frame->base->type == ELF_VALUE_TYPE_PROTOTYPE || frame->base->type == ELF_VALUE_TYPE_CLOSURE);
				frame->regs[byte.b_x] = * frame->base;
			}
			break;

			case BC_LOAD_NATIVE_FUNCTION: {
				if (byte.b_y < 0 || (u32)byte.b_y >= state->native_function_count)
				{
					ASSERT(!"native function index is out of bounds");
					break;
				}
				frame->regs[byte.b_x] = value_from_function(state->native_functions[(u32)byte.b_y]);
			} break;

			case BC_LOAD_PROTOTYPE: {
				ASSERT(byte.b_y >= 0);
				u32 prototype_index = (u32)byte.b_y;
				ASSERT(prototype_index_is_valid(state, prototype_index));
				frame->regs[byte.b_x] = proto_value(proto_from_index(state, prototype_index));
			} break;

			case BC_NEW_TABLE: {
				elf_Table *table = elf_table_new(state);
				frame->regs[byte.b_x] = value_from_table(table);
			} break;

			case BC_NEW_ARRAY: {
				frame->regs[byte.b_x] = value_from_array(elf_array_new(state));
			} break;

			case BC_NEW_CLOSURE: {
				ASSERT(byte.b_y >= 0);
				u32 prototype_index = (u32)byte.b_y;
				ASSERT(prototype_index_is_valid(state, prototype_index));
				Proto *prototype = proto_from_index(state, prototype_index);

				Value *result = &frame->regs[byte.b_x];
				elf_Closure *new_closure = elf_closure_new(state, prototype, result, prototype->captures);
				ASSERT(new_closure);
				*result = cls_value(new_closure);
			} break;

			case BC_GET_FIELD: {
				Value *result = &frame->regs[byte.b_x];
				Value object = frame->regs[byte.b_y];
				Value field = frame->regs[byte.b_z];

				if (value_is_nil(field)) {
					report_runtime_error(state, ERROR_INVALID_TABLE_KEY, "nil is not a valid field");
				}

				if (value_is_table(object))
				{
					*result = elf_table_get_or_nil(state, value_as_table(object), field);
				}
				else
				{
					vm_error_invalid_field_arguments(state, object, field);
				}
			} break;

			case BC_SET_FIELD: {
				Value table = frame->regs[byte.b_x];
				Value key = frame->regs[byte.b_y];
				Value value = frame->regs[byte.b_z];

				if (value_is_table(table)) {
					if (value_is_nil(key)) {
						report_runtime_error(state, ERROR_INVALID_TABLE_KEY, "key is nil...");
					}

					elf_table_set(state, value_as_table(table), key, value);
				}
				else if (value_is_user(table)) {
					report_runtime_error(state, ERROR_NOT_IMPLEMENTED, "overload not implemented");
				}
				else if (value_is_string(table)) {
					report_runtime_error(state, ERROR_READONLY, "strings are readonly, you may not change them");
				}
				else {
					report_runtime_error(state, ERROR_TYPE_MISMATCH, "attempted to set field of '%s' value", value_type_name(table.type));
				}
			} break;

			case BC_GET_INDEX: {
				Value object = frame->regs[byte.b_y];
				Value index_value = frame->regs[byte.b_z];
				Value *result = &frame->regs[byte.b_x];

				if (value_is_array(object)) {
					elf_Array *array = value_as_array(object);
					u32 index = vm_check_index(state, index_value, elf_array_length(array));
					value_copy(result, elf_array_get(state, array, index));
				}
				else if (value_is_table(object)) {
					if (value_is_nil(index_value))
					{
						report_runtime_error(state, ERROR_INVALID_TABLE_KEY, "nil is not a valid table key");
					}
					value_copy(result, elf_table_get_or_nil(state, value_as_table(object), index_value));
				}
				else if (value_is_string(object)) {
					elf_String *string = value_as_string(object);
					u32 index = vm_check_index(state, index_value, elf_string_size(string));
					*result = int_value((u8)elf_string_data(string)[index]);
				}
				else if (value_is_buffer(object)) {
					elf_ByteBuffer *buffer = value_as_buffer(object);
					u32 index = vm_check_index(state, index_value, buffer->size);
					*result = int_value(buffer->data[index]);
				}
				else {
					report_runtime_error(state, ERROR_TYPE_MISMATCH,
					"cannot index '%s' value", value_type_name(object.type));
				}
			} break;

			case BC_SET_INDEX: {
				Value object = frame->regs[byte.b_x];
				Value index_value = frame->regs[byte.b_y];
				Value assigned = frame->regs[byte.b_z];
				if (value_is_array(object))
				{
					elf_Array *array = value_as_array(object);
					u32 index = vm_check_index(state, index_value, elf_array_length(array));
					elf_array_set(state, array, index, assigned);
				}
				else if (value_is_table(object))
				{
					if (value_is_nil(index_value))
					{
						report_runtime_error(state, ERROR_INVALID_TABLE_KEY, "nil is not a valid table key");
					}
					elf_table_set(state, value_as_table(object), index_value, assigned);
				}
				else if (value_is_buffer(object))
				{
					elf_ByteBuffer *buffer = value_as_buffer(object);
					u32 index = vm_check_index(state, index_value, buffer->size);
					check_value_type(state, assigned, ELF_VALUE_TYPE_INTEGER);
					i64 value = value_as_integer(assigned);
					if (value < 0 || value > UINT8_MAX)
					{
						report_runtime_error(state, ERROR_OUT_OF_RANGE,
						"buffer byte is out of range: %lld", value);
					}
					elf_buffer_require_writable(state, buffer);
					buffer->data[index] = (u8)value;
				}
				else
				{
					report_runtime_error(state, ERROR_TYPE_MISMATCH,
					"cannot assign through index of '%s' value", value_type_name(object.type));
				}
			} break;

			case BC_ARRAY_APPEND: {
				Value array = frame->regs[byte.b_x];
				check_value_type(state, array, ELF_VALUE_TYPE_ARRAY);
				elf_array_add(state, value_as_array(array), frame->regs[byte.b_y]);
			} break;

			case BC_MEASURE: {
				Value value = frame->regs[byte.b_y];
				u32 measure = 0;
				if (value_measure(value, &measure))
				{
					frame->regs[byte.b_x] = int_value(measure);
				}
				else
				{
					report_runtime_error(state, ERROR_TYPE_MISMATCH, "cannot measure '%s' value", value_type_name(value.type));
				}
			} break;

			case BC_GET_META_FIELD: {
				Value value = frame->regs[byte.b_y];
				elf_Table *metatable = elf_get_type_metatable(state, value);

				if (!metatable) {
					report_runtime_error(state, ERROR_MISSING_METATABLE, "'%s': required metatable for metafield lookup", value_type_name(value.type));
				}

				value_copy(&frame->regs[byte.b_x], elf_table_get_or_nil(state, metatable, frame->regs[byte.b_z]));
			} break;

			case BC_CHECK_TYPE: {
				check_value_type_rule(state, frame->regs[byte.b_x], byte.b_y);
			} break;

			case BC_EQ: {
				frame->regs[byte.b_x] = int_value(vm_values_equal(frame->regs[byte.b_y], frame->regs[byte.b_z]));
			} break;

			case BC_NOT_EQ: {
				frame->regs[byte.b_x] = int_value(!vm_values_equal(frame->regs[byte.b_y], frame->regs[byte.b_z]));
			} break;

			case BC_LESS_THAN: {
				Value left = frame->regs[byte.b_y];
				Value right = frame->regs[byte.b_z];
				if (value_is_numeric(left) && value_is_numeric(right)) {
					if (value_is_number(left) || value_is_number(right)) {
						frame->regs[byte.b_x] = int_value(value_to_number(left) < value_to_number(right));
					}
					else {
						frame->regs[byte.b_x] = int_value(value_as_integer(left) < value_as_integer(right));
					}
				}
				else {
					vm_error_invalid_operands(state, BC_LESS_THAN, left, right);
				}
			} break;

			case BC_LESS_THAN_EQ: {
				Value left = frame->regs[byte.b_y];
				Value right = frame->regs[byte.b_z];
				if (value_is_numeric(left) && value_is_numeric(right)) {
					if (value_is_number(left) || value_is_number(right)) {
						frame->regs[byte.b_x] = int_value(value_to_number(left) <= value_to_number(right));
					}
					else {
						frame->regs[byte.b_x] = int_value(value_as_integer(left) <= value_as_integer(right));
					}
				}
				else {
					vm_error_invalid_operands(state, BC_LESS_THAN_EQ, left, right);
				}
			} break;

			case BC_ADD:
			{
				Value left = frame->regs[byte.b_y];
				Value right = frame->regs[byte.b_z];

				if (value_is_string(left))
				{
					frame->regs[byte.b_x] = str_value(vm_add_values_to_str(state, left, right));
				}
				else if (value_is_numeric(left) && value_is_numeric(right))
				{
					if (value_is_number(left) || value_is_number(right)) {
						frame->regs[byte.b_x] = num_value(value_to_number(left) + value_to_number(right));
					}
					else {
						frame->regs[byte.b_x] = int_value(value_as_integer(left) + value_as_integer(right));
					}
				}
				else
				{
					vm_error_invalid_operands(state, BC_ADD, left, right);
				}
			}
			break;

			case BC_SUB: {
				Value left = frame->regs[byte.b_y];
				Value right = frame->regs[byte.b_z];
				if (value_is_numeric(left) && value_is_numeric(right)) {
					if (value_is_number(left) || value_is_number(right)) {
						frame->regs[byte.b_x] = num_value(value_to_number(left) - value_to_number(right));
					}
					else {
						frame->regs[byte.b_x] = int_value(value_as_integer(left) - value_as_integer(right));
					}
				}
				else {
					vm_error_invalid_operands(state, BC_SUB, left, right);
				}
			} break;

			case BC_MUL: {
				Value left = frame->regs[byte.b_y];
				Value right = frame->regs[byte.b_z];
				if (value_is_numeric(left) && value_is_numeric(right)) {
					if (value_is_number(left) || value_is_number(right)) {
						frame->regs[byte.b_x] = num_value(value_to_number(left) * value_to_number(right));
					}
					else {
						frame->regs[byte.b_x] = int_value(value_as_integer(left) * value_as_integer(right));
					}
				}
				else {
					vm_error_invalid_operands(state, BC_MUL, left, right);
				}
			} break;

			case BC_DIV:
			{
				Value left = frame->regs[byte.b_y];
				Value right = frame->regs[byte.b_z];
				if (value_is_numeric(left) && value_is_numeric(right)) {
					if (value_is_number(left) || value_is_number(right))
					{
						frame->regs[byte.b_x] = num_value(value_to_number(left) / value_to_number(right));
					}
					else
					{
						i64 dividend = value_as_integer(left);
						i64 divisor = value_as_integer(right);
						if (divisor == 0) {
							report_runtime_error(state, ERROR_DIVISION_BY_ZERO, "integer division by zero");
						}
						if (dividend == INT64_MIN && divisor == -1) {
							report_runtime_error(state, ERROR_OUT_OF_RANGE, "integer division overflow: minimum integer divided by -1");
						}
						frame->regs[byte.b_x] = int_value(dividend / divisor);
					}
				}
				else {
					vm_error_invalid_operands(state, BC_DIV, left, right);
				}
			} break;

			case BC_MOD: {
				Value left = frame->regs[byte.b_y];
				Value right = frame->regs[byte.b_z];
				if (value_is_numeric(left) && value_is_numeric(right)) {
					if (value_is_number(left) || value_is_number(right)) {
						f64 left_number = value_to_number(left);
						f64 right_number = value_to_number(right);
						frame->regs[byte.b_x] = num_value(left_number - ((i64)(left_number / right_number)) * right_number);
					}
					else {
						frame->regs[byte.b_x] = int_value(value_as_integer(left) % value_as_integer(right));
					}
				}
				else {
					vm_error_invalid_operands(state, BC_MOD, left, right);
				}
			} break;

			case BC_POW: {
				Value left = frame->regs[byte.b_y];
				Value right = frame->regs[byte.b_z];
				if (value_is_numeric(left) && value_is_numeric(right)) {
					if (value_is_number(left) || value_is_number(right)) {
						frame->regs[byte.b_x] = num_value(pow(value_to_number(left), value_to_number(right)));
					}
					else {
						frame->regs[byte.b_x] = int_value((i64)pow(value_as_integer(left), value_as_integer(right)));
					}
				}
				else {
					vm_error_invalid_operands(state, BC_POW, left, right);
				}
			} break;

			case BC_BITWISE_NOT: {
				Value value = frame->regs[byte.b_y];
				check_value_type(state, value, ELF_VALUE_TYPE_INTEGER);
				frame->regs[byte.b_x] = int_value(~value_as_integer(value));
			} break;

			case BC_BITWISE_AND: {
				Value left = frame->regs[byte.b_y];
				Value right = frame->regs[byte.b_z];
				if (value_is_integer(left) && value_is_integer(right)) {
					frame->regs[byte.b_x] = int_value(value_as_integer(left) & value_as_integer(right));
				}
				else {
					vm_error_invalid_operands(state, BC_BITWISE_AND, left, right);
				}
			} break;

			case BC_BITWISE_OR: {
				Value left = frame->regs[byte.b_y];
				Value right = frame->regs[byte.b_z];
				if (value_is_integer(left) && value_is_integer(right)) {
					frame->regs[byte.b_x] = int_value(value_as_integer(left) | value_as_integer(right));
				}
				else {
					vm_error_invalid_operands(state, BC_BITWISE_OR, left, right);
				}
			} break;

			case BC_BITWISE_XOR: {
				Value left = frame->regs[byte.b_y];
				Value right = frame->regs[byte.b_z];
				if (value_is_integer(left) && value_is_integer(right)) {
					frame->regs[byte.b_x] = int_value(value_as_integer(left) ^ value_as_integer(right));
				}
				else {
					vm_error_invalid_operands(state, BC_BITWISE_XOR, left, right);
				}
			} break;

			case BC_SHIFT_LEFT: {
				Value left = frame->regs[byte.b_y];
				Value right = frame->regs[byte.b_z];
				if (value_is_integer(left) && value_is_integer(right)) {
					frame->regs[byte.b_x] = int_value(value_as_integer(left) << value_as_integer(right));
				}
				else {
					vm_error_invalid_operands(state, BC_SHIFT_LEFT, left, right);
				}
			} break;

			case BC_SHIFT_RIGHT: {
				Value left = frame->regs[byte.b_y];
				Value right = frame->regs[byte.b_z];
				if (value_is_integer(left) && value_is_integer(right)) {
					frame->regs[byte.b_x] = int_value(value_as_integer(left) >> value_as_integer(right));
				}
				else {
					vm_error_invalid_operands(state, BC_SHIFT_RIGHT, left, right);
				}
			} break;

			default: {
				report_runtime_error(state, ERROR_UNKNOWN_BYTECODE, "'%s' unknown bytecode", bytecode_type_name(byte.b_type));
			} break;
		}
	}

	exit:
	return nroot;
}

