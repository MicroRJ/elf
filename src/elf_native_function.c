//
// See Copyright Notice In elf.h
//

#define NATIVE_FUNCTION_INITIAL_CAPACITY 16u

ErrorCode elf_native_function_ensure(elf_State *state, elf_Function function, u32 *index)
{
	if (index) {
		*index = UINT32_MAX;
	}
	if (!state || !function || !index) {
		return ERROR_INVALID_ARGUMENT;
	}

	for (u32 i = 0; i < state->native_function_count; ++i)
	{
		if (state->native_functions[i] == function)
		{
			*index = i;
			return ERROR_NONE;
		}
	}

	if (state->native_function_count == UINT32_MAX) {
		return ERROR_OUT_OF_RANGE;
	}
	if (state->native_function_count == state->native_function_capacity)
	{
		u32 old_capacity = state->native_function_capacity;
		u32 new_capacity = old_capacity ? old_capacity << 1 : NATIVE_FUNCTION_INITIAL_CAPACITY;
		if (new_capacity < old_capacity) {
			new_capacity = UINT32_MAX;
		}
		u64 old_size = (u64)old_capacity * sizeof(*state->native_functions);
		u64 new_size = (u64)new_capacity * sizeof(*state->native_functions);
		if (new_size > SIZE_MAX) {
			return ERROR_OUT_OF_MEMORY;
		}
		elf_Function *functions = elf_realloc(state, state->native_functions, old_size, new_size);
		if (!functions) {
			return ERROR_OUT_OF_MEMORY;
		}
		state->native_functions = functions;
		state->native_function_capacity = new_capacity;
	}

	u32 function_index = state->native_function_count++;
	state->native_functions[function_index] = function;
	*index = function_index;
	return ERROR_NONE;
}

void elf_native_functions_destroy(elf_State *state)
{
	ASSERT(state);
	elf_free(state, state->native_functions, (u64)state->native_function_capacity * sizeof(*state->native_functions));
	state->native_functions = 0;
	state->native_function_count = 0;
	state->native_function_capacity = 0;
}
