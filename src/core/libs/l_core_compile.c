//
// See Copyright Notice In elf.h
//

//
// Compiler-backed core operations.
//

static b32 lib_push_constant_expr(elf_State *state, const char *name, elf_StrSlice source)
{
	(void)state;
	(void)name;
	(void)source;
	return false;
}

ELF_FUNCTION(l_core_const_expr)
{
	lib_check_arg_count(S, "const_expr", nargs, 1, 1);
	elf_String *contents = lib_load_string(S, 1);
	elf_StrSlice source = {(char *)elf_string_data(contents), elf_string_size(contents)};
	if (!lib_push_constant_expr(S, "<const_expr>", source)) {
		elf_push_nil(S);
	}
	return 1;
}
