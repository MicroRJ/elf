//
// See Copyright Notice In elf.h
//

#ifndef ELF_CORE_BUFFER_H
#define ELF_CORE_BUFFER_H

typedef struct elf_State elf_State;
typedef struct elf_ByteBuffer elf_ByteBuffer;

struct elf_ByteBuffer
{
	elf_Object obj;
	u8        *data;
	u32        size;
	u32        capacity;
};

elf_ByteBuffer *elf_buffer_new_object(elf_State *state, u32 capacity);
b32 elf_buffer_reserve_object(elf_State *state, elf_ByteBuffer *buffer, u32 capacity);
b32 elf_buffer_resize_object(elf_State *state, elf_ByteBuffer *buffer, u32 size);
b32 elf_buffer_append_object(elf_State *state, elf_ByteBuffer *buffer, const void *data, u32 size);
b32 elf_buffer_patch_object(elf_State *state, elf_ByteBuffer *buffer, u32 position,
	u32 delete_size, const void *data, u32 insert_size);

#endif
