//
// See Copyright Notice In elf.h
//

typedef struct Compiler Compiler;
typedef u32 Com_ModuleId;

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

#define COM_MODULE_INVALID ((Com_ModuleId)-1)

typedef struct
{
	Compiler *compiler;
	Com_ModuleId importer;
	struct Atom *path;
	void *user_data;
	struct Atom *name;
	struct Atom *canonical_path;
	elf_StrSlice source;
}
Com_ModuleLoadParams;

typedef b32 Com_ModuleLoader(Com_ModuleLoadParams *params);

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
Com_Module;

struct Compiler
{
	elf_State         *state;
	elf_Arena         *arena;
	struct Atom_Table *atoms;
	Com_Module        *modules;
	u32                module_count;
	u32                module_capacity;
	Com_ModuleLoader  *module_loader;
	void              *module_loader_data;
	Com_Report         report;
	b32                log_messages;
};

Compiler *compiler_create(elf_State *state, elf_Arena *arena);
Com_ModuleId compiler_add_root_module(Compiler *compiler, const char *name, elf_StrSlice source);
void compiler_set_module_loader(Compiler *compiler, Com_ModuleLoader *loader, void *user_data);
void compiler_set_log_messages(Compiler *compiler, b32 enabled);
void compiler_record_import(Compiler *compiler, Com_ModuleId module, struct Ast_T *import);
b32 compiler_parse_modules(Compiler *compiler);

b32 compiler_report(Compiler *compiler, Com_Severity severity, Com_Phase phase, SourceSite site, const char *format, ...);
b32 compiler_reportv(Compiler *compiler, Com_Severity severity, Com_Phase phase, SourceSite site, const char *format, va_list args);

BcFunctionRef compiler_compile(Compiler *compiler);
int compiler_push_constant_expr(Compiler *compiler);
int compiler_push_json(Compiler *compiler);

BcFunctionRef elf_compile_source(elf_State *state, const char *name, elf_StrSlice source);
int elf_push_constant_expr_source(elf_State *state, const char *name, elf_StrSlice source);
int elf_push_json_source(elf_State *state, const char *name, elf_StrSlice source);
