//
// See Copyright Notice In elf.h
//

typedef elf_StrSlice elf_StrSlice;

BcFunctionRef elf_compile_source(elf_State *state, const char *name, elf_StrSlice source);

void elf_init_compiler_strings(elf_State *state);

int elf_push_constant_expr_source(elf_State *state, const char *name, elf_StrSlice source);
int elf_push_json_source(elf_State *state, const char *name, elf_StrSlice source);
