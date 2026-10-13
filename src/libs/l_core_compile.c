//
// See Copyright Notice In elf.h
//

//
// Compiler-backed core operations.
//

ELF_FUNCTION(l_core_const_expr)
{
	lib_check_arg_count(S, "const_expr", nargs, 1, 1);
	elf_String *contents = lib_load_string(S, 1);
	elf_StrSlice source = {(char *)string_data(contents), string_size(contents)};
	if (elf_push_constant_expr(S, "<const_expr>", source) != ELF_ERROR_NONE) elf_push_nil(S);
	return 1;
}
