//
// See Copyright Notice In elf.h
//

#ifndef ELF_ERROR_H
#define ELF_ERROR_H

#define ERROR_CODE_X_LIST(X) \
	X(ERROR_NONE,                         "none") \
	X(ERROR_INVALID_SOURCE,               "invalid-source") \
	X(ERROR_SOURCE_PATH,                  "source-path") \
	X(ERROR_IMPORT,                       "import") \
	X(ERROR_PARSE_FAILED,                 "parse-failed") \
	X(ERROR_INVALID_UNICODE_ESCAPE,       "invalid-unicode-escape") \
	X(ERROR_UNTERMINATED_ESCAPE_SEQUENCE, "unterminated-escape-sequence") \
	X(ERROR_INVALID_ESCAPE_SEQUENCE,      "invalid-escape-sequence") \
	X(ERROR_INVALID_NUMBER_LITERAL,       "invalid-number-literal") \
	X(ERROR_INTEGER_LITERAL_OVERFLOW,     "integer-literal-overflow") \
	X(ERROR_UNTERMINATED_STRING,          "unterminated-string") \
	X(ERROR_NEWLINE_IN_STRING,            "newline-in-string") \
	X(ERROR_UNTERMINATED_COMMENT,         "unterminated-comment") \
	X(ERROR_UNTERMINATED_INTERPOLATION,   "unterminated-interpolation") \
	X(ERROR_INVALID_CHARACTER_CONSTANT,   "invalid-character-constant") \
	X(ERROR_EXPECTED_MACRO_NAME,          "expected-macro-name") \
	X(ERROR_UNRECOGNIZED_MACRO,           "unrecognized-macro") \
	X(ERROR_NUL_BYTE_IN_SOURCE,           "nul-byte-in-source") \
	X(ERROR_UNEXPECTED_CHARACTER,         "unexpected-character") \
	X(ERROR_UNEXPECTED_TOKEN,             "unexpected-token") \
	X(ERROR_INVALID_EXPRESSION,           "invalid-expression") \
	X(ERROR_EXPECTED_TOKEN,               "expected-token") \
	X(ERROR_INVALID_DECLARATION,          "invalid-declaration") \
	X(ERROR_INVALID_ASSIGNMENT,           "invalid-assignment") \
	X(ERROR_DUPLICATE_DECLARATION,        "duplicate-declaration") \
	X(ERROR_INVALID_FIELD_INITIALIZER,    "invalid-field-initializer") \
	X(ERROR_EXCESS_COMMA,                 "excess-comma") \
	X(ERROR_MISSING_FUNCTION_BODY,        "missing-function-body") \
	X(ERROR_INVALID_STATEMENT,            "invalid-statement") \
	X(ERROR_STATIC_LOOKUP,                "static-lookup") \
	X(ERROR_INVALID_STATIC_EXPRESSION,    "invalid-static-expression") \
	X(ERROR_CYCLIC_STATIC_DEFINITION,     "cyclic-static-definition") \
	X(ERROR_INVALID_FUNCTION_PARAMETER,   "invalid-function-parameter") \
	X(ERROR_INVALID_STATIC_DECLARATION,   "invalid-static-declaration") \
	X(ERROR_DUPLICATE_STATIC_DECLARATION, "duplicate-static-declaration") \
	X(ERROR_EXECUTABLE_MODULE_STATEMENT,  "executable-module-statement") \
	X(ERROR_LOWERING,                     "lowering") \
	X(ERROR_INTERNAL,                     "internal") \
	X(ERROR_UNREFERENCED_ENTITY,          "unreferenced-entity") \
	X(ERROR_UNDECLARED_IDENTIFIER,        "undeclared-identifier") \
	X(ERROR_RUNTIME,                      "runtime") \
	X(ERROR_EXPECTED_CALLABLE,            "expected-callable") \
	X(ERROR_INVALID_ARGUMENT_COUNT,       "invalid-argument-count") \
	X(ERROR_INVALID_ARGUMENT,             "invalid-argument") \
	X(ERROR_INVALID_RETURN_COUNT,         "invalid-return-count") \
	X(ERROR_TYPE_MISMATCH,                "type-mismatch") \
	X(ERROR_OUT_OF_RANGE,                 "out-of-range") \
	X(ERROR_READONLY,                     "readonly") \
	X(ERROR_OUT_OF_MEMORY,                "out-of-memory") \
	X(ERROR_INVALID_TABLE_KEY,            "invalid-table-key") \
	X(ERROR_MISSING_METATABLE,            "missing-metatable") \
	X(ERROR_DIVISION_BY_ZERO,             "division-by-zero") \
	X(ERROR_NOT_IMPLEMENTED,              "not-implemented") \
	X(ERROR_UNKNOWN_BYTECODE,             "unknown-bytecode")

typedef enum
{
#define ERROR_CODE_ENUM(name, text) name,
	ERROR_CODE_X_LIST(ERROR_CODE_ENUM)
#undef ERROR_CODE_ENUM
	ERROR_CODE_COUNT,
}
ErrorCode;

#define ERROR_SEVERITY_X_LIST(X) \
	X(ERROR_NOTE,    "note") \
	X(ERROR_WARNING, "warning") \
	X(ERROR_ERROR,   "error") \
	X(ERROR_FATAL,   "fatal")

typedef enum
{
#define ERROR_SEVERITY_ENUM(name, text) name,
	ERROR_SEVERITY_X_LIST(ERROR_SEVERITY_ENUM)
#undef ERROR_SEVERITY_ENUM
	ERROR_SEVERITY_COUNT,
}
ErrorSeverity;

#define ERROR_PHASE_X_LIST(X) \
	X(ERROR_PHASE_SOURCE,   "source") \
	X(ERROR_PHASE_LEXER,    "lexer") \
	X(ERROR_PHASE_PARSER,   "parser") \
	X(ERROR_PHASE_CHECKER,  "checker") \
	X(ERROR_PHASE_LOWERING, "lowering") \
	X(ERROR_PHASE_BYTECODE, "bytecode") \
	X(ERROR_PHASE_RUNTIME,  "runtime")

typedef enum
{
#define ERROR_PHASE_ENUM(name, text) name,
	ERROR_PHASE_X_LIST(ERROR_PHASE_ENUM)
#undef ERROR_PHASE_ENUM
	ERROR_PHASE_COUNT,
}
ErrorPhase;

#endif
