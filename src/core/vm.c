//
// See Copyright Notice In elf.h
//

static void vm_error_invalid_operands(elf_State *state, BytecodeType op, elf_Value left, elf_Value right)
{
	elf_report_runtime_error(state, RUNTIME_ERROR_GENERIC, -1, "%s: invalid operands '%s' and '%s'",
		bytecode_type_name(op), value_type_name(left.type), value_type_name(right.type));
}

static void vm_error_cannot_call(elf_State *state, elf_Value value)
{
	elf_report_runtime_error(state, RUNTIME_ERROR_GENERIC, -1, "cannot call '%s'", value_type_name(value.type));
}

static void vm_error_invalid_field_arguments(elf_State *state, elf_Value object, elf_Value field)
{
	elf_report_runtime_error(state, RUNTIME_ERROR_GENERIC, -1, "cannot get field '%s' from '%s'",
		value_type_name(field.type),
		value_type_name(object.type));
}

static inline elf_Table *vm_check_array(elf_State *state, elf_Value value)
{
	check_value_type(state, value, ELF_VALUE_TYPE_TABLE);
	return value_as_table(value);
}

static inline u32 vm_check_index(elf_State *state, int instr, elf_Value value, u32 count)
{
	check_value_type(state, value, ELF_VALUE_TYPE_INTEGER);
	return check_array_index(state, instr, value_as_integer(value), count);
}

static inline elf_String *vm_add_values_to_str(elf_State *state, elf_Value left, elf_Value right)
{
	elf_Scratch scratch = elf_begin_scratch();
	char *join_start = elf_arena_push(scratch.arena, 0);
	elf_print_value(scratch.arena, left);
	elf_print_value(scratch.arena, right);
	char *join_end = elf_arena_push_zero(scratch.arena, 1);
	elf_String *string = elf_string_from_data_size(state, join_start, (u32)(join_end - join_start));
	elf_end_scratch(scratch);
	return string;
}

static inline b32 vm_values_equal(elf_Value left, elf_Value right)
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
		return strings_equal(value_as_string(left), value_as_string(right));
	}

	return value_as_integer(left) == value_as_integer(right);
}

static elf_Closure *vm_new_closure(elf_State *state, BcFunctionRef function_ref, elf_Value *captures)
{
	BcFunction *function = bc_function_from_ref(function_ref);
	u32 size = sizeof(elf_Closure) + sizeof(elf_Value) * function->captures;
	elf_Closure *closure = (elf_Closure *)elf_gc_alloc(state, ELF_OBJECT_CLOSURE, size);
	closure->function = function_ref;

	value_copy_many(closure->captures, captures, function->captures);
	return closure;
}

static inline elf_Value *vm_slot(elf_Value *reference, i16 index)
{
	ASSERT(index >= 0);
	return reference + index;
}

static int run_bytecode_frame(elf_State *state)
{
	elf_Table *globals = state->globals;
	StackFrame *root_frame = state->frame;
	int nrets = state->frame->nrets;

activate_frame:
	StackFrame *frame = state->frame;
	ASSERT(frame->module && frame->function);
	elf_Module *module = frame->module;
	BcFunction *function = frame->function;
	elf_Value *framebase = frame->framebase;
	elf_Value *reference = frame->reference;
	elf_Value *captures = frame->captures;
	u32 instruction = frame->instruction;
	u8 frame_nrets = frame->nrets;
	u8 ncaptures = frame->ncaptures;
	int byte_offset = function->offset;
	int byte_count = function->length;
	const Bytecode *code = module->bytecode + byte_offset;

	while (instruction < byte_count) {
		int instr = instruction;
		int next_instruction = instr + 1;
		int byte_index = byte_offset + instr;
		frame->instruction = instruction;

		Bytecode byte = code[instr];

		switch (byte.b_type) {
			case BC_HALT: {
				goto exit;
			} break;

			case BC_NOP: {
			} break;

			case BC_CALL: {
				ASSERT(byte.b_x >= 0 && (u32)byte.b_x < function->stack_size);
				elf_Value callee = reference[byte.b_x];
				state->stack_ptr = reference + byte.b_x + byte.b_y + 1;

				if (value_is_closure(callee)) {
					push_stack_frame(state);
					prepare_closure_stack_frame(state, state->frame, value_as_closure(callee), byte.b_y, byte.b_z);
					ASSERT(state->stack_ptr == state->frame->framebase + state->frame->framesize);
					goto activate_frame;
				}
				else if (value_is_function(callee)) {
					push_stack_frame(state);
					call_native_function(state, value_as_function(callee), byte.b_y, byte.b_z);
					pop_stack_frame(state);
					state->frame->instruction++;
					elf_Value *stack_pointer = state->frame->framebase + state->frame->framesize;
					if (stack_pointer > state->stack_ptr) {
						value_zero_many(state->stack_ptr, stack_pointer - state->stack_ptr);
					}
					state->stack_ptr = stack_pointer;
					ASSERT(state->stack_ptr == state->frame->framebase + state->frame->framesize);
					goto activate_frame;
				}
				else {
					vm_error_cannot_call(state, callee);
				}
			} break;

			case BC_RETURN: {
				nrets = MIN(byte.b_y, frame_nrets);
				if (nrets) value_copy_many(framebase - 1, vm_slot(reference, byte.b_x), nrets);

				ASSERT(state->frame >= root_frame);
				if (state->frame != root_frame) {
					pop_stack_frame(state);
					state->frame->instruction++;
					elf_Value *stack_pointer = state->frame->framebase + state->frame->framesize;
					if (stack_pointer > state->stack_ptr) {
						value_zero_many(state->stack_ptr, stack_pointer - state->stack_ptr);
					}
					state->stack_ptr = stack_pointer;
					goto activate_frame;
				}
				else {
					ASSERT(framebase == root_frame->framebase);
					state->stack_ptr = framebase - 1 + nrets;
					goto exit;
				}
			} break;

			case BC_JUMP: {
				next_instruction = instr + byte.b_x;
			} break;

			case BC_JZ: {
				if (value_as_integer(*vm_slot(reference, byte.b_y)) == 0) {
					next_instruction = instr + byte.b_x;
				}
			} break;

			case BC_JNZ: {
				if (value_as_integer(*vm_slot(reference, byte.b_y)) != 0) {
					next_instruction = instr + byte.b_x;
				}
			} break;

			case BC_JE: {
				if (vm_values_equal(*vm_slot(reference, byte.b_y), *vm_slot(reference, byte.b_z))) {
					next_instruction = instr + byte.b_x;
				}
			} break;

			case BC_JNE: {
				if (!vm_values_equal(*vm_slot(reference, byte.b_y), *vm_slot(reference, byte.b_z))) {
					next_instruction = instr + byte.b_x;
				}
			} break;

			case BC_LOADNIL: {
				*vm_slot(reference, byte.b_x) = value_nil();
			} break;

			case BC_LOADKINT: {
				*vm_slot(reference, byte.b_x) = value_from_integer(module->integer_constants[byte.b_y]);
			} break;

			case BC_LOADKNUM: {
				*vm_slot(reference, byte.b_x) = value_from_number(module->number_constants[byte.b_y]);
			} break;

			case BC_LOADKSTRING: {
				*vm_slot(reference, byte.b_x) = value_from_string(module->strings[byte.b_y]);
			} break;

			case BC_LOADCVAL: {
				ASSERT(byte.b_y >= 0 && byte.b_y < ncaptures);
				value_copy(vm_slot(reference, byte.b_x), captures[byte.b_y]);
			} break;

			case BC_RELOAD: {
				value_copy(vm_slot(reference, byte.b_x), *vm_slot(reference, byte.b_y));
			} break;

			case BC_CURRENT_CLOSURE: {
				value_copy(vm_slot(reference, byte.b_x), framebase[-1]);
			} break;

			case BC_GETGLOBAL: {
				value_copy(vm_slot(reference, byte.b_x), elf_array_get(state, globals, byte.b_y));
			} break;

			case BC_SETGLOBAL: {
				elf_array_set(state, globals, byte.b_x, *vm_slot(reference, byte.b_y));
			} break;

			case BC_TABLE: {
				*vm_slot(reference, byte.b_x) = value_from_table(elf_new_table_rogue(state));
			} break;

			case BC_CLOSURE: {
				ASSERT(byte.b_y >= 0);
				BcFunctionRef function = {
					.module = module,
					.index  = (u32)byte.b_y,
				};
				ASSERT(bc_function_ref_is_valid(function));

				elf_Value *result = vm_slot(reference, byte.b_x);
				elf_Closure *new_closure = vm_new_closure(state, function, result);
				*result = value_from_closure(new_closure);
			} break;

			case BC_GETFIELD: {
				elf_Value *result = vm_slot(reference, byte.b_x);
				elf_Value object = *vm_slot(reference, byte.b_y);
				elf_Value field = *vm_slot(reference, byte.b_z);

				if (value_is_nil(field)) {
					elf_report_runtime_error(state, RUNTIME_ERROR_GENERIC, byte_index, "nil is not a valid field");
				}

				switch (object.type) {
					case ELF_VALUE_TYPE_TABLE: {
						*result = elf_table_get_or_nil(state, value_as_table(object), field);
					} break;

					case ELF_VALUE_TYPE_STRING: {
						const char *text = string_data(value_as_string(object));

						if (value_is_integer(field)) {
							i64 index = value_as_integer(field);
							*result = value_from_integer(text[index]);
						}
						else if (value_is_string(field)) {
							// Todo, impl!
							*result = value_nil();
						}
						else {
							vm_error_invalid_field_arguments(state, object, field);
						}
					} break;

					default: {
						vm_error_invalid_field_arguments(state, object, field);
					} break;
				}
			} break;

			case BC_SETFIELD: {
				elf_Value table = *vm_slot(reference, byte.b_x);
				elf_Value key = *vm_slot(reference, byte.b_y);
				elf_Value value = *vm_slot(reference, byte.b_z);

				if (value_is_table(table)) {
					if (value_is_nil(key)) {
						elf_report_runtime_error(state, RUNTIME_ERROR_GENERIC, byte_index, "key is nil...");
					}

					elf_table_set(state, value_as_table(table), key, value);
				}
				else if (value_is_user(table)) {
					elf_report_runtime_error(state, RUNTIME_ERROR_GENERIC, byte_index, "overload not implemented");
				}
				else if (value_is_string(table)) {
					elf_report_runtime_error(state, RUNTIME_ERROR_GENERIC, byte_index, "strings are readonly, you may not change them");
				}
				else {
					elf_report_runtime_error(state, RUNTIME_ERROR_GENERIC, byte_index, "attempted to set field of '%s' value", value_type_name(table.type));
				}
			} break;

			case BC_GETINDEX: {
				elf_Table *array = vm_check_array(state, *vm_slot(reference, byte.b_y));
				u32 index = vm_check_index(state, byte_index, *vm_slot(reference, byte.b_z), elf_array_length(array));
				value_copy(vm_slot(reference, byte.b_x), elf_array_get(state, array, index));
			} break;

			case BC_SETINDEX: {
				elf_Table *array = vm_check_array(state, *vm_slot(reference, byte.b_x));
				u32 index = vm_check_index(state, byte_index, *vm_slot(reference, byte.b_y), elf_array_length(array));
				elf_array_set(state, array, index, *vm_slot(reference, byte.b_z));
			} break;

			case BC_ARRAYADD: {
				elf_Value array = *vm_slot(reference, byte.b_x);
				check_value_type(state, array, ELF_VALUE_TYPE_TABLE);
				elf_array_add(state, value_as_table(array), *vm_slot(reference, byte.b_y));
			} break;

			case BC_GETLENGTH: {
				elf_Table *array = vm_check_array(state, *vm_slot(reference, byte.b_y));
				*vm_slot(reference, byte.b_x) = value_from_integer(elf_array_length(array));
			} break;

			case BC_GETMETAFIELD: {
				elf_Value value = *vm_slot(reference, byte.b_y);
				elf_Table *metatable = elf_get_type_metatable(state, value);

				if (!metatable) {
					elf_report_runtime_error(state, RUNTIME_ERROR_GENERIC, -1, "'%s': required metatable for metafield lookup", value_type_name(value.type));
				}

				value_copy(vm_slot(reference, byte.b_x), elf_table_get_or_nil(state, metatable, *vm_slot(reference, byte.b_z)));
			} break;

			case BC_ENFORCE: {
				check_value_type_rule(state, *vm_slot(reference, byte.b_x), byte.b_y);
			} break;

			case BC_I2N: {
				*vm_slot(reference, byte.b_x) = value_from_number(value_to_number(*vm_slot(reference, byte.b_y)));
			} break;

			case BC_N2I: {
				*vm_slot(reference, byte.b_x) = value_from_integer(value_to_integer(*vm_slot(reference, byte.b_y)));
			} break;

			case BC_EQ: {
				*vm_slot(reference, byte.b_x) = value_from_integer(vm_values_equal(*vm_slot(reference, byte.b_y), *vm_slot(reference, byte.b_z)));
			} break;

			case BC_NEQ: {
				*vm_slot(reference, byte.b_x) = value_from_integer(!vm_values_equal(*vm_slot(reference, byte.b_y), *vm_slot(reference, byte.b_z)));
			} break;

			case BC_LT: {
				elf_Value left = *vm_slot(reference, byte.b_y);
				elf_Value right = *vm_slot(reference, byte.b_z);
				if (value_is_numeric(left) && value_is_numeric(right)) {
					if (value_is_number(left) || value_is_number(right)) {
						*vm_slot(reference, byte.b_x) = value_from_integer(value_to_number(left) < value_to_number(right));
					}
					else {
						*vm_slot(reference, byte.b_x) = value_from_integer(value_as_integer(left) < value_as_integer(right));
					}
				}
				else {
					vm_error_invalid_operands(state, BC_LT, left, right);
				}
			} break;

			case BC_LTEQ: {
				elf_Value left = *vm_slot(reference, byte.b_y);
				elf_Value right = *vm_slot(reference, byte.b_z);
				if (value_is_numeric(left) && value_is_numeric(right)) {
					if (value_is_number(left) || value_is_number(right)) {
						*vm_slot(reference, byte.b_x) = value_from_integer(value_to_number(left) <= value_to_number(right));
					}
					else {
						*vm_slot(reference, byte.b_x) = value_from_integer(value_as_integer(left) <= value_as_integer(right));
					}
				}
				else {
					vm_error_invalid_operands(state, BC_LTEQ, left, right);
				}
			} break;

			case BC_ADD: {
				elf_Value left = *vm_slot(reference, byte.b_y);
				elf_Value right = *vm_slot(reference, byte.b_z);
				if (value_is_string(left)) {
					*vm_slot(reference, byte.b_x) = value_from_string(vm_add_values_to_str(state, left, right));
				}
				else if (value_is_numeric(left) && value_is_numeric(right)) {
					if (value_is_number(left) || value_is_number(right)) {
						*vm_slot(reference, byte.b_x) = value_from_number(value_to_number(left) + value_to_number(right));
					}
					else {
						*vm_slot(reference, byte.b_x) = value_from_integer(value_as_integer(left) + value_as_integer(right));
					}
				}
				else {
					vm_error_invalid_operands(state, BC_ADD, left, right);
				}
			} break;

			case BC_SUB: {
				elf_Value left = *vm_slot(reference, byte.b_y);
				elf_Value right = *vm_slot(reference, byte.b_z);
				if (value_is_numeric(left) && value_is_numeric(right)) {
					if (value_is_number(left) || value_is_number(right)) {
						*vm_slot(reference, byte.b_x) = value_from_number(value_to_number(left) - value_to_number(right));
					}
					else {
						*vm_slot(reference, byte.b_x) = value_from_integer(value_as_integer(left) - value_as_integer(right));
					}
				}
				else {
					vm_error_invalid_operands(state, BC_SUB, left, right);
				}
			} break;

			case BC_MUL: {
				elf_Value left = *vm_slot(reference, byte.b_y);
				elf_Value right = *vm_slot(reference, byte.b_z);
				if (value_is_numeric(left) && value_is_numeric(right)) {
					if (value_is_number(left) || value_is_number(right)) {
						*vm_slot(reference, byte.b_x) = value_from_number(value_to_number(left) * value_to_number(right));
					}
					else {
						*vm_slot(reference, byte.b_x) = value_from_integer(value_as_integer(left) * value_as_integer(right));
					}
				}
				else {
					vm_error_invalid_operands(state, BC_MUL, left, right);
				}
			} break;

			case BC_DIV: {
				elf_Value left = *vm_slot(reference, byte.b_y);
				elf_Value right = *vm_slot(reference, byte.b_z);
				if (value_is_numeric(left) && value_is_numeric(right)) {
					if (value_is_number(left) || value_is_number(right)) {
						*vm_slot(reference, byte.b_x) = value_from_number(value_to_number(left) / value_to_number(right));
					}
					else {
						*vm_slot(reference, byte.b_x) = value_from_integer(value_as_integer(left) / value_as_integer(right));
					}
				}
				else {
					vm_error_invalid_operands(state, BC_DIV, left, right);
				}
			} break;

			case BC_MOD: {
				elf_Value left = *vm_slot(reference, byte.b_y);
				elf_Value right = *vm_slot(reference, byte.b_z);
				if (value_is_numeric(left) && value_is_numeric(right)) {
					if (value_is_number(left) || value_is_number(right)) {
						f64 left_number = value_to_number(left);
						f64 right_number = value_to_number(right);
						*vm_slot(reference, byte.b_x) = value_from_number(left_number - ((i64)(left_number / right_number)) * right_number);
					}
					else {
						*vm_slot(reference, byte.b_x) = value_from_integer(value_as_integer(left) % value_as_integer(right));
					}
				}
				else {
					vm_error_invalid_operands(state, BC_MOD, left, right);
				}
			} break;

			case BC_POW: {
				elf_Value left = *vm_slot(reference, byte.b_y);
				elf_Value right = *vm_slot(reference, byte.b_z);
				if (value_is_numeric(left) && value_is_numeric(right)) {
					if (value_is_number(left) || value_is_number(right)) {
						*vm_slot(reference, byte.b_x) = value_from_number(pow(value_to_number(left), value_to_number(right)));
					}
					else {
						*vm_slot(reference, byte.b_x) = value_from_integer((i64)pow(value_as_integer(left), value_as_integer(right)));
					}
				}
				else {
					vm_error_invalid_operands(state, BC_POW, left, right);
				}
			} break;

			case BC_BIT_NOT: {
				elf_Value value = *vm_slot(reference, byte.b_y);
				check_value_type(state, value, ELF_VALUE_TYPE_INTEGER);
				*vm_slot(reference, byte.b_x) = value_from_integer(~value_as_integer(value));
			} break;

			case BC_BIT_AND: {
				elf_Value left = *vm_slot(reference, byte.b_y);
				elf_Value right = *vm_slot(reference, byte.b_z);
				if (value_is_integer(left) && value_is_integer(right)) {
					*vm_slot(reference, byte.b_x) = value_from_integer(value_as_integer(left) & value_as_integer(right));
				}
				else {
					vm_error_invalid_operands(state, BC_BIT_AND, left, right);
				}
			} break;

			case BC_BIT_OR: {
				elf_Value left = *vm_slot(reference, byte.b_y);
				elf_Value right = *vm_slot(reference, byte.b_z);
				if (value_is_integer(left) && value_is_integer(right)) {
					*vm_slot(reference, byte.b_x) = value_from_integer(value_as_integer(left) | value_as_integer(right));
				}
				else {
					vm_error_invalid_operands(state, BC_BIT_OR, left, right);
				}
			} break;

			case BC_BIT_XOR: {
				elf_Value left = *vm_slot(reference, byte.b_y);
				elf_Value right = *vm_slot(reference, byte.b_z);
				if (value_is_integer(left) && value_is_integer(right)) {
					*vm_slot(reference, byte.b_x) = value_from_integer(value_as_integer(left) ^ value_as_integer(right));
				}
				else {
					vm_error_invalid_operands(state, BC_BIT_XOR, left, right);
				}
			} break;

			case BC_BIT_SHL: {
				elf_Value left = *vm_slot(reference, byte.b_y);
				elf_Value right = *vm_slot(reference, byte.b_z);
				if (value_is_integer(left) && value_is_integer(right)) {
					*vm_slot(reference, byte.b_x) = value_from_integer(value_as_integer(left) << value_as_integer(right));
				}
				else {
					vm_error_invalid_operands(state, BC_BIT_SHL, left, right);
				}
			} break;

			case BC_BIT_SHR: {
				elf_Value left = *vm_slot(reference, byte.b_y);
				elf_Value right = *vm_slot(reference, byte.b_z);
				if (value_is_integer(left) && value_is_integer(right)) {
					*vm_slot(reference, byte.b_x) = value_from_integer(value_as_integer(left) >> value_as_integer(right));
				}
				else {
					vm_error_invalid_operands(state, BC_BIT_SHR, left, right);
				}
			} break;

			default: {
				elf_report_runtime_error(state, RUNTIME_ERROR_UNKNOWN_BYTECODE, NO_BYTE, "'%s' unknown bytecode", bytecode_type_name(byte.b_type));
			} break;
		}
		instruction = next_instruction;
	}
	frame->instruction = instruction;

exit:
	return nrets;
}
