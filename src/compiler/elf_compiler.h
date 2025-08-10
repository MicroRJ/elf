//
// See Copyright Notice In elf.h
//

// if < 0 then an error occurred, otherwise, if success, the result is an index into
// the proto-array where you can get the prototype and execute it as a closure
int elf_makefile(elf_State *S, const char *name);


// " parses a constant expression "
// return value indicates success, the result is on the stack
int elf_load_const_expr_from_text(elf_State *S, const char *name, const char *contents);



// " parses JSON as a table "
// return value is the the JSON object
int elf_load_json(elf_State *S, const char *name, const char *contents);