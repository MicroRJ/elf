//
// See Copyright Notice In elf.h
//

// todo: we're single threaded, and ... memory per TU??
static void *thread_alloc(int size) {
	THREAD char buffer[4096];
	THREAD int cursor;

	if (size > sizeof(buffer)) {
		return 0;
	}
	if(cursor + size > sizeof(buffer)) {
		cursor = 0;
	}
	void *memory = buffer + cursor;
	cursor += size;
	return memory;
}