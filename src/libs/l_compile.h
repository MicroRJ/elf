#ifndef ELF_LIB_COMPILE_H
#define ELF_LIB_COMPILE_H

#include "elf.h"

typedef struct Compiler Compiler;
struct Ast_T;

elf_Bool lib_push_compiler_ast(elf_State *state, Compiler *compiler, struct Ast_T *ast);
elf_Bool lib_push_constant_expr(elf_State *state, const char *name, elf_StrSlice source);
elf_Bool lib_push_json(elf_State *state, const char *name, elf_StrSlice source);
elf_Bool lib_push_jsonl(elf_State *state, const char *name, elf_StrSlice source);

#endif
