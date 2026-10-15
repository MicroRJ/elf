//
// See Copyright Notice In elf.h
//

#include "bytecode.h"

typedef struct Compiler Compiler;
typedef u32 Com_SourceId;

typedef enum
{
	COM_ERROR_NONE = 0,
	COM_ERROR_SOURCE,
	COM_ERROR_OOM,
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

#define COM_SOURCE_INVALID ((Com_SourceId)-1)

// Request and response shared with the host's module loader. The compiler
// supplies the import context and path; the loader returns identity and source.
typedef struct
{
	Compiler *compiler;
	Com_SourceId importer;
	struct Atom *path;
	void *user_data;
	struct Atom *name;
	struct Atom *canonical_path;
	elf_StrSlice source;
}
Com_ModuleLoadParams;

typedef b32 Com_ModuleLoader(Com_ModuleLoadParams *params);

// One source file taking part in a compilation. The compiler owns its copied
// text, parsed AST, and the imports discovered while parsing it.
typedef struct
{
	struct Atom *name;
	struct Atom *canonical_path;
	SourceBuffer source;
	struct Ast_T *ast;
	struct Ast_T **imports;
	u32 import_count;
	u32 import_capacity;
}
Com_Source;

// Raw text owned by the arena containing the compiled module.
typedef struct
{
	const char *data;
	u32         size;
}
Com_String;

// One source preserved in the compiled module for source maps and diagnostics.
typedef struct
{
	Com_String   name;
	Com_String   canonical_path;
	SourceBuffer source;
}
Com_ModuleSource;

// A global name referenced by the module. name indexes Com_Module.strings.
typedef struct
{
	u32 name;
}
Com_GlobalSymbol;

// Identifies bytecode whose module-local global index must be replaced with a
// runtime global slot when the compiled module is linked.
typedef struct
{
	u32 bytecode_index;
	u32 global_index;
}
Com_GlobalRelocation;

// One complete, runtime-independent compiler output. All referenced storage
// belongs to the module's output arena.
typedef struct Com_Module Com_Module;
struct Com_Module
{
	Com_ModuleSource *sources;
	u32               source_count;
	u32               root_source;

	Com_String *strings;
	u32         string_count;

	Com_GlobalSymbol     *globals;
	u32                   global_count;
	Com_GlobalRelocation *global_relocations;
	u32                   global_relocation_count;

	i64 *integer_constants;
	u32  integer_constant_count;
	f64 *number_constants;
	u32  number_constant_count;

	BcFunction *functions;
	u32         function_count;
	Bytecode   *bytecode;
	u32         bytecode_count;

	b32 has_entry;
	u32 entry_index;
};

typedef enum
{
	COM_UNIT_FUNCTION,
	COM_UNIT_MODULE,
}
Com_UnitType;

// Owns one compilation session: its sources, atoms, diagnostics, and temporary
// syntax and lowering data all share the caller-provided arena. Runtime work is
// outside this type.
struct Compiler
{
	elf_Arena         *arena;
	struct Atom_Table *atoms;
	Com_Source        *sources;
	u32                source_count;
	u32                source_capacity;
	Com_ModuleLoader  *module_loader;
	void              *module_loader_data;
	Com_Report         report;
	b32                log_messages;
};

Compiler *compiler_create(elf_Arena *arena);
Com_SourceId compiler_add_root_source(Compiler *compiler, const char *name, elf_StrSlice source);
void compiler_set_module_loader(Compiler *compiler, Com_ModuleLoader *loader, void *user_data);
void compiler_set_log_messages(Compiler *compiler, b32 enabled);
b32 compiler_record_import(Compiler *compiler, Com_SourceId source, struct Ast_T *import);
b32 compiler_parse_sources(Compiler *compiler);

b32 compiler_report(Compiler *compiler, Com_Severity severity, Com_Phase phase, SourceSite site, const char *format, ...);
b32 compiler_reportv(Compiler *compiler, Com_Severity severity, Com_Phase phase, SourceSite site, const char *format, va_list args);

Com_Module *compiler_compile(Compiler *compiler, elf_Arena *output_arena, Com_UnitType unit_type);
struct Ast_T *compiler_parse_constant_expr(Compiler *compiler);
struct Ast_T *compiler_parse_json(Compiler *compiler);
