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
typedef struct Com_Source Com_Source;

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

struct Compiler
{
	elf_State         *target;
	Arena             *arena;
	struct Atom_Table *atoms;
	struct Scope      *globals;
	Com_Source        *first_source;
	Com_Source        *last_source;
	u32                error_count;
	u32                warning_count;
};

struct Com_Source
{
	Compiler           *compiler;
	elf_Source         *runtime_source;
	Com_Source         *next;
	struct Atom        *name;
	struct Atom        *path;
	dy_String           source;
	struct Ast_T       *ast;
	struct Ast_T      **imports;
	u32                 import_count;
	struct Scope       *scope;
	Com_SourceState     state;
	Com_Report          report;
};

#endif
