//
// See Copyright Notice In elf.h
//

#include "elf.h"
#include "elf_core.h"
#include "libs.h"
#include "lib_com.h"

elf_State *elf_create_state(void)
{
	elf_State *state = elf_runtime_create_state();
	if (!state)
	{
		return 0;
	}

	elf_SymTable *elf_symbols = elf_open_core_libraries(state);
	elf_open_host_libraries(state, elf_symbols);
	elf_open_compiler_libraries(state, elf_symbols);
	return state;
}
