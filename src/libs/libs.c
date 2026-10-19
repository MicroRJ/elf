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

#include "l_core_io.c"
#include "l_load_file.c"
#include "l_json.c"
#include "l_env.c"
#include "l_fs.c"
#include "l_path.c"
#include "l_random.c"
#include "l_time.c"

elf_Bool elf_push_code_file(elf_State *state, const char *name)
{
	if (!name) {
		elf_push_nil(state);
		return false;
	}

	Scratch scratch = scratch_begin();
	elf_StrSlice source = {};
	dy_File_Info info;
	dy_File file = {};
	b32 root_loaded = !dy_get_file_info(dy_string_from_cstring(name), &info).error && !info.is_directory;
	if (root_loaded) {
		root_loaded = !dy_access_file(dy_string_from_cstring(name), DY_FILE_OPEN_EXISTING, DY_FILE_READ | DY_FILE_SHARE_READ, &file).error;
	}
	if (root_loaded)
	{
		char *data = arena_push(scratch.arena, info.size + 16, 1);
		zero_memory(data + info.size, 16);
		dy_u64 read = 0;
		dy_Result read_result = dy_read_file(file, data, info.size, &read);
		dy_close_file(file);
		root_loaded = !read_result.error && read == info.size;
		if (root_loaded) {
			source = (elf_StrSlice) {data, info.size};
		}
	}
	elf_StrSlice source_name = {(char *)name, strlen(name)};
	elf_Prototype *prototype = root_loaded ? compiler_compile(state, scratch.arena, source_name, source) : 0;
	scratch_end(scratch);

	if (!prototype)
	{
		if (!root_loaded) {
			elf_push_nil(state);
		}
		return false;
	}

	ASSERT(prototype->captures == 0);
	elf_Closure *closure = elf_closure_new(state, prototype, 0, 0);
	if (!closure) {
		return false;
	}
	push_value(state, value_from_closure(closure));
	return true;
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
	elf_lib_random(state);        elf_set_field(state, root, "random");
	elf_lib_time(state);          elf_set_field(state, root, "time");
	elf_push_fun(state, l_core_load_file); elf_set_field(state, root, "load_file");
	elf_push_fun(state, l_core_print);     elf_set_field(state, root, "print");
	elf_push_fun(state, l_core_println);   elf_set_field(state, root, "println");
	elf_push_fun(state, l_core_println);   elf_set_field(state, root, "printl");

cleanup:
	elf_set_top(state, checkpoint);
}
