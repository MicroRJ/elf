//
// See Copyright Notice In elf.h
//

#include <dy.h>

static const char *error_code_names[] =
{
#define ERROR_CODE_NAME(code, name) [code] = name,
	ERROR_CODE_X_LIST(ERROR_CODE_NAME)
#undef ERROR_CODE_NAME
};

static SourceSite find_source_for_instr(elf_Prototype *function, int instr)
{
	SourceSite site = {};
	for (u32 i = 0; i < function->source_map_count; ++i)
	{
		BcSourceMapEntry entry = function->source_map[i];
		if (instr >= entry.byte_start && instr < entry.byte_end)
		{
			site = entry.site;
			break;
		}
	}
	return site;
}

static const char *error_code_name(ErrorCode code)
{
	if ((u32)code < ERROR_CODE_COUNT && error_code_names[code])
	{
		return error_code_names[code];
	}
	return "runtime";
}

static StackFrame *runtime_current_bytecode_frame(elf_State *state)
{
	StackFrame *result = 0;
	for (StackFrame *frame = state->frame;; --frame)
	{
		if (frame->prototype)
		{
			result = frame;
			break;
		}
		if (frame == state->frame_stack)
		{
			break;
		}
	}
	return result;
}

static void print_runtime_source_location(elf_State *state, StackFrame *frame)
{
	ASSERT(frame);
	ASSERT(frame->prototype);
	elf_Prototype *function = frame->prototype;
	u32 instr = frame->instruction;
	ASSERT(instr < function->bytecode_count);
	SourceSite site = find_source_for_instr(function, instr);
	const char *bytecode_name = bytecode_type_name(BC_TYPE(function->bytecode[instr]));

	if (source_slice_is_valid(site))
	{
		log_linef(LOG_LEVEL_ERROR, "<unknown> [%u:%llu] [%u](%s)", site.line_index, source_slice_column(site), instr, bytecode_name);
	}
	else
	{
		log_linef(LOG_LEVEL_ERROR, "<unknown> [?] [%u](%s): source information could not be found", instr, bytecode_name);
	}
}

static void print_runtime_call_stack(elf_State *state, StackFrame *current)
{
	b32 printed = false;
	for (StackFrame *frame = current; frame && frame > state->frame_stack;)
	{
		--frame;
		if (frame->prototype)
		{
			if (!printed)
			{
				log_line(LOG_LEVEL_INFO, "call stack:");
				printed = true;
			}
			print_runtime_source_location(state, frame);
		}
	}
}

static void print_runtime_context(elf_State *state)
{
	StackFrame *current = runtime_current_bytecode_frame(state);
	if (current)
	{
		print_runtime_source_location(state, current);
		print_runtime_call_stack(state, current);
	}
	else
	{
		log_line(LOG_LEVEL_ERROR, "source information could not be found");
	}
}

// TODO(RJ): this is deprecated!
static void abort_after_runtime_error(void)
{
#if defined(_DEBUG) && defined(ELF_DEBUG_BREAK_ON_RUNTIME_ERROR)
	dy_debug_break();
#endif
	log_line(LOG_LEVEL_FATAL, "elf is exiting...");
	dy_exit_process(1);
}

static void report_runtime_error_message(elf_State *state, ErrorCode code, const char *message)
{
	log_linef(LOG_LEVEL_ERROR, "runtime error[%s]: %s", error_code_name(code), message);
	print_runtime_context(state);
	abort_after_runtime_error();
}

void print_current_runtime_source_location(elf_State *state)
{
	print_runtime_context(state);
}

void report_runtime_error(elf_State *state, ErrorCode code, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	Scratch scratch = scratch_begin();
	char *message = arena_pushfv(scratch.arena, format, args);
	va_end(args);
	arena_push_zero(scratch.arena, 1, 1);

	report_runtime_error_message(state, code, message);

	scratch_end(scratch);
}
