#ifndef ELF_BATTERY_HELPERS_H
#define ELF_BATTERY_HELPERS_H

#include "elf.h"

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef elf_b32 b32;
typedef elf_i32 i32;
typedef elf_u32 u32;
typedef elf_i64 i64;
typedef elf_u64 u64;
typedef elf_f64 f64;

typedef struct Battery_Binding
{
	const char *name;
	elf_Function function;
}
Battery_Binding;

typedef struct elf_Buffer
{
	char *data;
	size_t size;
	size_t capacity;
}
elf_Buffer;

static u32 battery_array_count(size_t size, size_t element_size)
{
	return (u32)(size / element_size);
}

static b32 buffer_reserve(elf_Buffer *buffer, size_t additional)
{
	if (additional <= buffer->capacity - buffer->size) return 1;
	size_t required = buffer->size + additional;
	size_t capacity = buffer->capacity ? buffer->capacity : 4096;
	while (capacity < required) capacity *= 2;
	char *data = realloc(buffer->data, capacity);
	if (!data) return 0;
	buffer->data = data;
	buffer->capacity = capacity;
	return 1;
}

static void buffer_destroy(elf_Buffer *buffer)
{
	free(buffer->data);
	*buffer = (elf_Buffer){0};
}

static void lib_check_arg_count(elf_State *state, const char *name,
	int nargs, int minimum, int maximum)
{
	int count = nargs - 1;
	if (count < minimum || count > maximum) {
		char message[256];
		if (minimum == maximum) {
			snprintf(message, sizeof(message), "%s expected %i argument(s), got %i",
				name, minimum, count);
		} else {
			snprintf(message, sizeof(message), "%s expected %i to %i arguments, got %i",
				name, minimum, maximum, count);
		}
		elf_error(state, message);
	}
}

static elf_StrSlice lib_load_string(elf_State *state, u32 index)
{
	elf_StrSlice value = {0};
	if (!elf_to_str(state, (elf_i32)index, &value)) {
		elf_error(state, "expected string argument");
	}
	return value;
}

static const char *lib_load_cstr(elf_State *state, u32 index)
{
	const char *value = "";
	if (!elf_to_cstr(state, (elf_i32)index, &value)) {
		elf_error(state, "expected string argument");
	}
	return value;
}

static i64 lib_load_integer(elf_State *state, u32 index)
{
	elf_Int value = 0;
	if (!elf_to_int(state, (elf_i32)index, &value)) {
		elf_error(state, "expected integer argument");
	}
	return value;
}

static void lib_push_string(elf_State *state, const char *data, u32 size)
{
	elf_push_str(state, data, (int)size);
}

static void lib_set_integer_field(elf_State *state, elf_i32 table,
	const char *name, i64 value)
{
	elf_push_int(state, value);
	elf_set_field(state, table, name);
}

static void lib_set_string_field(elf_State *state, elf_i32 table,
	const char *name, const char *data, u32 size)
{
	elf_push_str(state, data ? data : "", (int)size);
	elf_set_field(state, table, name);
}

static void lib_set_nil_field(elf_State *state, elf_i32 table, const char *name)
{
	elf_push_nil(state);
	elf_set_field(state, table, name);
}

static void new_binding_table(elf_State *state, const Battery_Binding *bindings, u32 count)
{
	elf_new_table(state);
	elf_i32 table = elf_abs_index(state, -1);
	for (u32 i = 0; i < count; ++i)
	{
		elf_push_fun(state, bindings[i].function);
		elf_set_field(state, table, bindings[i].name);
	}
}

#endif
