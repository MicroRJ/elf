//
// Compiler-backed core operations.
//

ELF_FUNCTION(l_core_load_file)
{
	lib_check_arg_count(S, "load_file", nargs, 1, INT_MAX);
	const char *name = lib_load_cstr(S, 1);
	if (!elf_push_code_file(S, name)) return 0;

	push_value(S, load_value(S, 0));
	for (i64 index = 2; index < nargs; ++index)
	{
		push_value(S, load_value(S, index));
	}
	return elf_tail_call(S, nargs - 1, nrets);
}

ELF_FUNCTION(l_core_const_expr)
{
	lib_check_arg_count(S, "const_expr", nargs, 1, 1);
	elf_String *contents = lib_load_string(S, 1);
	elf_StrSlice source = {(char *)atom_data(contents), atom_size(contents)};
	elf_push_constant_expr(S, "<const_expr>", source);
	return 1;
}
