//
// See Copyright Notice In elf.h
//


elf_pubapi
bool elf_readfileh(elf_State *inter, elf_Handle file, int size) {
	if (!file) {
		elf_lerror("invalid file handle");
		elf_pushnil(inter);
		return false;
	}

	if (size == -1) {
		size = sys_size_file(file);
	}

	elf_String *contents = elf_alloc_string2(inter, size);
	vsetstr(inter->stack_ptr, contents);
	incstackptr(inter);

	sys_read_file(file, contents->text, size);

	return true;
}

elf_pubapi
bool elf_readfilen(elf_State *inter, const char *name, int size)
{
	elf_Handle file = sys_open_file(name, SYS_OPEN_READ, SYS_OPEN_EXISTING);

	bool noerr = elf_readfileh(inter, file, size);

	sys_close_file(file);

	return noerr;
}

elf_pubapi
bool elf_readfile(elf_State *inter, int stk, int size) {
	bool noerr = false;

	if (tisstr(elf_gettag(inter, stk))) {

		const char *name = elf_tostr(inter, stk);

		elf_Handle file = sys_open_file(name, SYS_OPEN_READ, SYS_OPEN_EXISTING);

		noerr = elf_readfileh(inter, file, size);

		sys_close_file(file);
	}
	else if (tissys(elf_gettag(inter, stk))) {

		elf_Handle file = elf_tosys(inter, stk);

		noerr = elf_readfileh(inter, file, size);
	}
	else {

		elf_errorf(inter, -1
		, "'%s': 'readfile' expected handle or file name", tag2s[elf_gettag(inter, stk)]);

		elf_pushnil(inter);
	}
	return noerr;
}
