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
	elf_String *atom = elf_atom_from_data_size(state, join_start, (u32)(join_end - join_start));
	elf_end_scratch(scratch);
	return atom;
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

	if (value_is_atom(left)) {
		return atoms_equal(value_as_atom(left), value_as_atom(right));
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

static inline void vm_activate_frame(elf_State *state, StackFrame *frame, int *byte_offset, int *byte_count, const Bytecode **code)
{
	ASSERT(frame->module && frame->function);
	*byte_offset = frame->function->offset;
	*byte_count = frame->function->length;
	*code = frame->module->bytecode + *byte_offset;
	state->active_module = frame->module;
	state->active_function = frame->function;
}

static int run_bytecode_frame(elf_State *state, StackFrame frame)
{
	elf_Table *globals = state->globals;
	elf_Value *root_frame = frame.framebase;
	int nrets = frame.nrets;
	i32 subframe_count = 0;
	int byte_offset;
	int byte_count;
	const Bytecode *code;
	vm_activate_frame(state, &frame, &byte_offset, &byte_count, &code);

	while (frame.nextinstr < byte_count) {
		int instr = frame.nextinstr++;
		int byte_index = byte_offset + instr;
		// Todo, remove this!
		state->byte = byte_index;

		Bytecode byte = code[instr];
		elf_Value *result_slot = &frame.reference[byte.b_x];
		elf_Value *left_slot = &frame.reference[byte.b_y];
		elf_Value *right_slot = &frame.reference[byte.b_z];

		switch (byte.b_type) {
			case BC_HALT: {
				goto exit;
			} break;

			case BC_NOP: {
			} break;

			case BC_CALL: {
				elf_Value callee = *result_slot;
				state->stack_ptr = frame.reference + byte.b_x + byte.b_y + 1;

				state->frame_stack[state->frame_index++] = frame;
				if (value_is_closure(callee)) {
					prepare_closure_stack_frame(state, &frame, value_as_closure(callee), byte.b_y, byte.b_z);
					vm_activate_frame(state, &frame, &byte_offset, &byte_count, &code);
					subframe_count++;
				}
				else if (value_is_function(callee)) {
					call_native_function(state, value_as_function(callee), byte.b_y, byte.b_z);
					state->active_module = frame.module;
					state->active_function = frame.function;
					state->frame_index--;
					elf_Value *stack_pointer = frame.framebase + frame.framesize;
					if (stack_pointer > state->stack_ptr) {
						value_zero_many(state->stack_ptr, stack_pointer - state->stack_ptr);
					}
					state->stack_ptr = stack_pointer;
				}
				else {
					vm_error_cannot_call(state, callee);
				}

				ASSERT(state->stack_ptr == frame.framebase + frame.framesize);
			} break;

			case BC_RETURN: {
				nrets = MIN(byte.b_y, frame.nrets);
				value_copy_many(frame.framebase - 1, frame.reference + byte.b_x, nrets);

				ASSERT(state->frame_index > 0);
				if (subframe_count) {
					frame = state->frame_stack[--state->frame_index];
					vm_activate_frame(state, &frame, &byte_offset, &byte_count, &code);
					elf_Value *stack_pointer = frame.framebase + frame.framesize;
					if (stack_pointer > state->stack_ptr) {
						value_zero_many(state->stack_ptr, stack_pointer - state->stack_ptr);
					}
					state->stack_ptr = stack_pointer;
					subframe_count--;
				}
				else {
					ASSERT(frame.framebase == root_frame);
					state->stack_ptr = frame.framebase - 1 + nrets;
					goto exit;
				}
			} break;

			case BC_JUMP: {
				frame.nextinstr = instr + byte.b_x;
			} break;

			case BC_JZ: {
				if (value_as_integer(*left_slot) == 0) {
					frame.nextinstr = instr + byte.b_x;
				}
			} break;

			case BC_JNZ: {
				if (value_as_integer(*left_slot) != 0) {
					frame.nextinstr = instr + byte.b_x;
				}
			} break;

			case BC_JE: {
				if (vm_values_equal(*left_slot, *right_slot)) {
					frame.nextinstr = instr + byte.b_x;
				}
			} break;

			case BC_JNE: {
				if (!vm_values_equal(*left_slot, *right_slot)) {
					frame.nextinstr = instr + byte.b_x;
				}
			} break;

			case BC_LOADNIL: {
				*result_slot = value_nil();
			} break;

			case BC_LOADKINT: {
				*result_slot = value_from_integer(frame.module->integer_constants[byte.b_y]);
			} break;

			case BC_LOADKNUM: {
				*result_slot = value_from_number(frame.module->number_constants[byte.b_y]);
			} break;

			case BC_LOADCVAL: {
				ASSERT(byte.b_y >= 0 && byte.b_y < frame.closuresize);
				value_copy(result_slot, frame.closureenv[byte.b_y]);
			} break;

			case BC_RELOAD: {
				value_copy(result_slot, *left_slot);
			} break;

			case BC_CURRENT_CLOSURE: {
				value_copy(result_slot, frame.framebase[-1]);
			} break;

			case BC_GETGLOBAL: {
				value_copy(result_slot, elf_array_get(state, globals, byte.b_y));
			} break;

			case BC_SETGLOBAL: {
				elf_array_set(state, globals, byte.b_x, *left_slot);
			} break;

			case BC_TABLE: {
				*result_slot = value_from_table(elf_new_table_rogue(state));
			} break;

			case BC_CLOSURE: {
				ASSERT(byte.b_y >= 0);
				BcFunctionRef function = {
					.module = frame.module,
					.index  = (u32)byte.b_y,
				};
				ASSERT(bc_function_ref_is_valid(function));

				elf_Closure * new_closure = vm_new_closure(state, function, result_slot);
				*result_slot = value_from_closure(new_closure);
			} break;

			case BC_GETFIELD: {
				elf_Value object = *left_slot;
				elf_Value field = *right_slot;

				if (value_is_nil(field)) {
					elf_report_runtime_error(state, RUNTIME_ERROR_GENERIC, byte_index, "nil is not a valid field");
				}

				switch (object.type) {
					case ELF_VALUE_TYPE_TABLE: {
						*result_slot = elf_table_get_or_nil(state, value_as_table(object), field);
					} break;

					case ELF_VALUE_TYPE_ATOM: {
						const char *text = atom_data(value_as_atom(object));

						if (value_is_integer(field)) {
							i64 index = value_as_integer(field);
							*result_slot = value_from_integer(text[index]);
						}
						else if (value_is_atom(field)) {
							// Todo, impl!
							*result_slot = value_nil();
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
				elf_Value table = *result_slot;
				elf_Value key = *left_slot;
				elf_Value value = *right_slot;

				if (value_is_table(table)) {
					if (value_is_nil(key)) {
						elf_report_runtime_error(state, RUNTIME_ERROR_GENERIC, byte_index, "key is nil...");
					}

					elf_table_set(state, value_as_table(table), key, value);
				}
				else if (value_is_user(table)) {
					elf_report_runtime_error(state, RUNTIME_ERROR_GENERIC, byte_index, "overload not implemented");
				}
				else if (value_is_atom(table)) {
					elf_report_runtime_error(state, RUNTIME_ERROR_GENERIC, byte_index, "atoms are readonly, you may not change them");
				}
				else {
					elf_report_runtime_error(state, RUNTIME_ERROR_GENERIC, byte_index, "attempted to set field of '%s' value", value_type_name(table.type));
				}
			} break;

			case BC_GETINDEX: {
				elf_Table *array = vm_check_array(state, *left_slot);
				u32 index = vm_check_index(state, byte_index, *right_slot, elf_array_length(array));
				value_copy(result_slot, elf_array_get(state, array, index));
			} break;

			case BC_SETINDEX: {
				elf_Table *array = vm_check_array(state, *result_slot);
				u32 index = vm_check_index(state, byte_index, *left_slot, elf_array_length(array));
				elf_array_set(state, array, index, *right_slot);
			} break;

			case BC_ARRAYADD: {
				check_value_type(state, *result_slot, ELF_VALUE_TYPE_TABLE);
				elf_array_add(state, value_as_table(*result_slot), *left_slot);
			} break;

			case BC_GETLENGTH: {
				elf_Table *array = vm_check_array(state, *left_slot);
				*result_slot = value_from_integer(elf_array_length(array));
			} break;

			case BC_GETMETAFIELD: {
				elf_Value value = *left_slot;
				elf_Table *metatable = elf_get_type_metatable(state, value);

				if (!metatable) {
					elf_report_runtime_error(state, RUNTIME_ERROR_GENERIC, -1, "'%s': required metatable for metafield lookup", value_type_name(value.type));
				}

				value_copy(result_slot, elf_table_get_or_nil(state, metatable, *right_slot));
			} break;

			case BC_ENFORCE: {
				check_value_type_rule(state, *result_slot, byte.b_y);
			} break;

			case BC_I2N: {
				*result_slot = value_from_number(value_to_number(*left_slot));
			} break;

			case BC_N2I: {
				*result_slot = value_from_integer(value_to_integer(*left_slot));
			} break;

			case BC_EQ: {
				*result_slot = value_from_integer(vm_values_equal(*left_slot, *right_slot));
			} break;

			case BC_NEQ: {
				*result_slot = value_from_integer(!vm_values_equal(*left_slot, *right_slot));
			} break;

			case BC_LT: {
				elf_Value left = *left_slot;
				elf_Value right = *right_slot;
				if (value_is_numeric(left) && value_is_numeric(right)) {
					if (value_is_number(left) || value_is_number(right)) {
						*result_slot = value_from_integer(value_to_number(left) < value_to_number(right));
					}
					else {
						*result_slot = value_from_integer(value_as_integer(left) < value_as_integer(right));
					}
				}
				else {
					vm_error_invalid_operands(state, BC_LT, left, right);
				}
			} break;

			case BC_LTEQ: {
				elf_Value left = *left_slot;
				elf_Value right = *right_slot;
				if (value_is_numeric(left) && value_is_numeric(right)) {
					if (value_is_number(left) || value_is_number(right)) {
						*result_slot = value_from_integer(value_to_number(left) <= value_to_number(right));
					}
					else {
						*result_slot = value_from_integer(value_as_integer(left) <= value_as_integer(right));
					}
				}
				else {
					vm_error_invalid_operands(state, BC_LTEQ, left, right);
				}
			} break;

			case BC_ADD: {
				elf_Value left = *left_slot;
				elf_Value right = *right_slot;
				if (value_is_atom(left)) {
					*result_slot = value_from_atom(vm_add_values_to_str(state, left, right));
				}
				else if (value_is_numeric(left) && value_is_numeric(right)) {
					if (value_is_number(left) || value_is_number(right)) {
						*result_slot = value_from_number(value_to_number(left) + value_to_number(right));
					}
					else {
						*result_slot = value_from_integer(value_as_integer(left) + value_as_integer(right));
					}
				}
				else {
					vm_error_invalid_operands(state, BC_ADD, left, right);
				}
			} break;

			case BC_SUB: {
				elf_Value left = *left_slot;
				elf_Value right = *right_slot;
				if (value_is_numeric(left) && value_is_numeric(right)) {
					if (value_is_number(left) || value_is_number(right)) {
						*result_slot = value_from_number(value_to_number(left) - value_to_number(right));
					}
					else {
						*result_slot = value_from_integer(value_as_integer(left) - value_as_integer(right));
					}
				}
				else {
					vm_error_invalid_operands(state, BC_SUB, left, right);
				}
			} break;

			case BC_MUL: {
				elf_Value left = *left_slot;
				elf_Value right = *right_slot;
				if (value_is_numeric(left) && value_is_numeric(right)) {
					if (value_is_number(left) || value_is_number(right)) {
						*result_slot = value_from_number(value_to_number(left) * value_to_number(right));
					}
					else {
						*result_slot = value_from_integer(value_as_integer(left) * value_as_integer(right));
					}
				}
				else {
					vm_error_invalid_operands(state, BC_MUL, left, right);
				}
			} break;

			case BC_DIV: {
				elf_Value left = *left_slot;
				elf_Value right = *right_slot;
				if (value_is_numeric(left) && value_is_numeric(right)) {
					if (value_is_number(left) || value_is_number(right)) {
						*result_slot = value_from_number(value_to_number(left) / value_to_number(right));
					}
					else {
						*result_slot = value_from_integer(value_as_integer(left) / value_as_integer(right));
					}
				}
				else {
					vm_error_invalid_operands(state, BC_DIV, left, right);
				}
			} break;

			case BC_MOD: {
				elf_Value left = *left_slot;
				elf_Value right = *right_slot;
				if (value_is_numeric(left) && value_is_numeric(right)) {
					if (value_is_number(left) || value_is_number(right)) {
						f64 left_number = value_to_number(left);
						f64 right_number = value_to_number(right);
						*result_slot = value_from_number(left_number - ((i64)(left_number / right_number)) * right_number);
					}
					else {
						*result_slot = value_from_integer(value_as_integer(left) % value_as_integer(right));
					}
				}
				else {
					vm_error_invalid_operands(state, BC_MOD, left, right);
				}
			} break;

			case BC_POW: {
				elf_Value left = *left_slot;
				elf_Value right = *right_slot;
				if (value_is_numeric(left) && value_is_numeric(right)) {
					if (value_is_number(left) || value_is_number(right)) {
						*result_slot = value_from_number(pow(value_to_number(left), value_to_number(right)));
					}
					else {
						*result_slot = value_from_integer((i64)pow(value_as_integer(left), value_as_integer(right)));
					}
				}
				else {
					vm_error_invalid_operands(state, BC_POW, left, right);
				}
			} break;

			case BC_BIT_NOT: {
				check_value_type(state, *left_slot, ELF_VALUE_TYPE_INTEGER);
				*result_slot = value_from_integer(~value_as_integer(*left_slot));
			} break;

			case BC_BIT_AND: {
				elf_Value left = *left_slot;
				elf_Value right = *right_slot;
				if (value_is_integer(left) && value_is_integer(right)) {
					*result_slot = value_from_integer(value_as_integer(left) & value_as_integer(right));
				}
				else {
					vm_error_invalid_operands(state, BC_BIT_AND, left, right);
				}
			} break;

			case BC_BIT_OR: {
				elf_Value left = *left_slot;
				elf_Value right = *right_slot;
				if (value_is_integer(left) && value_is_integer(right)) {
					*result_slot = value_from_integer(value_as_integer(left) | value_as_integer(right));
				}
				else {
					vm_error_invalid_operands(state, BC_BIT_OR, left, right);
				}
			} break;

			case BC_BIT_XOR: {
				elf_Value left = *left_slot;
				elf_Value right = *right_slot;
				if (value_is_integer(left) && value_is_integer(right)) {
					*result_slot = value_from_integer(value_as_integer(left) ^ value_as_integer(right));
				}
				else {
					vm_error_invalid_operands(state, BC_BIT_XOR, left, right);
				}
			} break;

			case BC_BIT_SHL: {
				elf_Value left = *left_slot;
				elf_Value right = *right_slot;
				if (value_is_integer(left) && value_is_integer(right)) {
					*result_slot = value_from_integer(value_as_integer(left) << value_as_integer(right));
				}
				else {
					vm_error_invalid_operands(state, BC_BIT_SHL, left, right);
				}
			} break;

			case BC_BIT_SHR: {
				elf_Value left = *left_slot;
				elf_Value right = *right_slot;
				if (value_is_integer(left) && value_is_integer(right)) {
					*result_slot = value_from_integer(value_as_integer(left) >> value_as_integer(right));
				}
				else {
					vm_error_invalid_operands(state, BC_BIT_SHR, left, right);
				}
			} break;

			default: {
				elf_report_runtime_error(state, RUNTIME_ERROR_UNKNOWN_BYTECODE, NO_BYTE, "'%s' unknown bytecode", bytecode_type_name(byte.b_type));
			} break;
		}
	}

exit:
	return nrets;
}
