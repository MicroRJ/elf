//
// See Copyright Notice In elf.h
//

#ifndef ELF_COMPILER_H
#define ELF_COMPILER_H

#include "bc_module.h"
#include "error.h"

typedef struct Compiler Compiler;
typedef u32 Com_SourceId;

typedef enum
{
	COM_STATUS_OK = 0,
	COM_STATUS_REPORTED,
	COM_STATUS_OOM,
}
Com_Status;

typedef struct Com_Message Com_Message;
struct Com_Message
{
	Com_Message *next;
	ErrorCode code;
	ErrorSeverity severity;
	ErrorPhase phase;
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
	Com_Source         *next;
	Com_SourceId        id;
	struct Atom        *name;
	struct Atom        *path;
	BcSourceBuffer      source;
	struct Ast_T       *ast;
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

Compiler *compiler_create(Arena *arena);
Com_Source *compiler_source_from_id(Compiler *compiler, Com_SourceId source_id);

void compiler_log_report(Compiler *compiler);

b32 compiler_report(Compiler *compiler, ErrorCode code, ErrorSeverity severity, ErrorPhase phase, SourceSite site, const char *format, ...);
b32 compiler_reportv(Compiler *compiler, ErrorCode code, ErrorSeverity severity, ErrorPhase phase, SourceSite site, const char *format, va_list args);

BcModule *compiler_compile(Arena *arena, elf_StrSlice name, elf_StrSlice source);
struct Ast_T *compiler_parse_constant_expr(Arena *arena, elf_StrSlice name, elf_StrSlice source);
struct Ast_T *compiler_parse_json(Arena *arena, elf_StrSlice name, elf_StrSlice source);
struct Ast_T *compiler_parse_jsonl(Arena *arena, elf_StrSlice name, elf_StrSlice source);

#endif
