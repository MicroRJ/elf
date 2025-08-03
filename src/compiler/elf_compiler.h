//
// See Copyright Notice In elf.h
//

elf_Proto elf_compile(elf_State *S, elf_String *name, elf_String *contents, bool as_expr);

// " parses a constant expression "
// return value indicates success, the result is on the stack
int elf_load_const_expr(elf_State *S, char *name, char *contents);

// " parses JSON as a table "
// return value is the the JSON object
int elf_load_json(elf_State *S, char *name, char *contents);