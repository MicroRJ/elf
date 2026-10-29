//
// See Copyright Notice In elf.h
//

elf_GCError *elf_error_new(elf_State *state, elf_ErrorCode code, elf_String *message, elf_Table *data)
{
	ASSERT(state);
	ASSERT(message);

	u32 frame_count = 0;
	for (StackFrame *frame = frame_current(state); frame; frame = frame_previous(state, frame))
	{
		if (frame_proto(frame))
		{
			frame_count += 1;
		}
	}

	u32 size = sizeof(elf_GCError) + frame_count * sizeof(elf_ErrorFrame);
	elf_GCError *error = elf_gc_alloc(state, ELF_OBJECT_ERROR, size);
	error->obj.status |= ELF_OBJECT_READONLY;
	error->code = code;
	error->message = message;
	error->data = data;
	error->source = 0;

	for (StackFrame *frame = frame_current(state); frame; frame = frame_previous(state, frame))
	{
		elf_Prototype *proto = frame_proto(frame);
		if (proto)
		{
			if (!error->source)
			{
				error->source = proto->source;
			}
			error->frames[error->frame_count].prototype = proto;
			error->frames[error->frame_count].instruction = frame_active_instr(frame) - proto->bytecode;
			error->frame_count += 1;
		}
	}

	ASSERT(error->frame_count == frame_count);
	return error;
}

elf_GCError *elf_error_newfv(elf_State *state, elf_ErrorCode code, elf_Table *data, const char *format, va_list args)
{
	ASSERT(state);
	ASSERT(format);

	if (data)
	{
		push_value(state, value_from_table(data));
	}

	Scratch scratch = scratch_begin();
	dy_String formatted = arena_pushfv(scratch.arena, format, args);
	elf_String *message = formatted.data ? elf_string_from_data_size(state, formatted.data, (u32)formatted.size) : elf_string_from_data(state, "error");
	scratch_end(scratch);

	push_value(state, str_value(message));
	elf_GCError *error = elf_error_new(state, code, message, data);
	pop_value(state);

	if (data)
	{
		pop_value(state);
	}
	return error;
}

elf_GCError *elf_error_newf(elf_State *state, elf_ErrorCode code, elf_Table *data, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	elf_GCError *error = elf_error_newfv(state, code, data, format, args);
	va_end(args);
	return error;
}
