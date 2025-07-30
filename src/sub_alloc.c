//
// See Copyright Notice In elf.h
//

static void dealloc_memory_debug(Allocator fn, const void *memory, Debug_Source loca) {
	elf_Error error = fn(0,0,0,0,(void **)&memory,loca);
	// ASSERT(PASSED(error));
}


static void *alloc_memory_debug(Allocator fn, elf_Int length, Debug_Source loca) {
	void *memory = 0;
	elf_Error error = fn(0,0,0,length,&memory,loca);
	// ASSERT(PASSED(error));
	return memory;
}


static void *realloc_memory_debug(Allocator fn, elf_Int length, void *memory, Debug_Source loca) {
	elf_Error error = fn(0,0,0,length,&memory,loca);
	// ASSERT(PASSED(error));
	return memory;
}


static void *calloc_memory_debug(Allocator fn, elf_Int size, Debug_Source loca) {
	return clear_memory(alloc_memory_debug(fn,size,loca),size);
}


static ALLOCATOR_FN(global_allocator) {
	if (memory==0) {
		return 1;
	}
	if (new_size==0) {
#if defined(_DEBUG_ALLOC)
		stb_leakcheck_free(*memory);
#else
		free(*memory);
#endif
	} else {
		if (*memory==0) {
#if defined(_DEBUG_ALLOC)
			*memory=stb_leakcheck_malloc(new_size,debug.fileName,debug.lineNumber);
#else
			*memory=malloc(new_size);
#endif
		} else {
#if defined(_DEBUG_ALLOC)
			*memory=stb_leakcheck_realloc(*memory,new_size,debug.fileName,debug.lineNumber);
#else
			*memory=realloc(*memory,new_size);
#endif
		}
		if (*memory==0) {
			elf_error_log("fatal error: out of memory");
			return 1;
		}
	}
	return 0;
}


static ALLOCATOR_FN(thread_allocator) {
	if (memory==0) {
		return 1;
	}
	if (new_size==0) {
		return 1;
	} else {
		if (*memory!=0) {
			return 1;
		}

		// TODO:
		THREAD char buffer[0x10000];
		THREAD char *cursor = 0;
		if (cursor == 0) cursor = buffer;

		if (new_size > sizeof(buffer)) {
			return 1;
		}

		if((cursor - buffer) + new_size > sizeof(buffer)) {
			cursor = buffer;
		}

		*memory = cursor;
		cursor += new_size;
	}
	return 0;
}
