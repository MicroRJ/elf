//
// See Copyright Notice In elf.h
//

// char *name, FILE *io, int size, int pos
// todo: remove!
int elf_read_file(elf_State *inter, int size) {

	int closeio = false;

	elf_Handle file = 0;

	if (inter->stack_ptr[-1].tag == elf_tag_Handle) {
		file = inter->stack_ptr[-1].x_sys;
		// todo: define a proper invalid handle value
		if (!file) {
			elf_error_log("'%p': handle passed in is invalid", file);
			return elf_push_nil(inter);
		}
	} else if (inter->stack_ptr[-1].tag == elf_tag_String) {
		char *name = inter->stack_ptr[-1].x_str->text;
		file = sys_open_file(name, SYS_OPEN_READ, SYS_OPEN_EXISTING);
		// todo: define a proper invalid handle value
		if (!file) {
			elf_error_log("'%s': could not read", name);
			return elf_push_nil(inter);
		}
		closeio = true;
	}

	if (size == -1) {
		size = sys_size_file(file);
	}

	elf_String *contents = elf_alloc_string2(inter, size);
	sys_read_file(file, contents->text, 0, size);

	TOP(inter)->tag = elf_tag_String;
	TOP(inter)->x_str = contents;

	int stk = inctop(inter);
	if (closeio) sys_close_file(file);
	return stk;
}
