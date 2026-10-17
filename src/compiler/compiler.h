//
// See Copyright Notice In elf.h
//

#ifndef ELF_COMPILER_H
#define ELF_COMPILER_H

#include "bytecode.h"

typedef struct Compiler Compiler;
typedef u32 Com_SourceId;

typedef enum
{
	COM_STATUS_OK = 0,
	COM_STATUS_REPORTED,
	COM_STATUS_OOM,
}
Com_Status;

typedef enum
{
	COM_ERROR_NONE = 0,
	COM_ERROR_INVALID_SOURCE,
	COM_ERROR_SOURCE_PATH,
	COM_ERROR_IMPORT,
	COM_ERROR_PARSE_FAILED,
	COM_ERROR_INVALID_UNICODE_ESCAPE,
	COM_ERROR_UNTERMINATED_ESCAPE_SEQUENCE,
	COM_ERROR_INVALID_ESCAPE_SEQUENCE,
	COM_ERROR_INVALID_NUMBER_LITERAL,
	COM_ERROR_INTEGER_LITERAL_OVERFLOW,
	COM_ERROR_UNTERMINATED_STRING,
	COM_ERROR_NEWLINE_IN_STRING,
	COM_ERROR_UNTERMINATED_COMMENT,
	COM_ERROR_UNTERMINATED_INTERPOLATION,
	COM_ERROR_INVALID_CHARACTER_CONSTANT,
	COM_ERROR_EXPECTED_MACRO_NAME,
	COM_ERROR_UNRECOGNIZED_MACRO,
	COM_ERROR_NUL_BYTE_IN_SOURCE,
	COM_ERROR_UNEXPECTED_CHARACTER,
	COM_ERROR_UNEXPECTED_TOKEN,
	COM_ERROR_INVALID_EXPRESSION,
	COM_ERROR_EXPECTED_TOKEN,
	COM_ERROR_INVALID_DECLARATION,
	COM_ERROR_DUPLICATE_DECLARATION,
	COM_ERROR_SYNTAX_DEPRECATION,
	COM_ERROR_INVALID_FIELD_INITIALIZER,
	COM_ERROR_EXCESS_COMMA,
	COM_ERROR_MISSING_FUNCTION_BODY,
	COM_ERROR_INVALID_STATEMENT,
	COM_ERROR_STATIC_LOOKUP,
	COM_ERROR_INVALID_STATIC_EXPRESSION,
	COM_ERROR_CYCLIC_STATIC_DEFINITION,
	COM_ERROR_INVALID_FUNCTION_PARAMETER,
	COM_ERROR_INVALID_STATIC_DECLARATION,
	COM_ERROR_DUPLICATE_STATIC_DECLARATION,
	COM_ERROR_EXECUTABLE_MODULE_STATEMENT,
	COM_ERROR_LOWERING,
	COM_ERROR_INTERNAL,
	COM_ERROR_UNREFERENCED_ENTITY,
	COM_ERROR_UNDECLARED_IDENTIFIER,
}
Com_Error;

typedef enum
{
	COMPILER_NOTE = 0,
	COMPILER_WARNING,
	COMPILER_ERROR,
}
Com_Severity;

typedef enum
{
	COMPILER_PHASE_LEXER = 0,
	COMPILER_PHASE_PARSER,
	COMPILER_PHASE_EVALUATION,
	COMPILER_PHASE_LOWERING,
	COMPILER_PHASE_BYTECODE,
}
Com_Phase;

typedef struct Com_Message Com_Message;
struct Com_Message
{
	Com_Message *next;
	Com_Error error;
	Com_Severity severity;
	Com_Phase phase;
	SourceSite site;
	elf_StrSlice text;
};

typedef struct
{
	Com_Message *first;
	Com_Message *last;
	u32 message_count;
	u32 error_count;
	u32 warning_count;
}
Com_Report;

#define COM_INVALID_SOURCE_ID ((Com_SourceId)-1)

typedef enum
{
	COM_SOURCE_UNPARSED = 0,
	COM_SOURCE_PARSED,
	COM_SOURCE_CHECKING,
	COM_SOURCE_CHECKED,
}
Com_SourceState;

typedef struct Com_Source Com_Source;
struct Com_Source
{
	Com_Source        *next;
	Com_SourceId       id;
	struct Atom        *name;
	struct Atom        *path;
	BcSourceBuffer      source;
	struct Ast_T       *ast;
	// struct Scope       *exports;
	struct Scope       *scope;
	Com_SourceState     state;
};

// NOTE(RJ):
// This will remain only the shared stuff across compilation stages;
// Error reporting sink, logging configurations apparently, compile wide allocations
// and sources ...
struct Compiler
{
	Arena             *arena;
	struct Atom_Table *atoms;
	Com_Source        *first_source;
	Com_Source        *last_source;
	u32                source_count;
	Com_Report         report;
};

Compiler *compiler_create(elf_Arena *arena);
Com_Source *compiler_source_from_id(Compiler *compiler, Com_SourceId source_id);

void compiler_log_report(Compiler *compiler);

b32 compiler_report(Compiler *compiler, Com_Error error, Com_Severity severity, Com_Phase phase, SourceSite site, const char *format, ...);
b32 compiler_reportv(Compiler *compiler, Com_Error error, Com_Severity severity, Com_Phase phase, SourceSite site, const char *format, va_list args);

BcModule *compiler_compile(elf_Arena *arena, elf_StrSlice name, elf_StrSlice source);
struct Ast_T *compiler_parse_constant_expr(elf_Arena *arena, elf_StrSlice name, elf_StrSlice source);
struct Ast_T *compiler_parse_json(elf_Arena *arena, elf_StrSlice name, elf_StrSlice source);
struct Ast_T *compiler_parse_jsonl(elf_Arena *arena, elf_StrSlice name, elf_StrSlice source);

#endif
