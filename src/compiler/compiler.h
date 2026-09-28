//
// See Copyright Notice In elf.h
//

typedef struct Compiler Compiler;

struct Compiler
{
	elf_State      *state;
	elf_Arena      *arena;
	const char     *source_name;
	elf_StrSlice    source;
	elf_Diagnostic *diagnostics;
	u32             diagnostic_count;
	u32             diagnostic_capacity;
	u32             error_count;
	u32             warning_count;
};

Compiler *compiler_create(elf_State *state, elf_Arena *arena, const char *name, elf_StrSlice source);
void compiler_report(Compiler *compiler, elf_DiagnosticSeverity severity, elf_DiagnosticPhase phase,
	SourceSite site, const char *format, ...);
void compiler_reportv(Compiler *compiler, elf_DiagnosticSeverity severity, elf_DiagnosticPhase phase,
	SourceSite site, const char *format, va_list args);
void compiler_finish_report(Compiler *compiler, elf_CompileReport *report);

BcFunctionRef elf_compile_source(elf_State *state, const char *name, elf_StrSlice source, elf_CompileReport *report);
int elf_push_constant_expr_source(elf_State *state, const char *name, elf_StrSlice source, elf_CompileReport *report);
int elf_push_json_source(elf_State *state, const char *name, elf_StrSlice source, elf_CompileReport *report);
