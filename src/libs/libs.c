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

elf_Bool elf_push_code_file(elf_State *state, const char *name)
{
	if (!name) {
		elf_push_nil(state);
		return false;
	}

	elf_Scratch scratch = elf_begin_scratch();
	Compiler *compiler = compiler_create(scratch.arena);
	Com_SourceId root = compiler_add_root_file(compiler, name);
	b32 root_loaded = root != COM_SOURCE_INVALID;
	Com_Module *compiled_module = root != COM_SOURCE_INVALID ? compiler_compile(compiler, scratch.arena, COM_UNIT_FUNCTION) : 0;
	elf_Module *module = elf_link_module(state, compiled_module);
	BcFunctionRef entry = {};
	if (module && compiled_module->has_entry)
	{
		entry.module = module;
		entry.index = compiled_module->entry_index;
	}
	elf_end_scratch(scratch);

	if (!bc_function_ref_is_valid(entry))
	{
		if (!root_loaded) {
			elf_push_nil(state);
		}
		return false;
	}

	ASSERT(bc_function_from_ref(entry)->captures == 0);
	elf_Closure *closure = elf_gc_alloc(state, ELF_OBJECT_CLOSURE, sizeof(*closure));
	closure->function = entry;
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
