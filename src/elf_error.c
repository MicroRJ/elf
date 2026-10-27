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

	for (StackFrame *frame = frame_current(state); frame; frame = frame_previous(state, frame))
	{
		elf_Prototype *proto = frame_proto(frame);
		if (proto)
		{
			error->frames[error->frame_count].prototype = proto;
			error->frames[error->frame_count].instruction = frame_active_instr(frame) - proto->bytecode;
			error->frame_count += 1;
		}
	}

	ASSERT(error->frame_count == frame_count);
	return error;
}
