#ifndef ELF_CORE_USER_OBJECT_H
#define ELF_CORE_USER_OBJECT_H

typedef struct elf_UserObject elf_UserObject;

struct elf_UserObject
{
	elf_Object obj;
	const elf_UserType *type;
	elf_Table *metatable;
	max_align_t data[];
};

elf_UserObject *elf_user_object_new(elf_State *state, const elf_UserType *type,
	elf_Table *metatable, u32 size);

#endif
