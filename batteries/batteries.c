//
// Optional batteries for the official Windows host.
//

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include <platform.h>

#undef ELF_VERSION
#include "elf_batteries.h"
#include "battery_helpers.h"

#include "libs/l_core_io.c"
#include "libs/l_load_file.c"
#include "libs/l_serialization.c"
#include "libs/l_env.c"
#include "libs/l_fs.c"
#include "libs/l_path.c"
#include "libs/l_process.c"
#include "libs/l_random.c"
#include "libs/l_time.c"

static elf_StrSlice battery_source_buffer_from_file(const char *name)
{
	elf_StrSlice source = {0};
	Platform_File_Info info;
	if (!platform_get_file_info(name, &info) || info.is_directory || info.size > UINT_MAX) return source;
	Platform_File file = platform_access_file(name, PLATFORM_FILE_OPEN_EXISTING, PLATFORM_FILE_READ | PLATFORM_FILE_SHARE_READ);
	if (!platform_file_is_valid(file)) return source;

	char *data = calloc(1, (size_t)info.size + 16);
	if (!data) {
		platform_close_file(file);
		return source;
	}
	U64 read = 0;
	B32 success = platform_read_file(file, data, info.size, &read);
	platform_close_file(file);
	if (!success || read != info.size) {
		free(data);
		return source;
	}

	source.data = data;
	source.size = info.size;
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
	elf_lib_env(state);           elf_set_field(state, root, "env");
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
