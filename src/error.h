//
// See Copyright Notice In elf.h
//

#ifndef ELF_ERROR_H
#define ELF_ERROR_H

#define ERROR_SEVERITY_X_LIST(X) \
	X(ERROR_NOTE,    "note") \
	X(ERROR_WARNING, "warning") \
	X(ERROR_ERROR,   "error") \
	X(ERROR_FATAL,   "fatal")

typedef enum
{
#define ERROR_SEVERITY_ENUM(name, text) name,
	ERROR_SEVERITY_X_LIST(ERROR_SEVERITY_ENUM)
#undef ERROR_SEVERITY_ENUM
	ERROR_SEVERITY_COUNT,
}
ErrorSeverity;

#define ERROR_PHASE_X_LIST(X) \
	X(ERROR_PHASE_SOURCE,   "source") \
	X(ERROR_PHASE_LEXER,    "lexer") \
	X(ERROR_PHASE_PARSER,   "parser") \
	X(ERROR_PHASE_CHECKER,  "checker") \
	X(ERROR_PHASE_LOWERING, "lowering") \
	X(ERROR_PHASE_BYTECODE, "bytecode") \
	X(ERROR_PHASE_RUNTIME,  "runtime")

typedef enum
{
#define ERROR_PHASE_ENUM(name, text) name,
	ERROR_PHASE_X_LIST(ERROR_PHASE_ENUM)
#undef ERROR_PHASE_ENUM
	ERROR_PHASE_COUNT,
}
ErrorPhase;

#endif
