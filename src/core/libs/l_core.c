//
// See Copyright Notice In elf.h
//

//
// Root standard-library bindings.
//

#include "l_core_values.c"
#include "l_core_calls.c"
#include "l_core_compile.c"

static const elf_Binding l_core[] = {
	{"assert",       l_core_assert},
	{"metatable",    l_core_metatable},
	{"freeze",       l_core_freeze},
	{"is_readonly",  l_core_is_readonly},

	{"type_of",      l_core_type_of},
	{"to_string",    l_core_to_string},
	{"is_atom",      l_core_is_atom},
	{"is_numeric",   l_core_is_numeric},
	{"to_number",    l_core_to_number},
	{"to_integer",   l_core_to_integer},

	{"nvargs",       l_core_nvargs},
	{"varg",         l_core_varg},
	{"nrets",        l_core_nrets},
	{"nargs",        l_core_nargs},
	{"arg",          l_core_arg},

	{"const_expr",   l_core_const_expr},
};

static elf_Table *elf_lib_core(elf_State *state)
{
	return new_binding_table(state, l_core, ARRAY_COUNT(l_core));
}
