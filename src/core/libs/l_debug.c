//
// See Copyright Notice In elf.h
//

//
// Runtime diagnostics. These are intentionally kept out of the root API.
//

ELF_FUNCTION(l_debug_memory_bytes)
{
	lib_check_arg_count(S, "debug.memory_bytes", nargs, 0, 0);
	push_value(S, value_from_integer(S->gc_live_bytes));
	return 1;
}

ELF_FUNCTION(l_debug_object_count)
{
	lib_check_arg_count(S, "debug.object_count", nargs, 0, 0);
	push_value(S, value_from_integer(S->gc_reference_count));
	return 1;
}

static const elf_Binding l_debug[] = {
	{"memory_bytes", l_debug_memory_bytes},
	{"object_count", l_debug_object_count},
};

static elf_Table *elf_lib_debug(elf_State *state)
{
	return new_binding_table(state, l_debug, ARRAY_COUNT(l_debug));
}
