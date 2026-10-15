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

typedef enum
{
	COM_STATIC_UNRESOLVED,
	COM_STATIC_RESOLVING,
	COM_STATIC_RESOLVED,
}
Com_StaticState;

typedef enum
{
	COM_STATIC_INVALID,
	COM_STATIC_NIL,
	COM_STATIC_INTEGER,
	COM_STATIC_NUMBER,
	COM_STATIC_STRING,
	COM_STATIC_FUNCTION,
	COM_STATIC_NAMESPACE,
}
Com_StaticValueKind;

// One value produced while resolving a compile-time declaration. Functions
// use their stable compiler function index; namespaces use a source index.
typedef struct
{
	Com_StaticValueKind kind;
	union
	{
		i64 integer;
		f64 number;
		struct Atom *string;
		u32 function;
		Com_SourceId source;
	};
}
Com_StaticValue;

// One top-level declaration exported by a compiler source.
typedef struct
{
	struct Atom *name;
	SourceSite site;
	struct Ast_T *expr;
	Com_SourceId source;
	Com_StaticState state;
	Com_StaticValue value;
	b32 referenced;
}
Com_StaticSymbol;

// The exported static namespace belonging to one compiler source.
typedef struct
{
	Com_StaticSymbol *symbols;
	u32 symbol_count;
}
Com_StaticScope;

// One declared parameter in a statically resolved function signature. Default
// expressions are reduced to compiler values in the declaration's source.
typedef struct
{
	struct Atom *name;
	SourceSite site;
	b32 has_default;
	Com_StaticValue default_value;
}
Com_FunctionParameter;

// One function planned by the compiler before IR generation. Entry functions
// point at an AST file; static functions also own their resolved signature.
typedef struct
{
	struct Ast_T *ast;
	Com_FunctionParameter *params;
	Com_SourceId source;
	u32 index;
	u32 param_count;
	b32 is_entry;
	b32 is_static;
}
Com_Function;

typedef enum
{
	COM_STATIC_LOOKUP_NONE,
	COM_STATIC_LOOKUP_VALUE,
	COM_STATIC_LOOKUP_ERROR,
}
Com_StaticLookup;

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
	Com_StaticScope   *static_scopes;
	u32                static_scope_count;
	Com_Function      *functions;
	u32                function_count;
	u32                function_capacity;
	Com_Report         report;
	b32                log_messages;
};

Compiler *compiler_create(elf_Arena *arena);
Com_SourceId compiler_add_root_source(Compiler *compiler, const char *name, elf_StrSlice source);
Com_SourceId compiler_add_root_file(Compiler *compiler, const char *path);
void compiler_set_log_messages(Compiler *compiler, b32 enabled);
b32 compiler_record_import(Compiler *compiler, Com_SourceId source, struct Ast_T *import);
b32 compiler_parse_sources(Compiler *compiler);
b32 compiler_resolve_statics(Compiler *compiler, Com_UnitType unit_type);
Com_StaticSymbol *compiler_static_symbol_from_source(Compiler *compiler, Com_SourceId source, struct Atom *name);
Com_StaticLookup compiler_lookup_static_expr(Compiler *compiler, Com_SourceId source, struct Ast_T *expr, Com_StaticValue *out);
void compiler_report_unreferenced_statics(Compiler *compiler, Com_SourceId source);

b32 compiler_report(Compiler *compiler, Com_Severity severity, Com_Phase phase, SourceSite site, const char *format, ...);
b32 compiler_reportv(Compiler *compiler, Com_Severity severity, Com_Phase phase, SourceSite site, const char *format, va_list args);

Com_Module *compiler_compile(Compiler *compiler, elf_Arena *output_arena, Com_UnitType unit_type);
struct Ast_T *compiler_parse_constant_expr(Compiler *compiler);
struct Ast_T *compiler_parse_json(Compiler *compiler);
