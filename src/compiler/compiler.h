//
// See Copyright Notice In elf.h
//

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

#define COM_SOURCE_INVALID ((Com_SourceId)-1)

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
	SourceMapEntry *source_map;
	u32             source_map_count;

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

// One top-level compile-time declaration owned by a compiler source.
typedef struct
{
	struct Atom    *name;
	SourceSite      site;
	struct Ast_T   *expr;
	Com_SourceId    source;
	Com_StaticState state;
	Com_StaticValue value;
	b32 referenced;
	b32 exported;
}
Com_StaticSymbol;

// All compile-time declarations belonging to one compiler source.
typedef struct
{
	Com_StaticSymbol *symbols;
	u32 symbol_count;
}
Com_StaticScope;

typedef struct
{
	struct Atom *path;
	SourceSite site;
	Com_SourceId resolved_source;
}
Com_Import;

// One source file taking part in a compilation. The compiler owns its copied
// text, parsed AST, imports, and compile-time declarations.
typedef struct
{
	struct Atom *name;
	struct Atom *canonical_path;
	SourceBuffer source;
	struct Ast_T *ast;
	Com_Import *imports;
	u32 import_count;
	u32 import_capacity;
	Com_StaticScope statics;
}
Com_Source;

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

// One function planned by the compiler before IR generation. param_count is
// the complete VM arity including implicit parameters; params describes only
// the parameters declared by a static function.
typedef struct
{
	struct Ast_T *ast;
	Com_FunctionParameter *params;
	Com_SourceId source;
	u32 index;
	u32 param_count;
	b32 variadic;
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
	Com_Report         report;
	b32                log_messages;

	// TODO(RJ): remove this shit!
	Com_Function      *functions;
	u32                function_count;
	u32                function_capacity;
};

Compiler *compiler_create(elf_Arena *arena);
Com_SourceId compiler_add_root_source(Compiler *compiler, const char *name, elf_StrSlice source);
Com_SourceId compiler_add_root_file(Compiler *compiler, const char *path);
void compiler_set_log_messages(Compiler *compiler, b32 enabled);
b32 compiler_parse_sources(Compiler *compiler);
b32 compiler_resolve_statics(Compiler *compiler, Com_UnitType unit_type);
Com_StaticSymbol *compiler_static_symbol_from_source(Compiler *compiler, Com_SourceId source, struct Atom *name);
Com_StaticLookup compiler_lookup_static_expr(Compiler *compiler, Com_SourceId source, struct Ast_T *expr, Com_StaticValue *out);
void compiler_report_unreferenced_statics(Compiler *compiler, Com_SourceId source);

b32 compiler_report(Compiler *compiler, Com_Error error, Com_Severity severity, Com_Phase phase, SourceSite site, const char *format, ...);
b32 compiler_reportv(Compiler *compiler, Com_Error error, Com_Severity severity, Com_Phase phase, SourceSite site, const char *format, va_list args);

Com_Module *compiler_compile(Compiler *compiler, elf_Arena *output_arena, Com_UnitType unit_type);
struct Ast_T *compiler_parse_constant_expr(Compiler *compiler);
struct Ast_T *compiler_parse_json(Compiler *compiler);
struct Ast_T *compiler_parse_jsonl(Compiler *compiler);
