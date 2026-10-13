//
// See Copyright Notice In elf.h
//

#ifndef ELF_CORE_STRING_H
#define ELF_CORE_STRING_H

#define ELF_STRING_HEADER_SIZE ((u32)offsetof(elf_String, data))
#define ELF_STRING_MAX_SIZE    (0xffffffffu - ELF_STRING_HEADER_SIZE - 1)

static inline u32 string_size(elf_String *string)
{
	ASSERT(string);
	ASSERT(string->obj.size >= ELF_STRING_HEADER_SIZE + 1);
	return string->obj.size - ELF_STRING_HEADER_SIZE - 1;
}

static inline const char *string_data(elf_String *string)
{
	return string->data;
}

static inline u32 string_hash(elf_String *string)
{
	return string->hash;
}

static inline b32 strings_equal(elf_String *left, elf_String *right)
{
	return left == right;
}

static inline elf_String *elf_string_from_data(elf_State *state, const char *data)
{
	ASSERT(data);
	return elf_string_from_data_size(state, data, (u32)strlen(data));
}

#endif
