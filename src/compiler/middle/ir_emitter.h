//
// See Copyright Notice In elf.h
//

#ifndef IR_EMITTER_H
#define IR_EMITTER_H

typedef struct Compiler Compiler;

#include "checker.h"

IrModule *ir_emit_module(Compiler *compiler, Ast ast);

#endif