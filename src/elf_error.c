//
// See Copyright Notice In elf.h
//

elf_GCError *elf_error_new(elf_State *state, elf_String *message, elf_Table *data)
{
	ASSERT(state);
	ASSERT(message);

	u32 frame_count = 0;
	for (StackFrame *frame = state->frame;; --frame)
	{
		if (frame->prototype)
		{
			frame_count += 1;
		}
		if (frame == state->frame_stack)
		{
			break;
		}
	}

	u32 size = sizeof(elf_GCError) + frame_count * sizeof(elf_ErrorFrame);
	elf_GCError *error = elf_gc_alloc(state, ELF_OBJECT_ERROR, size);
	error->obj.status |= ELF_OBJECT_READONLY;
	error->message = message;
	error->data = data;

	for (StackFrame *frame = state->frame;; --frame)
	{
		if (frame->prototype)
		{
			error->frames[error->frame_count].prototype = frame->prototype;
			error->frames[error->frame_count].instruction = frame->instruction;
			error->frame_count += 1;
		}
		if (frame == state->frame_stack)
		{
			break;
		}
	}

	ASSERT(error->frame_count == frame_count);
	return error;
}
