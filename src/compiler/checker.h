#ifndef CHECKER_H
#define CHECKER_H

#include "compiler.h"
#include "entity.h"

typedef struct Checker Checker;
struct Checker {
	Compiler     *compiler;
	Scope        *globals;
	Scope        *source_scope;
	Scope        *active_scope;
	struct Ast_T *current_function;
};

Checker *check_source(Compiler *compiler, Com_Source *source);

#endif
