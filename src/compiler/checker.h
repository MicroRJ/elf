#ifndef CHECKER_H
#define CHECKER_H

#include "compiler.h"
#include "entity.h"

enum {MAX_ENTITIES=4096};

typedef struct Checker Checker;
struct Checker {
	Compiler     *compiler;
	Scope        *globals;
	struct Ast_T *current_function;
	u32           scope_start;
	u32           entity_index;
	Entity       *entity_stack[MAX_ENTITIES];
};

Checker *checker_run(Compiler *compiler, struct Ast_T *tree);

#endif
