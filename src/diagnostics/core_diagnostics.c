//
// See Copyright Notice In elf.h
//

#include "elf_os.h"

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
	elf_Module *module;
	BcFunction *function;
	SourceSite       site;
	int              instr;
	Bytecode          byte;
}
RuntimeSourceLocation;

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

static RuntimeSourceLocation runtime_source_location_for_function(elf_Module *module, BcFunction *function, int instr)
{
	RuntimeSourceLocation location = {};
	location.instr = instr;
	if (!module || !function) {
		return location;
	}

	if (instr >= (int)function->offset && instr < (int)(function->offset + function->length))
	{
		location.module = module;
		location.function = function;
		location.site = find_source_for_instr(function, instr);
		location.byte = module->bytecode[instr];
	}

	return location;
}

static StackFrame *runtime_current_bytecode_frame(elf_State *state)
{
	for (StackFrame *frame = state->frame;; --frame)
	{
		if (frame->module && frame->function) return frame;
		if (frame == state->frame_stack) break;
	}
	return 0;
}

static RuntimeSourceLocation runtime_source_location(elf_State *state, int instr)
{
	StackFrame *frame = runtime_current_bytecode_frame(state);
	if (!frame) return (RuntimeSourceLocation) {};
	if (instr == NO_BYTE) instr = frame->function->offset + frame->instruction;
	return runtime_source_location_for_function(frame->module, frame->function, instr);
}

static void print_runtime_source_location(RuntimeSourceLocation location)
{
	elf_Module *module = location.module;
	BcFunction *function = location.function;
	const char *name = module && module->source_name ? string_data(module->source_name) : 0;
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
	SourceBuffer source = {};
	if (module && module->source_data) {
		source.data = module->source_data;
		source.size = module->source_size;
	}
	print_source_slice_marker(location.site, source);
	log_line(LOG_LEVEL_INFO, "|");
}

static b32 print_runtime_call_stack(elf_State *state)
{
	StackFrame *current = runtime_current_bytecode_frame(state);
	b32 printed = false;
	for (StackFrame *frame = state->frame; frame > state->frame_stack;)
	{
		frame--;
		if (frame == current || !frame->module || !frame->function) continue;
		if (!printed) log_line(LOG_LEVEL_INFO, "call stack:");
		printed = true;
		int instr = frame->function->offset + frame->instruction;
		RuntimeSourceLocation location = runtime_source_location_for_function(frame->module, frame->function, instr);
		if (location.function) {
			print_runtime_source_location(location);
		}
	}
	return printed;
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
	elf_os_debug_break();
#endif
	log_line(LOG_LEVEL_FATAL, "elf is exiting...");
	elf_os_exit_process(1);
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
