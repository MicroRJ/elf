//
// See Copyright Notice In elf.h
//

// todo: we're single threaded, and ... memory per translation unit??
static void *thread_alloc(int amount) {
	THREAD char buffer[2048];
	THREAD int cursor;

	if (amount > sizeof(buffer)) {
		return 0;
	}
	if(cursor + amount > sizeof(buffer)) {
		cursor = 0;
	}
	void *memory = buffer + cursor;
	cursor += amount;
	return memory;
}