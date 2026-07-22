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

#undef ELF_VERSION
#include "elf_batteries.h"
#include "battery_helpers.h"
#include "battery_console.h"
#include "battery_file.h"
#include "battery_process.h"
#include "battery_time.h"

_Static_assert(sizeof(elf_PlatformFile) >= sizeof(HANDLE), "file handle is too small");

static HANDLE win32_handle(elf_PlatformFile file)
{
	return (HANDLE)(uintptr_t)file;
}

static elf_PlatformFile elf_platform_file_from_win32(HANDLE handle)
{
	return (elf_PlatformFile)(uintptr_t)handle;
}

#include "win32/battery_console_win32.c"
#include "win32/battery_file_win32.c"
#include "win32/battery_process_win32.c"
#include "win32/battery_time_win32.c"

#include "libs/l_core_io.c"
#include "libs/l_load_file.c"
#include "libs/l_serialization.c"
#include "libs/l_fs.c"
#include "libs/l_path.c"
#include "libs/l_process.c"
#include "libs/l_random.c"
#include "libs/l_time.c"

static elf_StrSlice battery_source_buffer_from_file(const char *name)
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

	char *data = calloc(1, (size_t)file_size + 16);
	if (!data) {
		elf_platform_close_file(file);
		return source;
	}
	i64 read = elf_platform_read_file(file, data, file_size);
	elf_platform_close_file(file);
	if (read != file_size) {
		free(data);
		return source;
	}

	source.data = data;
	source.size = (u64)file_size;
	return source;
}

int elf_push_code_file(elf_State *state, const char *name)
{
	if (!name) {
		elf_push_nil(state);
		return false;
	}
	elf_StrSlice source = battery_source_buffer_from_file(name);
	if (!source.data)
	{
		elf_push_nil(state);
		return false;
	}
	int result = elf_push_code_source(state, name, source);
	free(source.data);
	return result;
}

void elf_open_batteries(elf_State *state)
{
	if (!state) return;
	elf_i32 checkpoint = elf_get_top(state);
	elf_get_global(state, "elf");
	elf_i32 root = elf_abs_index(state, -1);
	if (elf_type(state, root) != ELF_VALUE_TYPE_TABLE) goto cleanup;

	elf_lib_serialization(state); elf_set_field(state, root, "serialization");
	elf_lib_fs(state);            elf_set_field(state, root, "fs");
	elf_lib_path(state);          elf_set_field(state, root, "path");
	elf_lib_process(state);       elf_set_field(state, root, "process");
	elf_lib_random(state);        elf_set_field(state, root, "random");
	elf_lib_time(state);          elf_set_field(state, root, "time");
	elf_push_fun(state, l_core_load_file); elf_set_field(state, root, "load_file");
	elf_push_fun(state, l_core_print);     elf_set_field(state, root, "print");
	elf_push_fun(state, l_core_println);   elf_set_field(state, root, "println");
	elf_push_fun(state, l_core_println);   elf_set_field(state, root, "printl");

cleanup:
	elf_set_top(state, checkpoint);
}
