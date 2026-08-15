//
// See Copyright Notice In elf.h
//

#ifndef ELF_CORE_STRING_H
#define ELF_CORE_STRING_H

typedef struct elf_State elf_State;
typedef struct elf_String elf_String;

#define ELF_STRING_MAX_SIZE 0xffffu

struct elf_String
{
	elf_Object  obj;
	elf_String *next;
	u32         hash;
	u16         id;
	u16         size;
	char        data[1];
};

elf_String *elf_string_from_data_size_id(elf_State *state, const char *data, u32 size, u16 id);

static inline u32 string_size(elf_String *string)
{
	// TODO(RJ) no need to store additional size!
	ASSERT(string->obj.size - sizeof(* string) - 1 == string->size);
	return string->size;
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

static elf_String *elf_string_from_data_size(elf_State *state, const char *data, u32 size)
{
	return elf_string_from_data_size_id(state, data, size, 0);
}

static elf_String *elf_string_from_data(elf_State *state, const char *data)
{
	ASSERT(data);
	return elf_string_from_data_size(state, data, (u32)strlen(data));
}

static elf_String *elf_string_from_data_id(elf_State *state, const char *data, u16 id)
{
	ASSERT(data);
	return elf_string_from_data_size_id(state, data, (u32)strlen(data), id);
}

#endif
