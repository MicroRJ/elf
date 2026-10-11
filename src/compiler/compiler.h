//
// See Copyright Notice In elf.h
//

typedef struct Compiler Compiler;
typedef u32 Com_ModuleId;

typedef struct
{
	Compiler *compiler;
	Com_ModuleId importer;
	struct Atom *path;
	void *user_data;
	struct Atom *name;
	struct Atom *canonical_path;
	SourceBuffer source;
}
Com_ModuleLoadParams;

typedef b32 Com_ModuleLoader(Com_ModuleLoadParams *params);

typedef struct
{
	struct Atom *name;
	struct Atom *canonical_path;
	SourceBuffer source;
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
	elf_Diagnostic    *diagnostics;
	u32                diagnostic_count;
	u32                diagnostic_capacity;
	u32                error_count;
	u32                warning_count;
	b32                log_diagnostics;
};

Compiler *compiler_create(elf_State *state, elf_Arena *arena, const char *name, elf_StrSlice source);
void compiler_set_module_loader(Compiler *compiler, Com_ModuleLoader *loader, void *user_data);
void compiler_report(Compiler *compiler, elf_DiagnosticSeverity severity, elf_DiagnosticPhase phase, SourceSite site, const char *format, ...);
void compiler_reportv(Compiler *compiler, elf_DiagnosticSeverity severity, elf_DiagnosticPhase phase, SourceSite site, const char *format, va_list args);
void compiler_finish_report(Compiler *compiler, elf_CompileReport *report);

BcFunctionRef elf_compile_source(elf_State *state, const char *name, elf_StrSlice source, elf_CompileReport *report);
int elf_push_constant_expr_source(elf_State *state, const char *name, elf_StrSlice source, elf_CompileReport *report);
int elf_push_json_source(elf_State *state, const char *name, elf_StrSlice source, elf_CompileReport *report);
