elf_UserObject *elf_user_object_new(elf_State *state, const elf_UserType *type,
	elf_Table *metatable, u32 size)
{
	ASSERT(state);
	ASSERT(type);
	ASSERT(metatable);
	ASSERT(size <= UINT32_MAX - offsetof(elf_UserObject, data));

	u32 allocation_size = (u32)offsetof(elf_UserObject, data) + size;
	elf_UserObject *user = elf_gc_alloc(state, ELF_OBJECT_USER, allocation_size);
	user->type = type;
	user->metatable = metatable;
	return user;
}
