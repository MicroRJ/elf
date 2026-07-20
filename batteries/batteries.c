//
// Optional batteries for the official Windows host.
//

#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "elf_batteries.h"
#include "base.h"
#include "platform.h"
#include "battery_console.h"
#include "battery_file.h"
#include "battery_process.h"
#include "battery_time.h"
#include "core.h"
#include "helpers.h"
#include "compiler.h"
#include "value_text.h"

STATIC_ASSERT(sizeof(elf_PlatformFile) >= sizeof(HANDLE));

static HANDLE win32_handle(elf_PlatformFile file)
{
	return (HANDLE)(uintptr_t)file;
}

static elf_PlatformFile elf_platform_file_from_win32(HANDLE handle)
{
	return (elf_PlatformFile)(uintptr_t)handle;
}

static elf_Table *battery_new_binding_table(elf_State *state,
	const elf_Binding *bindings, u32 count)
{
	elf_Table *table = elf_push_new_table(state);
	for (u32 i = 0; i < count; ++i)
	{
		elf_Value key = value_from_atom(elf_atom_from_data(state, bindings[i].name));
		elf_table_set(state, table, key, value_from_function(bindings[i].function));
	}
	return table;
}

// The core library sources use the constructor spelling. Keep that
// helper local to the batteries translation unit.
#define new_binding_table battery_new_binding_table

#include "win32/battery_console_win32.c"
#include "win32/battery_file_win32.c"
#include "win32/battery_process_win32.c"
#include "win32/battery_time_win32.c"

#include "../src/core/libs/l_native.c"
#include "libs/l_core_io.c"
#include "libs/l_load_file.c"
#include "libs/l_serialization.c"
#include "libs/l_fs.c"
#include "libs/l_path.c"
#include "libs/l_process.c"
#include "libs/l_random.c"
#include "libs/l_time.c"

static elf_StrSlice battery_source_buffer_from_file(elf_State *state, const char *name)
{
	elf_StrSlice source = {0};
	elf_PlatformFile file = elf_platform_open_file(name,
		ELF_PLATFORM_OPEN_READ, ELF_PLATFORM_OPEN_EXISTING);
	if (ELF_IS_HANDLE_INVALID(file)) return source;

	i64 file_size = elf_platform_file_size(file);
	if (file_size < 0 || (u64)file_size > UINT_MAX)
	{
		elf_platform_close_file(file);
		return source;
	}

	char *data = elf_arena_push(&state->arena, (u64)file_size + 16);
	zero_memory(data + file_size, 16);
	i64 read = elf_platform_read_file(file, data, file_size);
	elf_platform_close_file(file);
	if (read != file_size) return source;

	source.data = data;
	source.size = (u64)file_size;
	return source;
}

int elf_push_code_file(elf_State *state, const char *name)
{
	ASSERT(name);
	elf_StrSlice source = battery_source_buffer_from_file(state, name);
	if (!source.data)
	{
		elf_push_nil(state);
		return false;
	}
	return elf_push_code_source(state, name, source);
}

static void battery_set_table(elf_State *state, elf_Table *parent,
	const char *name, elf_Table *table)
{
	elf_Value key = value_from_atom(elf_atom_from_data(state, name));
	elf_table_set(state, parent, key, value_from_table(table));
}

static void battery_set_function(elf_State *state, elf_Table *parent,
	const char *name, elf_Function function)
{
	elf_Value key = value_from_atom(elf_atom_from_data(state, name));
	elf_table_set(state, parent, key, value_from_function(function));
}

void elf_open_batteries(elf_State *state)
{
	ASSERT(state);
	elf_Value *stack_checkpoint = state->stack_ptr;
	elf_Value key = value_from_atom(elf_atom_from_data(state, "elf"));
	elf_Value root_value = elf_table_get_or_nil(state, state->globals, key);
	ASSERT(value_is_table(root_value));
	elf_Table *root = value_as_table(root_value);

	battery_set_table(state, root, "serialization", elf_lib_serialization(state));
	battery_set_table(state, root, "fs", elf_lib_fs(state));
	battery_set_table(state, root, "path", elf_lib_path(state));
	battery_set_table(state, root, "process", elf_lib_process(state));
	battery_set_table(state, root, "random", elf_lib_random(state));
	battery_set_table(state, root, "time", elf_lib_time(state));
	battery_set_function(state, root, "load_file", l_core_load_file);
	battery_set_function(state, root, "print", l_core_print);
	battery_set_function(state, root, "println", l_core_println);
	battery_set_function(state, root, "printl", l_core_println);

	state->stack_ptr = stack_checkpoint;
}
