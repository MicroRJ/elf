//
// Standard host libraries.
//

#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <dy.h>
#include <dy_fs.h>
#include <dy_process.h>
#include <dy_time.h>

#include "elf.h"
#include "elf_core.h"
#include "libs.h"

#include "l_core_io.c"
#include "l_env.c"
#include "l_fs.c"
#include "l_path.c"
#include "l_random.c"
#include "l_time.c"

void elf_open_libraries(elf_State *state, elf_SymTable *elf_symbols)
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
