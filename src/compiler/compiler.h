//
// See Copyright Notice In elf.h
//

#ifndef ELF_COMPILER_H
#define ELF_COMPILER_H

#include "elf.h"
#include "base.h"
#include "source.h"
#include "error.h"

typedef struct Compiler Compiler;
typedef struct elf_Prototype elf_Prototype;
typedef struct elf_Source elf_Source;
typedef u32 Com_SourceId;

// Status for control flow based exception handling, it'll probably take a while to fully cover everything ...
typedef enum
{
	COM_STATUS_OK = 0,
	COM_STATUS_REPORTED,
	COM_STATUS_OOM,
}
Com_Status;

// Compiler wide diagnostic messages
typedef struct Com_Message Com_Message;
struct Com_Message
{
	Com_Message   *next;
	ErrorCode     code;
	ErrorSeverity severity;
	ErrorPhase    phase;
	SourceSite    site;
	elf_StrSlice  text;
};

// Diagnostic message container
typedef struct
{
	Com_Message *first;
	Com_Message *last;
	u32 message_count;
	u32 error_count;
	u32 warning_count;
}
Com_Report;

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
	Compiler           *compiler;
	Com_Source         *next;
	Com_SourceId        id;
	struct Atom        *name;
	struct Atom        *path;
	elf_Source         *runtime_source;
	BcSourceBuffer      source;
	struct Ast_T       *ast;
	struct Scope       *scope;
	Com_SourceState     state;
	Com_Report          report;
};

// NOTE(RJ):
// This will remain only the shared stuff across compilation stages;
// Error reporting sink, logging configurations apparently, compile wide allocations
// and sources ...
struct Compiler
{
	elf_State         *target;
	Arena             *arena;
	struct Atom_Table *atoms;
	Com_Source        *first_source;
	Com_Source        *last_source;
	u32                source_count;
	u32                error_count;
	u32                warning_count;
};

Compiler *compiler_create(Arena *arena);

void compiler_log_report(Compiler *compiler);

b32 compiler_report(Com_Source *source, ErrorCode code, ErrorSeverity severity, ErrorPhase phase, SourceSite site, const char *format, ...);
b32 compiler_reportv(Com_Source *source, ErrorCode code, ErrorSeverity severity, ErrorPhase phase, SourceSite site, const char *format, va_list args);

elf_Prototype *compiler_compile(elf_State *target, Arena *arena, elf_StrSlice name, elf_StrSlice source);
struct Ast_T *compiler_parse_constant_expr(Arena *arena, elf_StrSlice name, elf_StrSlice source);
struct Ast_T *compiler_parse_json(Arena *arena, elf_StrSlice name, elf_StrSlice source);
struct Ast_T *compiler_parse_jsonl(Arena *arena, elf_StrSlice name, elf_StrSlice source);

#endif
