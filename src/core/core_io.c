//
// See Copyright Notice In elf.h
//

// char *name, FILE *io, int size, int pos
// todo: remove!
int elf_read_file(elf_State *inter, int size) {

	int closeio = false;
	FILE *io = 0;
	if (inter->stack_ptr[-1].tag == elf_tag_Handle) {
		io = (FILE *) inter->stack_ptr[-1].x_sys;
		if (!io) {
			elf_error_log("'%p': handle passed in is invalid", io);
			return elf_push_nil(inter);
		}
	} else if (inter->stack_ptr[-1].tag == elf_tag_String) {
		char *name = inter->stack_ptr[-1].x_str->text;
		io = fopen(name, "rb");
		if (!io) {
			elf_error_log("'%s': could not read", name);
			return elf_push_nil(inter);
		}
		closeio = true;
	}

	// todo: remove this!
	if (size == -1) {
		fseek(io,0,SEEK_END);
		size = ftell(io);
		fseek(io,0,SEEK_SET);
	}

	elf_String *contents = elf_alloc_string2(inter, size);
	fread(contents->text, 1, size, io);
	TOP(inter)->tag = elf_tag_String;
	TOP(inter)->x_str = contents;
	int stk = inctop(inter);
	if (closeio) fclose(io);
	return stk;
}
