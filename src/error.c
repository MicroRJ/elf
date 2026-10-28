//
// See Copyright Notice In elf.h
//

#include <dy.h>
#include <dy_process.h>

static const char *error_code_names[] =
{
#define ELF_ERROR_CODE(name, text) [ELF_ERROR_##name] = text,
#include "elf_error.def"
#undef ELF_ERROR_CODE
};

static elf_SourceMapEntry *find_source_for_instr(elf_Prototype *function, u32 instr)
{
	elf_SourceMapEntry *result = 0;
	for (u32 i = 0; i < function->source_map_count; ++i)
	{
		elf_SourceMapEntry *entry = function->source_map + i;
		if (instr >= entry->byte_start && instr < entry->byte_end)
		{
			result = entry;
			break;
		}
	}
	return result;
}

const char *elf_error_code_name(elf_ErrorCode code)
{
	if ((u32)code < ELF_ERROR_COUNT && error_code_names[code])
	{
		return error_code_names[code];
	}
	return "unknown";
}

static StackFrame *runtime_current_bytecode_frame(elf_State *state)
{
	StackFrame *result = 0;
	for (StackFrame *frame = frame_current(state); frame; frame = frame_previous(state, frame))
	{
		if (frame_proto(frame))
		{
			result = frame;
			break;
		}
	}
	return result;
}

static void print_runtime_source_location(elf_Prototype *proto, u32 instr)
{
	ASSERT(proto);
	ASSERT(instr < proto->bytecode_count);
	elf_SourceMapEntry *entry = find_source_for_instr(proto, instr);
	const char *bytecode_name = bytecode_type_name(BC_TYPE(proto->bytecode[instr]));
	SourceSite site = {};
	if (entry)
	{
		site.offset = entry->source_offset;
		site.size = entry->source_size;
		site.line_offset = entry->line_offset;
		site.line_index = entry->line_index;
	}

	if (proto->source && source_slice_is_valid(site))
	{
		dy_String source = proto->source->text;

		log_linef(LOG_LEVEL_INFO, "%.*s [%u:%llu] [%u](%s)", (i32)proto->source->name.size, proto->source->name.data, site.line_index, source_slice_column(site), instr, bytecode_name);
		print_source_slice_marker(site, source);
	}
	else
	{
		log_linef(LOG_LEVEL_INFO, "<unknown> [?] [%u](%s): source information could not be found", instr, bytecode_name);
	}
}

static void print_runtime_call_stack(elf_State *state, StackFrame *current)
{
	b32 printed = false;
	for (StackFrame *frame = frame_previous(state, current); frame; frame = frame_previous(state, frame))
	{
		if (frame_proto(frame))
		{
			if (!printed)
			{
				log_line(LOG_LEVEL_INFO, "call stack:");
				printed = true;
			}
			elf_Prototype *proto = frame_proto(frame);
			u32 instr = frame_active_instr(frame) - proto->bytecode;
			print_runtime_source_location(proto, instr);
		}
	}
}

static void print_runtime_context(elf_State *state)
{
	StackFrame *current = runtime_current_bytecode_frame(state);
	if (current)
	{
		elf_Prototype *proto = frame_proto(current);
		u32 instr = frame_active_instr(current) - proto->bytecode;
		print_runtime_source_location(proto, instr);
		print_runtime_call_stack(state, current);
	}
	else
	{
		log_line(LOG_LEVEL_INFO, "source information could not be found");
	}
}

static void print_error_diagnostic(elf_GCError *error)
{
	ASSERT(error);
	ASSERT(error->message);

	log_format(LOG_LEVEL_ERROR, "runtime error[%s]", elf_error_code_name(error->code));
	log_linef(LOG_LEVEL_INFO, ": %.*s", (i32)elf_string_size(error->message), error->message->data);

	if (!error->frame_count)
	{
		log_line(LOG_LEVEL_INFO, "source information could not be found");
		return;
	}

	elf_ErrorFrame *frame = error->frames;
	print_runtime_source_location(frame->prototype, frame->instruction);

	if (error->frame_count > 1)
	{
		log_line(LOG_LEVEL_INFO, "call stack:");
		for (u32 index = 1; index < error->frame_count; ++index)
		{
			frame = error->frames + index;
			print_runtime_source_location(frame->prototype, frame->instruction);
		}
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

static void report_runtime_error_message(elf_State *state, elf_ErrorCode code, const char *message)
{
	log_format(LOG_LEVEL_ERROR, "runtime error[%s]", elf_error_code_name(code));
	log_linef(LOG_LEVEL_INFO, ": %s", message);
	print_runtime_context(state);
	abort_after_runtime_error();
}

void print_current_runtime_source_location(elf_State *state)
{
	print_runtime_context(state);
}

void report_runtime_error(elf_State *state, elf_ErrorCode code, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	Scratch scratch = scratch_begin();
	dy_String message = arena_pushfv(scratch.arena, format, args);
	va_end(args);
	arena_push_zero(scratch.arena, 1, 1);

	report_runtime_error_message(state, code, message.data ? message.data : "runtime error");

	scratch_end(scratch);
}
