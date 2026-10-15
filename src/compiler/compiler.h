//
// See Copyright Notice In elf.h
//

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

typedef struct
{
	elf_Module    *module;
	BcFunctionRef entry;
}
Com_Output;

typedef enum
{
	COM_UNIT_FUNCTION,
	COM_UNIT_MODULE,
}
Com_UnitType;

// Owns one compilation session: its sources, atoms, diagnostics, and temporary
// syntax and lowering data all share the caller-provided arena.
struct Compiler
{
	elf_State         *state;
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

Compiler *compiler_create(elf_State *state, elf_Arena *arena);
Com_SourceId compiler_add_root_source(Compiler *compiler, const char *name, elf_StrSlice source);
void compiler_set_module_loader(Compiler *compiler, Com_ModuleLoader *loader, void *user_data);
void compiler_set_log_messages(Compiler *compiler, b32 enabled);
b32 compiler_record_import(Compiler *compiler, Com_SourceId source, struct Ast_T *import);
b32 compiler_parse_sources(Compiler *compiler);

b32 compiler_report(Compiler *compiler, Com_Severity severity, Com_Phase phase, SourceSite site, const char *format, ...);
b32 compiler_reportv(Compiler *compiler, Com_Severity severity, Com_Phase phase, SourceSite site, const char *format, va_list args);

Com_Output compiler_compile(Compiler *compiler, Com_UnitType unit_type);
int compiler_push_constant_expr(Compiler *compiler);
int compiler_push_json(Compiler *compiler);

BcFunctionRef elf_compile_source(elf_State *state, const char *name, elf_StrSlice source);
int elf_push_constant_expr_source(elf_State *state, const char *name, elf_StrSlice source);
int elf_push_json_source(elf_State *state, const char *name, elf_StrSlice source);
