//
// Standard host libraries.
//

#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <dy.h>

#include "elf.h"
#include "elf_core.h"
#include "compiler.h"
#include "libs.h"

#include "l_compile.c"

#include "l_core_io.c"
#include "l_io.c"
#include "l_load_file.c"
#include "l_json.c"
#include "l_env.c"
#include "l_fs.c"
#include "l_path.c"
#include "l_process.c"
#include "l_random.c"
#include "l_time.c"

static elf_StrSlice lib_source_buffer_from_file(elf_State *state, const char *name)
{
	elf_StrSlice source = {0};
	dy_File_Info info;
	if (dy_get_file_info(dy_string_from_cstring(name), &info).error || info.is_directory || info.size > UINT_MAX) return source;
	dy_File file;
	if (dy_access_file(dy_string_from_cstring(name), DY_FILE_OPEN_EXISTING,
		DY_FILE_READ | DY_FILE_SHARE_READ, &file).error) return source;

	u64 allocation_size = info.size + 16;
	char *data = elf_alloc_zero(state, allocation_size);
	if (!data) {
		dy_close_file(file);
		return source;
	}
	dy_u64 read = 0;
	dy_Result read_result = dy_read_file(file, data, info.size, &read);
	dy_close_file(file);
	if (read_result.error || read != info.size) {
		elf_free(state, data, allocation_size);
		return source;
	}

	source.data = data;
	source.size = info.size;
	return source;
}

elf_Bool elf_push_code_file(elf_State *state, const char *name)
{
	if (!name) {
		elf_push_nil(state);
		return false;
	}
	elf_StrSlice source = lib_source_buffer_from_file(state, name);
	if (!source.data)
	{
		elf_push_nil(state);
		return false;
	}
	elf_ErrorCode result = elf_push_code_source(state, name, source);
	elf_free(state, source.data, source.size + 16);
	return result == ELF_ERROR_NONE;
}

void elf_open_libraries(elf_State *state)
{
	if (!state) return;
	elf_i32 checkpoint = elf_get_top(state);
	elf_get_global(state, "elf");
	elf_i32 root = elf_abs_index(state, -1);
	if (elf_type(state, root) != ELF_VALUE_TYPE_TABLE) goto cleanup;

	elf_lib_json(state);          elf_set_field(state, root, "json");
	elf_lib_env(state);           elf_set_field(state, root, "env");
	elf_lib_fs(state);            elf_set_field(state, root, "fs");
	elf_lib_path(state);          elf_set_field(state, root, "path");
	elf_lib_process(state);       elf_set_field(state, root, "process");
	elf_lib_random(state);        elf_set_field(state, root, "random");
	elf_lib_time(state);          elf_set_field(state, root, "time");
	elf_lib_io(state);            elf_set_field(state, root, "io");
	elf_push_fun(state, l_core_load_file); elf_set_field(state, root, "load_file");
	elf_push_fun(state, l_core_print);     elf_set_field(state, root, "print");
	elf_push_fun(state, l_core_println);   elf_set_field(state, root, "println");
	elf_push_fun(state, l_core_println);   elf_set_field(state, root, "printl");

cleanup:
	elf_set_top(state, checkpoint);
}
