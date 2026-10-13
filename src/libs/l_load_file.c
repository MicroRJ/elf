//
// Optional source-file loading.
//

ELF_FUNCTION(l_core_load_file)
{
	lib_check_arg_count(S, "load_file", nargs, 1, INT_MAX);
	const char *name = lib_load_cstr(S, 1);
	if (!elf_push_code_file(S, name)) return 0;

	elf_push_value(S, 0);
	for (i64 index = 2; index < nargs; ++index)
	{
		elf_push_value(S, (elf_i32)index);
	}
	return elf_tail_call(S, nargs - 1, nrets);
}
