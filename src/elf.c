//
// See Copyright Notice In elf.h
//

#include "elf.h"
#include "elf_runtime.h"

elf_SymTable *elf_open_core_libraries(elf_State *state);
void elf_open_host_libraries(elf_State *state, elf_SymTable *elf_symbols);
void elf_open_compiler_libraries(elf_State *state, elf_SymTable *elf_symbols);

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
