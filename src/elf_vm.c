//
// See Copyright Notice In elf.h
//

#define RUNTIME_CHECK ASSERT

// reduce visual noise
typedef elf_Prototype  Proto;
typedef elf_Value      Value;
typedef elf_ValueType  ValueType;
typedef elf_State      State;
typedef StackFrame     Frame;
typedef elf_Array      Array;
typedef elf_Table      Table;
typedef elf_String     String;
typedef elf_ByteBuffer Buffer;

// TODO(RJ): make this a helper
// TODO: crashes | formatting or allocating the error can still terminate while handling a recoverable VM failure.
static void vm_push_errorf(elf_State *state, elf_ErrorCode code, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	elf_GCError *error = elf_error_newfv(state, code, 0, format, args);
	va_end(args);
	push_value(state, value_from_error(error));
}

static void vm_error_invalid_operands(State *state, BcType op, Value left, Value right)
{
	vm_push_errorf(state, ELF_ERROR_TYPE_MISMATCH, "%s: invalid operands '%s' and '%s'", bytecode_type_name(op), value_type_name(left.type), value_type_name(right.type));
}

static void vm_error_cannot_call(State *state, Value value)
{
	vm_push_errorf(state, ELF_ERROR_EXPECTED_CALLABLE, "cannot call '%s'", value_type_name(value.type));
}

static inline b32 vm_check_value_type(State *state, Value value, ValueType type)
{
	if (value.type != type) {
		vm_push_errorf(state, ELF_ERROR_TYPE_MISMATCH, "type error, expected '%s', got '%s'", value_type_name(type), value_type_name(value.type));
	}
	return value.type == type;
}

#define INVALID_INDEX UINT64_MAX

static inline u64 vm_check_index(State *state, Value value, u32 count)
{
	if (!vm_check_value_type(state, value, ELF_VALUE_TYPE_INTEGER)) {
		return INVALID_INDEX;
	}

	i64 index = value_as_integer(value);

	i64 mapped = index < 0 ? index + count : index;

	if (mapped < 0 || mapped >= count)
	{
		vm_push_errorf(state, ELF_ERROR_OUT_OF_RANGE, "array index out of bounds: index %lli mapped to %lli, length %u", index, mapped, count);
		return INVALID_INDEX;
	}

	return (u64) mapped;
}

static inline elf_String *vm_add_values_to_str(State *state, Value left, Value right)
{
	// TODO: crashes | value formatting and string allocation do not propagate allocation failure yet.
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

	if (value_is_nil(left)) {
		return true;
	}

	if (value_is_function(left)) {
		return value_as_function(left) == value_as_function(right);
	}

	if (value_is_prototype(left)) {
		return value_as_prototype(left) == value_as_prototype(right);
	}

	return value_as_object(left) == value_as_object(right);
}

static void vm_zero_range_safe(Value *first, Value *past_last) {
	if (first < past_last) {
		value_zero_range(first, past_last);
	}
}

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

	// TODO: crashes | validate that the complete frame fits in the value stack before copying or clearing registers.
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
	// TODO: crashes | validate nargs against the active stack before deriving base.
	Value *base = state->stack_top - nargs - 1;

	// TODO: crashes | every callable kind can exhaust the fixed frame stack.
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
		// TODO: crashes | a native function can report a result count that is inconsistent with the stack.
		ASSERT(nrets <= state->stack_top - frame->base - frame->size);
	}
	else
	{
		vm_error_cannot_call(state, * base);
		nrets = 1;
	}

	state->stack_top -= nrets;
	value_move_many(base, state->stack_top, nrets);
	ASSERT(frame == state->frame);
	state->frame --;
	state->stack_top = base + nrets;
	return nrets;
}

#define INVALID_RETURN_COUNT UINT32_MAX
static u32 vm_resume(State *state)
{
	Frame *root = state->frame;
	Frame *frame = state->frame;

	u32 nroot = INVALID_RETURN_COUNT;

	for (;;)
	{
		Bytecode *const instr = frame->instr;
		Bytecode byte = * frame->instr ++;

		switch (byte.b_type)
		{
			case BC_CHECK_TYPE:
			{
				Value value = frame->regs[byte.b_x];
				TypeRule rule = byte.b_y;
				if (~ rule & 1 << value.type)
				{
					vm_push_errorf(state, ELF_ERROR_TYPE_MISMATCH, "type error, expected '%s', got '%s'", type_rule_name(rule), value_type_name(value.type));
					goto exit_error;
				}
			}
			break;

			case BC_CALL_DYNAMIC:
			{
				ASSERT((u32)byte.b_x < frame_proto(frame)->stack_size);

				Value *dest = frame->regs + byte.b_x;
				u32 nargs = byte.b_y;
				u32 ndest = byte.b_z;

				// TODO: crashes | script recursion can exhaust the fixed frame stack.
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

					frame = ++ state->frame;
					zero_memory(frame, sizeof(*frame));
					frame->base = dest;
					frame->regs = dest;
					frame->nargs = nargs;
					frame->size = nargs + 1;
					frame->ndest = ndest;
					state->stack_top = frame->base + frame->size;
					u32 nrets = dest->x_function(state, nargs);
					// TODO: crashes | validate native result counts without relying on pointer arithmetic or an assertion.
					RUNTIME_CHECK(state->stack_top >= frame->base + frame->size + nrets);

					nroot = nrets;
					goto finish_frame;
				}
				else
				{
					vm_error_cannot_call(state, * dest);
					goto exit_error;
				}
			}
			break;

			case BC_RETURN:
			{
				u32 irets = byte.b_x;
				nroot = byte.b_y;
				state->stack_top = frame->regs + irets + nroot;

				if (frame == root) {
					goto exit;
				}

				//
				// (stack_top, nroot):
				//	first_return = stack_top - nroot
				//
				// ** stack_top must be one past the last result and nroot must be
				// set to the number of results. **
				//
				finish_frame:

				// TODO: error propagation | a native or bytecode function can return an error here and execution currently resumes in its caller.
				Value *vrets = state->stack_top - nroot;
				u32 nrets = MIN(nroot, frame->ndest);
				value_move_many(frame->base, vrets, nrets);
				state->stack_top = frame->base + nrets;

				frame = -- state->frame;
				vm_zero_range_safe(state->stack_top, frame->base + frame->size);
				state->stack_top = frame->base + frame->size;

				nroot = INVALID_RETURN_COUNT;
			}
			break;

			case BC_JUMP:
			{
				frame->instr = instr + byte.b_x;
			}
			break;

			//	Note, no object value is allowed to have zero payload, it must instead
			//	become 'nil'.
			//	Zero payload should only be possible for nil, and the literal zero integer,
			// which is what we want ...
			case BC_JUMP_IF_ZERO:
			{
				if (value_is_number(frame->regs[byte.b_y])) {
					vm_push_errorf(state, ELF_ERROR_TYPE_MISMATCH, "cannot branch on 'number'");
					goto exit_error;
				}

				if (frame->regs[byte.b_y].x_int == 0) {
					frame->instr = instr + byte.b_x;
				}
			}
			break;

			case BC_JUMP_IF_NONZERO:
			{
				if (value_is_number(frame->regs[byte.b_y])) {
					vm_push_errorf(state, ELF_ERROR_TYPE_MISMATCH, "cannot branch on 'number'");
					goto exit_error;
				}

				if (frame->regs[byte.b_y].x_int != 0) {
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
				frame->regs[byte.b_x] = frame->regs[byte.b_y];
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
				frame->regs[byte.b_x] = frame->base->x_closure->captures[byte.b_y];
			}
			break;

			case BC_LOAD_NATIVE_FUNCTION:
			{
				frame->regs[byte.b_x] = value_from_function(state->native_functions[byte.b_y]);
			}
			break;

			case BC_LOAD_PROTOTYPE:
			{
				frame->regs[byte.b_x] = proto_value(proto_from_index(state, byte.b_y));
			}
			break;

			case BC_NEW_TABLE:
			{
				// TODO: crashes | object and table-storage allocation failures still assert.
				frame->regs[byte.b_x] = value_from_table(elf_table_new(state));
			}
			break;

			case BC_NEW_ARRAY:
			{
				// TODO: crashes | object allocation failure still asserts.
				frame->regs[byte.b_x] = value_from_array(elf_array_new(state));
			}
			break;

			case BC_NEW_CLOSURE:
			{
				Proto *proto = proto_from_index(state, byte.b_y);
				// TODO: crashes | closure allocation failure still asserts.
				frame->regs[byte.b_x] = cls_value(elf_closure_new(state, proto, frame->regs + byte.b_x, proto->captures));
			}
			break;

			case BC_GET_FIELD:
			{
				Value object = frame->regs[byte.b_y];
				Value field = frame->regs[byte.b_z];

				if (value_is_table(object))
				{
					// Todo: this is a good optimization opportunity, the compiler already knows
					// that the field isn't nil for most field accesses ... we need a dedicated
					// BC_GET_KFIELD or something, that directly references an atom or something.
					if (value_is_nil(field))
					{
						vm_push_errorf(state, ELF_ERROR_INVALID_TABLE_KEY, "'nil' is not a valid field");
						goto exit_error;
					}

					// Todo: when the compiler knows the type, we create a new instruction here,
					// that just jumps here without doing the type branching ...
					frame->regs[byte.b_x] = elf_table_get_or_nil(state, value_as_table(object), field);
				}
				else
				{
					vm_push_errorf(state, ELF_ERROR_TYPE_MISMATCH, "cannot field access '%s' value", value_type_name(object.type));
					goto exit_error;
				}
			}
			break;

			case BC_SET_FIELD:
			{
				Value object = frame->regs[byte.b_x];
				Value field = frame->regs[byte.b_y];
				Value value = frame->regs[byte.b_z];

				if (value_is_object(object)) {
					if (object.x_obj->status & ELF_OBJECT_READONLY) {
						vm_push_errorf(state, ELF_ERROR_READONLY, "cannot modify readonly '%s' object", value_type_name(object.type));
						goto exit_error;
					}
				}

				if (value_is_table(object))
				{
					if (value_is_nil(field))
					{
						vm_push_errorf(state, ELF_ERROR_INVALID_TABLE_KEY, "'nil' is not a valid field");
						goto exit_error;
					}

					// TODO: crashes | table growth still asserts on allocation failure.
					elf_table_set(state, value_as_table(object), field, value);
				}
				else
				{
					vm_push_errorf(state, ELF_ERROR_TYPE_MISMATCH, "cannot field access '%s' value", value_type_name(object.type));
					goto exit_error;
				}
			}
			break;

			case BC_GET_INDEX:
			{
				Value object = frame->regs[byte.b_y];
				Value index_value = frame->regs[byte.b_z];
				Value *result = &frame->regs[byte.b_x];

				if (value_is_array(object))
				{
					Array *array = value_as_array(object);

					u64 index = vm_check_index(state, index_value, array->count);
					if (index == INVALID_INDEX) {
						goto exit_error;
					}

					*result = array->items[index];
				}
				else if (value_is_table(object))
				{
					if (value_is_nil(index_value))
					{
						vm_push_errorf(state, ELF_ERROR_INVALID_TABLE_KEY, "'nil' is not a valid field");
						goto exit_error;
					}

					*result = elf_table_get_or_nil(state, value_as_table(object), index_value);
				}
				else if (value_is_string(object))
				{
					String *string = value_as_string(object);

					u64 index = vm_check_index(state, index_value, elf_string_size(string));
					if (index == INVALID_INDEX) {
						goto exit_error;
					}

					*result = int_value((u8) string->data[index]);
				}
				else if (value_is_buffer(object))
				{
					Buffer *buffer = value_as_buffer(object);

					u64 index = vm_check_index(state, index_value, buffer->size);
					if (index == INVALID_INDEX) {
						goto exit_error;
					}

					*result = int_value(buffer->data[index]);
				}
				else
				{
					vm_push_errorf(state, ELF_ERROR_TYPE_MISMATCH, "cannot index '%s' value", value_type_name(object.type));
					goto exit_error;
				}
			} break;

			case BC_SET_INDEX:
			{
				Value object = frame->regs[byte.b_x];
				Value index_value = frame->regs[byte.b_y];
				Value value = frame->regs[byte.b_z];


				if (value_is_object(object)) {
					if (object.x_obj->status & ELF_OBJECT_READONLY) {
						vm_push_errorf(state, ELF_ERROR_READONLY, "cannot modify readonly '%s' object", value_type_name(object.type));
						goto exit_error;
					}
				}

				if (value_is_array(object))
				{
					Array *array = value_as_array(object);

					u64 index = vm_check_index(state, index_value, array->count);
					if (index == INVALID_INDEX) {
						goto exit_error;
					}

					array->items[index] = value;
				}
				else if (value_is_table(object))
				{
					if (value_is_nil(index_value))
					{
						vm_push_errorf(state, ELF_ERROR_INVALID_TABLE_KEY, "'nil' is not a valid field");
						goto exit_error;
					}

					// TODO: crashes | table growth still asserts on allocation failure.
					elf_table_set(state, value_as_table(object), index_value, value);
				}
				else if (value_is_buffer(object))
				{
					Buffer *buffer = value_as_buffer(object);

					u64 index = vm_check_index(state, index_value, buffer->size);
					if (index == INVALID_INDEX) {
						goto exit_error;
					}

					if (!vm_check_value_type(state, value, ELF_VALUE_TYPE_INTEGER)) {
						goto exit_error;
					}

					i64 integer_value = value_as_integer(value);
					if ((u8) integer_value != integer_value)
					{
						vm_push_errorf(state, ELF_ERROR_OUT_OF_RANGE, "buffer byte is out of range: %lld", integer_value);
						goto exit_error;
					}

					buffer->data[index] = (u8) integer_value;
				}
				else
				{
					vm_push_errorf(state, ELF_ERROR_TYPE_MISMATCH, "cannot index '%s' value", value_type_name(object.type));
					goto exit_error;
				}

			} break;

			case BC_ARRAY_APPEND:
			{
				Value array = frame->regs[byte.b_x];

				if (!vm_check_value_type(state, array, ELF_VALUE_TYPE_ARRAY)) {
					goto exit_error;
				}

				// TODO: crashes | readonly arrays and allocation failure still terminate inside elf_array_add.
				elf_array_add(state, value_as_array(array), frame->regs[byte.b_y]);
			}
			break;

			case BC_MEASURE:
			{
				Value value = frame->regs[byte.b_y];
				u32 measure = 0;
				if (value_measure(value, &measure))
				{
					frame->regs[byte.b_x] = int_value(measure);
				}
				else
				{
					vm_push_errorf(state, ELF_ERROR_TYPE_MISMATCH, "cannot measure '%s' value", value_type_name(value.type));
					goto exit_error;
				}
			}
			break;

			// Todo, this instruction is deprecated, waiting to add proper types to remove
			// this path entirely
			case BC_GET_META_FIELD:
			{
				Value value = frame->regs[byte.b_y];
				elf_Table *metatable = elf_get_type_metatable(state, value);

				if (!metatable)
				{
					vm_push_errorf(state, ELF_ERROR_MISSING_METATABLE, "'%s': required metatable for metafield lookup", value_type_name(value.type));
					goto exit_error;
				}

				frame->regs[byte.b_x] = elf_table_get_or_nil(state, metatable, frame->regs[byte.b_z]);
			}
			break;


			case BC_EQ:
			{
				frame->regs[byte.b_x] = int_value(vm_values_equal(frame->regs[byte.b_y], frame->regs[byte.b_z]));
			}
			break;

			case BC_NOT_EQ:
			{
				frame->regs[byte.b_x] = int_value(!vm_values_equal(frame->regs[byte.b_y], frame->regs[byte.b_z]));
			}
			break;

			case BC_LESS_THAN:
			{
				Value x = frame->regs[byte.b_y];
				Value y = frame->regs[byte.b_z];

				if (value_is_numeric(x) && value_is_numeric(y))
				{
					if (value_is_number(x) || value_is_number(y))
					{
						frame->regs[byte.b_x] = int_value(value_to_number(x) < value_to_number(y));
					}
					else
					{
						frame->regs[byte.b_x] = int_value(value_as_integer(x) < value_as_integer(y));
					}
				}
				else
				{
					vm_error_invalid_operands(state, BC_LESS_THAN, x, y);
					goto exit_error;
				}
			}
			break;

			case BC_LESS_THAN_EQ:
			{
				Value x = frame->regs[byte.b_y];
				Value y = frame->regs[byte.b_z];

				if (value_is_numeric(x) && value_is_numeric(y))
				{
					if (value_is_number(x) || value_is_number(y))
					{
						frame->regs[byte.b_x] = int_value(value_to_number(x) <= value_to_number(y));
					}
					else
					{
						frame->regs[byte.b_x] = int_value(value_as_integer(x) <= value_as_integer(y));
					}
				}
				else
				{
					vm_error_invalid_operands(state, BC_LESS_THAN_EQ, x, y);
					goto exit_error;
				}
			}
			break;

			case BC_ADD:
			{
				Value x = frame->regs[byte.b_y];
				Value y = frame->regs[byte.b_z];

				if (value_is_string(x))
				{
					frame->regs[byte.b_x] = str_value(vm_add_values_to_str(state, x, y));
				}
				else if (value_is_numeric(x) && value_is_numeric(y))
				{
					if (value_is_number(x) || value_is_number(y))
					{
						frame->regs[byte.b_x] = num_value(value_to_number(x) + value_to_number(y));
					}
					else
					{
						// TODO: undefined behavior | detect signed integer overflow.
						frame->regs[byte.b_x] = int_value(value_as_integer(x) + value_as_integer(y));
					}
				}
				else
				{
					vm_error_invalid_operands(state, BC_ADD, x, y);
					goto exit_error;
				}
			}
			break;

			case BC_SUB:
			{
				Value x = frame->regs[byte.b_y];
				Value y = frame->regs[byte.b_z];

				if (value_is_numeric(x) && value_is_numeric(y))
				{
					if (value_is_number(x) || value_is_number(y))
					{
						frame->regs[byte.b_x] = num_value(value_to_number(x) - value_to_number(y));
					}
					else
					{
						// TODO: undefined behavior | detect signed integer overflow.
						frame->regs[byte.b_x] = int_value(value_as_integer(x) - value_as_integer(y));
					}
				}
				else
				{
					vm_error_invalid_operands(state, BC_SUB, x, y);
					goto exit_error;
				}
			}
			break;

			case BC_MUL:
			{
				Value x = frame->regs[byte.b_y];
				Value y = frame->regs[byte.b_z];

				if (value_is_numeric(x) && value_is_numeric(y))
				{
					if (value_is_number(x) || value_is_number(y))
					{
						frame->regs[byte.b_x] = num_value(value_to_number(x) * value_to_number(y));
					}
					else
					{
						// TODO: undefined behavior | detect signed integer overflow.
						frame->regs[byte.b_x] = int_value(value_as_integer(x) * value_as_integer(y));
					}
				}
				else
				{
					vm_error_invalid_operands(state, BC_MUL, x, y);
					goto exit_error;
				}
			}
			break;

			case BC_DIV:
			{
				Value x = frame->regs[byte.b_y];
				Value y = frame->regs[byte.b_z];

				if (value_is_numeric(x) && value_is_numeric(y))
				{
					if (value_is_number(x) || value_is_number(y))
					{
						frame->regs[byte.b_x] = num_value(value_to_number(x) / value_to_number(y));
					}
					else
					{
						i64 ix = value_as_integer(x);
						i64 iy = value_as_integer(y);
						if (iy == 0)
						{
							vm_push_errorf(state, ELF_ERROR_DIVISION_BY_ZERO, "integer division by zero");
							goto exit_error;
						}
						if (ix == INT64_MIN && iy == -1)
						{
							vm_push_errorf(state, ELF_ERROR_OUT_OF_RANGE, "integer division overflow: minimum integer divided by -1");
							goto exit_error;
						}
						frame->regs[byte.b_x] = int_value(ix / iy);
					}
				}
				else
				{
					vm_error_invalid_operands(state, BC_DIV, x, y);
					goto exit_error;
				}
			}
			break;

			case BC_MOD:
			{
				Value x = frame->regs[byte.b_y];
				Value y = frame->regs[byte.b_z];

				if (value_is_numeric(x) && value_is_numeric(y))
				{
					if (value_is_number(x) || value_is_number(y))
					{
						f64 nx = value_to_number(x);
						f64 ny = value_to_number(y);
						// TODO: undefined behavior | define zero, NaN, and out-of-range quotient semantics without converting the quotient to i64.
						frame->regs[byte.b_x] = num_value(nx - ((i64)(nx / ny)) * ny);
					}
					else
					{
						i64 ix = value_as_integer(x);
						i64 iy = value_as_integer(y);
						if (iy == 0)
						{
							vm_push_errorf(state, ELF_ERROR_DIVISION_BY_ZERO, "integer modulo by zero");
							goto exit_error;
						}
						frame->regs[byte.b_x] = int_value(ix == INT64_MIN && iy == -1 ? 0 : ix % iy);
					}
				}
				else
				{
					vm_error_invalid_operands(state, BC_MOD, x, y);
					goto exit_error;
				}
			}
			break;

			case BC_POW:
			{
				Value x = frame->regs[byte.b_y];
				Value y = frame->regs[byte.b_z];

				if (value_is_numeric(x) && value_is_numeric(y))
				{
					if (value_is_number(x) || value_is_number(y))
					{
						frame->regs[byte.b_x] = num_value(pow(value_to_number(x), value_to_number(y)));
					}
					else
					{
						// TODO: undefined behavior | pow can produce a value outside the i64 range.
						frame->regs[byte.b_x] = int_value((i64)pow(value_as_integer(x), value_as_integer(y)));
					}
				}
				else
				{
					vm_error_invalid_operands(state, BC_POW, x, y);
					goto exit_error;
				}
			}
			break;

			case BC_BITWISE_NOT:
			{
				Value x = frame->regs[byte.b_y];
				if (!value_is_integer(x))
				{
					vm_push_errorf(state, ELF_ERROR_TYPE_MISMATCH, "%s: invalid operand '%s'", bytecode_type_name(BC_BITWISE_NOT), value_type_name(x.type));
					goto exit_error;
				}
				frame->regs[byte.b_x] = int_value(~value_as_integer(x));
			}
			break;

			case BC_BITWISE_AND:
			{
				Value x = frame->regs[byte.b_y];
				Value y = frame->regs[byte.b_z];
				if (value_is_integer(x) && value_is_integer(y))
				{
					frame->regs[byte.b_x] = int_value(value_as_integer(x) & value_as_integer(y));
				}
				else
				{
					vm_error_invalid_operands(state, BC_BITWISE_AND, x, y);
					goto exit_error;
				}
			}
			break;

			case BC_BITWISE_OR:
			{
				Value x = frame->regs[byte.b_y];
				Value y = frame->regs[byte.b_z];
				if (value_is_integer(x) && value_is_integer(y))
				{
					frame->regs[byte.b_x] = int_value(value_as_integer(x) | value_as_integer(y));
				}
				else
				{
					vm_error_invalid_operands(state, BC_BITWISE_OR, x, y);
					goto exit_error;
				}
			}
			break;

			case BC_BITWISE_XOR:
			{
				Value x = frame->regs[byte.b_y];
				Value y = frame->regs[byte.b_z];
				if (value_is_integer(x) && value_is_integer(y))
				{
					frame->regs[byte.b_x] = int_value(value_as_integer(x) ^ value_as_integer(y));
				}
				else
				{
					vm_error_invalid_operands(state, BC_BITWISE_XOR, x, y);
					goto exit_error;
				}
			}
			break;

			case BC_SHIFT_LEFT:
			{
				Value x = frame->regs[byte.b_y];
				Value y = frame->regs[byte.b_z];
				if (value_is_integer(x) && value_is_integer(y))
				{
					// TODO: undefined behavior | validate the shift count and define signed left-shift overflow.
					frame->regs[byte.b_x] = int_value(value_as_integer(x) << value_as_integer(y));
				}
				else
				{
					vm_error_invalid_operands(state, BC_SHIFT_LEFT, x, y);
					goto exit_error;
				}
			}
			break;

			case BC_SHIFT_RIGHT:
			{
				Value x = frame->regs[byte.b_y];
				Value y = frame->regs[byte.b_z];
				if (value_is_integer(x) && value_is_integer(y))
				{
					// TODO: undefined behavior | validate the shift count and define right shift of negative values.
					frame->regs[byte.b_x] = int_value(value_as_integer(x) >> value_as_integer(y));
				}
				else
				{
					vm_error_invalid_operands(state, BC_SHIFT_RIGHT, x, y);
					goto exit_error;
				}
			}
			break;

			default:
			{
				vm_push_errorf(state, ELF_ERROR_UNKNOWN_BYTECODE, "'%s' unknown bytecode", bytecode_type_name(byte.b_type));
				goto exit_error;
			}
			break;
		}
	}

	RUNTIME_CHECK(!"Internal error, this place is unreachable");


	/* exit_error(stack_top[-1] == error) */
	exit_error:
	{
		state->frame = root;
		Value error = state->stack_top[-1];
		ASSERT(error.type == ELF_VALUE_TYPE_ERROR);
		state->stack_top = root->base;
		* state->stack_top ++ = error;
		nroot = 1;
	}

	exit:

	ASSERT(nroot < 256);
	return nroot;
}

