//
// See Copyright Notice In elf.h
//

typedef enum
{
	RUNTIME_ERROR_NONE = 0,
	RUNTIME_ERROR_GENERIC,
	RUNTIME_ERROR_EXPECTS_CALLABLE,
	RUNTIME_ERROR_INVALID_ARGUMENT_COUNT,
	RUNTIME_ERROR_UNKNOWN_BYTECODE
}
RuntimeErrorType;

#define report_runtime_error(state, error, format, ...) report_runtime_error_(state, error, temporay_format(format, __VA_ARGS__))
static void report_runtime_error_(elf_State *state, RuntimeErrorType error, const char *message)
{
}
