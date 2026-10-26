#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <dy.h>
#include <dy_fs.h>
#include <dy_process.h>
#include <dy_time.h>

#include "elf.h"
#include "elf_runtime.h"

#include "l_math.c"
#include "l_number.c"
#include "l_core.c"
#include "l_gc.c"
#include "l_table.c"
#include "l_array.c"
#include "l_string.c"
#include "l_buffer.c"
#include "l_error.c"

//
// Standard host libraries.
//

#include "l_core_io.c"
#include "l_env.c"
#include "l_fs.c"
#include "l_path.c"
#include "l_random.c"
#include "l_time.c"

elf_SymTable *elf_open_core_libraries(elf_State *state)
{
	ASSERT(state);
	elf_Value *stack_checkpoint = state->stack_top;

	state->metatables.string = elf_lib_string(state);
	state->metatables.integer = push_new_table(state);
	state->metatables.number = elf_lib_number(state);
	state->metatables.table = elf_lib_table(state);
	state->metatables.array = elf_lib_array(state);
	state->metatables.buffer = elf_lib_buffer(state);
	state->metatables.error = elf_lib_error(state);

	// The library constructors push each table while building the graph. The
	// metatables have dedicated roots in the collector.
	state->stack_top = stack_checkpoint;

	elf_SymTable *root = 0;
	elf_sym_create(&root);

	elf_SymTable *elf = elf_lib_core();

	elf_SymTable *gc = 0;
	elf_sym_create(&gc);

	elf_sym_fun(gc, ELF_LIT("counters"), l_gc_counters);
	elf_sym_fun(elf, ELF_LIT("buffer"), l_buffer_new);
	elf_sym_tab(elf, ELF_LIT("math"), elf_lib_math());
	elf_sym_tab(elf, ELF_LIT("gc"), gc);
	elf_sym_tab(root, ELF_LIT("elf"), elf);
	state->symbols = root;
	return elf;
}

void elf_open_host_libraries(elf_State *state, elf_SymTable *elf_symbols)
{
	elf_sym_tab(elf_symbols, ELF_LIT("env"), elf_lib_env());
	elf_sym_tab(elf_symbols, ELF_LIT("fs"), elf_lib_fs(state));
	elf_sym_tab(elf_symbols, ELF_LIT("path"), elf_lib_path());
	elf_sym_tab(elf_symbols, ELF_LIT("random"), elf_lib_random());
	elf_sym_tab(elf_symbols, ELF_LIT("time"), elf_lib_time());
	elf_sym_fun(elf_symbols, ELF_LIT("print"), l_core_print);
	elf_sym_fun(elf_symbols, ELF_LIT("println"), l_core_println);
	elf_sym_fun(elf_symbols, ELF_LIT("printl"), l_core_println);
}
