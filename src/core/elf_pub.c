//
// See Copyright Notice In elf.h
//

#define PUBLIC_BINDING_INITIAL_CAPACITY 16u

typedef union
{
	i64          integer;
	f64          number;
	dy_String     string;
	elf_Function function;
}
PublicBindingInput;

static b32 public_binding_name_equal(dy_String left, dy_String right)
{
	return left.size == right.size && (!left.size || !memcmp(left.data, right.data, (size_t)left.size));
}

static b32 public_binding_exists(elf_State *state, dy_String name)
{
	for (u32 i = 0; i < state->public_binding_count; ++i)
	{
		if (public_binding_name_equal(state->public_bindings[i].name, name))
		{
			return true;
		}
	}
	return false;
}

static ErrorCode public_bindings_reserve_one(elf_State *state)
{
	if (state->public_binding_count < state->public_binding_capacity)
	{
		return ERROR_NONE;
	}
	if (state->public_binding_count == UINT32_MAX)
	{
		return ERROR_OUT_OF_RANGE;
	}

	u32 old_capacity = state->public_binding_capacity;
	u32 new_capacity = old_capacity ? old_capacity << 1 : PUBLIC_BINDING_INITIAL_CAPACITY;
	if (new_capacity < old_capacity)
	{
		new_capacity = UINT32_MAX;
	}
	u64 old_size = (u64)old_capacity * sizeof(*state->public_bindings);
	u64 new_size = (u64)new_capacity * sizeof(*state->public_bindings);
	if (new_size > SIZE_MAX)
	{
		return ERROR_OUT_OF_MEMORY;
	}

	elf_PublicBinding *bindings = elf_realloc(state, state->public_bindings, old_size, new_size);
	if (!bindings)
	{
		return ERROR_OUT_OF_MEMORY;
	}
	state->public_bindings = bindings;
	state->public_binding_capacity = new_capacity;
	return ERROR_NONE;
}

static ErrorCode public_binding_copy_string(elf_State *state, dy_String source, dy_String *destination)
{
	ASSERT(destination);
	*destination = (dy_String) {0};
	if (!source.data && source.size)
	{
		return ERROR_INVALID_ARGUMENT;
	}
	if (source.size == UINT64_MAX)
	{
		return ERROR_OUT_OF_RANGE;
	}

	char *data = elf_alloc(state, source.size + 1);
	if (!data)
	{
		return ERROR_OUT_OF_MEMORY;
	}
	if (source.size)
	{
		copy_memory(data, source.data, source.size);
	}
	data[source.size] = 0;
	destination->data = data;
	destination->size = source.size;
	return ERROR_NONE;
}

static ErrorCode public_binding_add(elf_State *state, dy_String name, elf_PublicType type, PublicBindingInput input)
{
	if (!state || !name.data || !name.size)
	{
		return ERROR_INVALID_ARGUMENT;
	}
	if (type == ELF_PUBLIC_STRING && !input.string.data && input.string.size)
	{
		return ERROR_INVALID_ARGUMENT;
	}
	if (type == ELF_PUBLIC_FUNCTION && !input.function)
	{
		return ERROR_INVALID_ARGUMENT;
	}
	if (type > ELF_PUBLIC_FUNCTION || public_binding_exists(state, name))
	{
		return ERROR_INVALID_ARGUMENT;
	}

	ErrorCode error = public_bindings_reserve_one(state);
	if (error != ERROR_NONE)
	{
		return error;
	}

	dy_String name_copy = {0};
	error = public_binding_copy_string(state, name, &name_copy);
	if (error != ERROR_NONE)
	{
		return error;
	}

	dy_String string_copy = {0};
	if (type == ELF_PUBLIC_STRING)
	{
		error = public_binding_copy_string(state, input.string, &string_copy);
		if (error != ERROR_NONE)
		{
			elf_free(state, name_copy.data, name_copy.size + 1);
			return error;
		}
	}

	u32 native_function_index = UINT32_MAX;
	if (type == ELF_PUBLIC_FUNCTION)
	{
		error = elf_native_function_ensure(state, input.function, &native_function_index);
		if (error != ERROR_NONE)
		{
			elf_free(state, name_copy.data, name_copy.size + 1);
			return error;
		}
	}

	elf_PublicBinding *binding = state->public_bindings + state->public_binding_count++;
	zero_memory(binding, sizeof(*binding));
	binding->name = name_copy;
	binding->type = type;
	switch (type)
	{
		case ELF_PUBLIC_INTEGER:
		{
			binding->integer = input.integer;
		}
		break;
		case ELF_PUBLIC_NUMBER:
		{
			binding->number = input.number;
		}
		break;
		case ELF_PUBLIC_STRING:
		{
			binding->string = string_copy;
		}
		break;
		case ELF_PUBLIC_FUNCTION:
		{
			binding->native_function_index = native_function_index;
		}
		break;
	}
	return ERROR_NONE;
}

static elf_ErrorCode public_binding_error(ErrorCode error)
{
	switch (error)
	{
		case ERROR_NONE:             return ELF_ERROR_NONE;
		case ERROR_INVALID_ARGUMENT: return ELF_ERROR_INVALID_ARGUMENT;
		case ERROR_OUT_OF_RANGE:     return ELF_ERROR_OUT_OF_RANGE;
		case ERROR_OUT_OF_MEMORY:    return ELF_ERROR_OUT_OF_MEMORY;
		default:
		{
			ASSERT(!"unhandled public binding error");
			return ELF_ERROR_INVALID_ARGUMENT;
		}
	}
}

elf_ErrorCode elf_pub_int(elf_State *state, elf_StrSlice name, elf_Int value)
{
	PublicBindingInput input = {.integer = value};
	return public_binding_error(public_binding_add(state, (dy_String) {name.data, name.size}, ELF_PUBLIC_INTEGER, input));
}

elf_ErrorCode elf_pub_num(elf_State *state, elf_StrSlice name, elf_Num value)
{
	PublicBindingInput input = {.number = value};
	return public_binding_error(public_binding_add(state, (dy_String) {name.data, name.size}, ELF_PUBLIC_NUMBER, input));
}

elf_ErrorCode elf_pub_str(elf_State *state, elf_StrSlice name, elf_StrSlice value)
{
	PublicBindingInput input = {.string = (dy_String) {value.data, value.size}};
	return public_binding_error(public_binding_add(state, (dy_String) {name.data, name.size}, ELF_PUBLIC_STRING, input));
}

elf_ErrorCode elf_pub_fun(elf_State *state, elf_StrSlice name, elf_Function function)
{
	PublicBindingInput input = {.function = function};
	return public_binding_error(public_binding_add(state, (dy_String) {name.data, name.size}, ELF_PUBLIC_FUNCTION, input));
}

void elf_public_bindings_destroy(elf_State *state)
{
	ASSERT(state);
	for (u32 i = 0; i < state->public_binding_count; ++i)
	{
		elf_PublicBinding *binding = state->public_bindings + i;
		elf_free(state, binding->name.data, binding->name.size + 1);
		if (binding->type == ELF_PUBLIC_STRING)
		{
			elf_free(state, binding->string.data, binding->string.size + 1);
		}
	}
	elf_free(state, state->public_bindings, (u64)state->public_binding_capacity * sizeof(*state->public_bindings));
	state->public_bindings = 0;
	state->public_binding_count = 0;
	state->public_binding_capacity = 0;
}
