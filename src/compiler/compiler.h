//
// See Copyright Notice In elf.h
//

typedef elf_SourceBuffer SourceBuffer;

BytecodeFunction elf_compile_source(elf_State *state, const char *name, SourceBuffer source);

void elf_init_compiler_atoms(elf_State *state);

int elf_push_constant_expr_source(elf_State *state, const char *name, SourceBuffer source);
int elf_push_json_source(elf_State *state, const char *name, SourceBuffer source);
