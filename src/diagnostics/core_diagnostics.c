//
// See Copyright Notice In elf.h
//

static const char *runtime_error_type_names[] =
{
	[RUNTIME_ERROR_NONE]                   = "none",
	[RUNTIME_ERROR_GENERIC]                = "runtime",
	[RUNTIME_ERROR_EXPECTS_CALLABLE]       = "call",
	[RUNTIME_ERROR_INVALID_ARGUMENT_COUNT] = "arguments",
	[RUNTIME_ERROR_UNKNOWN_BYTECODE]       = "bytecode",
};

typedef struct
{
	BcFunction *function;
	SourceSite       site;
	int              instr;
	Bytecode          byte;
}
RuntimeSourceLocation;

static BcFunction *find_function_for_instr(elf_State *S, int byte)
{
	for (u32 i = 0; i < S->bytecode_function_count; ++ i) {
		BcFunction *function = &S->bytecode_functions[i];
		if (byte >= function->offset && byte < function->offset + function->length) {
			return function;
		}
	}
	return 0;
}

static SourceSite find_source_for_instr(BcFunction *function, int instr)
{
	for (u32 i = 0; i < function->source_map_count; ++ i) {
		SourceMapEntry entry = function->source_map[i];
		if (instr >= entry.byte_start && instr < entry.byte_end) {
			return entry.site;
		}
	}
	return (SourceSite){};
}

static const char *runtime_error_type_name(RuntimeErrorType error)
{
	if ((u32)error < ARRAY_COUNT(runtime_error_type_names) && runtime_error_type_names[error]) {
		return runtime_error_type_names[error];
	}
	return "runtime";
}

static RuntimeSourceLocation runtime_source_location(elf_State *state, int instr)
{
	if (instr == NO_BYTE) {
		instr = state->byte;
	}

	RuntimeSourceLocation location = {};
	location.instr = instr;
	location.function = find_function_for_instr(state, instr);

	if (location.function)
	{
		location.site = find_source_for_instr(location.function, instr);
		location.byte = state->bytecode[instr];
	}

	return location;
}

static void print_runtime_source_location(RuntimeSourceLocation location)
{
	BcFunction *function = location.function;
	const char *name = function && function->source_name ? atom_data(function->source_name) : 0;
	if (!name) {
		name = "<unknown>";
	}

	if (!source_slice_is_valid(location.site)) {
		log_linef(LOG_LEVEL_ERROR, "%s [?] [%i](%s): source information could not be found"
		, name
		, location.instr
		, bytecode_type_name(BC_TYPE(location.byte)));
		return;
	}

	log_linef(LOG_LEVEL_ERROR, "%s [%u:%llu] [%i](%s):"
	, name
	, location.site.line_index
	, source_slice_column(location.site)
	, location.instr
	, bytecode_type_name(BC_TYPE(location.byte)));

	log_line(LOG_LEVEL_INFO, "|");
	elf_StrSlice source = {};
	if (function && function->source_data) {
		source.data = function->source_data;
		source.size = function->source_size;
	}
	print_source_slice_marker(location.site, source);
	log_line(LOG_LEVEL_INFO, "|");
}

static b32 print_runtime_call_stack(elf_State *state)
{
	if (state->frame_index <= 1) {
		return false;
	}

	log_line(LOG_LEVEL_INFO, "call stack:");
	for (u64 i = 1; i < state->frame_index; ++i)
	{
		StackFrame *frame = &state->frame_stack[i];
		int instr = frame->bytes + frame->nextinstr;
		RuntimeSourceLocation location = runtime_source_location(state, instr);
		if (location.function) {
			print_runtime_source_location(location);
		}
	}
	return true;
}

static void print_runtime_error_location(RuntimeSourceLocation location)
{
	if (location.function) {
		print_runtime_source_location(location);
	} else {
		log_line(LOG_LEVEL_ERROR, "source information could not be found");
	}
}

static void abort_after_runtime_error(void)
{
#if defined(_DEBUG) && defined(ELF_DEBUG_BREAK_ON_RUNTIME_ERROR)
	elf_platform_debug_break();
#endif
	log_line(LOG_LEVEL_FATAL, "elf is exiting...");
	elf_platform_exit_process(1);
}

static void report_runtime_error_message(elf_State *state, RuntimeErrorType error, int instr, const char *message)
{
	RuntimeSourceLocation location = runtime_source_location(state, instr);

	log_linef(LOG_LEVEL_ERROR, "runtime error[%s]: %s", runtime_error_type_name(error), message);
	print_runtime_error_location(location);
	print_runtime_call_stack(state);
	abort_after_runtime_error();
}

void elf_print_current_runtime_source_location(elf_State *state)
{
	RuntimeSourceLocation location = runtime_source_location(state, NO_BYTE);
	print_runtime_error_location(location);
	print_runtime_call_stack(state);
}

void elf_report_runtime_error(elf_State *state, RuntimeErrorType error, int instr, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	elf_Scratch scratch = elf_begin_scratch();
	char *message = elf_arena_pushfv(scratch.arena, format, args);
	va_end(args);
	elf_arena_push_zero(scratch.arena, 1);

	report_runtime_error_message(state, error, instr, message);

	elf_end_scratch(scratch);
}
