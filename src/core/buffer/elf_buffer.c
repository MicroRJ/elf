//
// See Copyright Notice In elf.h
//

static b32 buffer_require_writable(elf_State *state, elf_ByteBuffer *buffer)
{
	if (buffer->obj.status & ELF_OBJECT_READONLY)
	{
		elf_report_runtime_error(state, RUNTIME_ERROR_GENERIC, NO_BYTE,
			"attempted to write to readonly buffer");
		return false;
	}
	return true;
}

elf_ByteBuffer *elf_buffer_new_object(elf_State *state, u32 capacity)
{
	elf_ByteBuffer *buffer = elf_gc_alloc(state, ELF_OBJECT_BUFFER, sizeof(*buffer));
	if (capacity)
	{
		buffer->data = elf_alloc(state, capacity);
		if (!buffer->data)
		{
			elf_report_runtime_error(state, RUNTIME_ERROR_GENERIC, NO_BYTE,
				"could not allocate %u bytes for buffer", capacity);
		}
		buffer->capacity = capacity;
	}
	return buffer;
}

b32 elf_buffer_reserve_object(elf_State *state, elf_ByteBuffer *buffer, u32 capacity)
{
	ASSERT(buffer);
	if (!buffer_require_writable(state, buffer)) return false;
	if (capacity <= buffer->capacity) return true;

	u32 grown = buffer->capacity ? buffer->capacity : 16;
	while (grown < capacity)
	{
		if (grown > UINT32_MAX / 2)
		{
			grown = capacity;
			break;
		}
		grown *= 2;
	}

	u8 *data = elf_realloc(state, buffer->data, buffer->capacity, grown);
	if (!data)
	{
		elf_report_runtime_error(state, RUNTIME_ERROR_GENERIC, NO_BYTE,
			"could not grow buffer to %u bytes", grown);
		return false;
	}
	buffer->data = data;
	buffer->capacity = grown;
	return true;
}

b32 elf_buffer_resize_object(elf_State *state, elf_ByteBuffer *buffer, u32 size)
{
	ASSERT(buffer);
	if (!buffer_require_writable(state, buffer)) return false;
	u32 previous_size = buffer->size;
	if (!elf_buffer_reserve_object(state, buffer, size)) return false;
	if (size > previous_size) zero_memory(buffer->data + previous_size, size - previous_size);
	buffer->size = size;
	return true;
}

b32 elf_buffer_append_object(elf_State *state, elf_ByteBuffer *buffer, const void *data, u32 size)
{
	ASSERT(buffer);
	ASSERT(data || !size);
	if (!buffer_require_writable(state, buffer)) return false;
	if (size > UINT32_MAX - buffer->size)
	{
		elf_report_runtime_error(state, RUNTIME_ERROR_GENERIC, NO_BYTE,
			"buffer size exceeds the supported range");
		return false;
	}

	u32 source_offset = 0;
	b32 source_is_buffer = false;
	if (size && buffer->data)
	{
		uintptr_t source_address = (uintptr_t)data;
		uintptr_t buffer_address = (uintptr_t)buffer->data;
		if (source_address >= buffer_address &&
			source_address - buffer_address < buffer->capacity)
		{
			source_offset = (u32)(source_address - buffer_address);
			source_is_buffer = true;
			if (source_offset > buffer->size || size > buffer->size - source_offset)
			{
				elf_report_runtime_error(state, RUNTIME_ERROR_GENERIC, NO_BYTE,
					"buffer append source is out of bounds");
				return false;
			}
		}
	}

	u32 destination = buffer->size;
	if (!elf_buffer_reserve_object(state, buffer, destination + size)) return false;
	if (source_is_buffer) data = buffer->data + source_offset;
	if (size) memmove(buffer->data + destination, data, size);
	buffer->size += size;
	return true;
}

b32 elf_buffer_patch_object(elf_State *state, elf_ByteBuffer *buffer, u32 position,
	u32 delete_size, const void *data, u32 insert_size)
{
	ASSERT(buffer);
	ASSERT(data || !insert_size);
	if (!buffer_require_writable(state, buffer)) return false;
	if (position > buffer->size || delete_size > buffer->size - position)
	{
		elf_report_runtime_error(state, RUNTIME_ERROR_GENERIC, NO_BYTE,
			"buffer patch range is out of bounds");
		return false;
	}
	u32 size_after_delete = buffer->size - delete_size;
	if (insert_size > UINT32_MAX - size_after_delete)
	{
		elf_report_runtime_error(state, RUNTIME_ERROR_GENERIC, NO_BYTE,
			"buffer size exceeds the supported range");
		return false;
	}

	const void *insert_data = data;
	u8 *insert_copy = 0;
	if (insert_size && buffer->data)
	{
		uintptr_t source_address = (uintptr_t)data;
		uintptr_t buffer_address = (uintptr_t)buffer->data;
		if (source_address >= buffer_address &&
			source_address - buffer_address < buffer->capacity)
		{
			u32 source_offset = (u32)(source_address - buffer_address);
			if (source_offset > buffer->size || insert_size > buffer->size - source_offset)
			{
				elf_report_runtime_error(state, RUNTIME_ERROR_GENERIC, NO_BYTE,
					"buffer patch source is out of bounds");
				return false;
			}
			insert_copy = elf_alloc(state, insert_size);
			if (!insert_copy)
			{
				elf_report_runtime_error(state, RUNTIME_ERROR_GENERIC, NO_BYTE,
					"could not allocate %u bytes for buffer patch", insert_size);
				return false;
			}
			copy_memory(insert_copy, data, insert_size);
			insert_data = insert_copy;
		}
	}

	u32 new_size = size_after_delete + insert_size;
	if (!elf_buffer_reserve_object(state, buffer, new_size))
	{
		elf_free(state, insert_copy, insert_copy ? insert_size : 0);
		return false;
	}
	u32 tail = buffer->size - position - delete_size;
	if (tail) memmove(buffer->data + position + insert_size,
		buffer->data + position + delete_size, tail);
	if (insert_size) copy_memory(buffer->data + position, insert_data, insert_size);
	buffer->size = new_size;
	elf_free(state, insert_copy, insert_copy ? insert_size : 0);
	return true;
}
