/*
** See Copyright Notice In elf.h
** alloc.c
*/



void *clear_memory(void *target, elf_Int length) {
	memset(target,0,length);
	return target;
}


void *copy_memory(void *dst, void const *src, elf_Int length) {
	memcpy(dst,src,length);
	return dst;
}


void dealloc_memory_debug(Allocator fn, const void *memory, DBGSource loca) {
	elf_Error error = fn(0,0,0,0,(void **)&memory,loca);
	ASSERT(PASSED(error));
}


void *alloc_memory_debug(Allocator fn, elf_Int length, DBGSource loca) {
	void *memory = 0;
	elf_Error error = fn(0,0,0,length,&memory,loca);
	ASSERT(PASSED(error));
	return memory;
}


void *realloc_memory_debug(Allocator fn, elf_Int length, void *memory, DBGSource loca) {
	elf_Error error = fn(0,0,0,length,&memory,loca);
	ASSERT(PASSED(error));
	return memory;
}


void *calloc_memory_debug(Allocator fn, elf_Int size, DBGSource loca) {
	return clear_memory(alloc_memory_debug(fn,size,loca),size);
}


ALLOCATOR_FN(global_allocator) {
	if (memory==0) {
		return Error_InvalidArguments;
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
			elf_debugger("fatal error: out of memory");
			return Error_OutOfMemory;
		}
	}
	return Error_None;
}


ALLOCATOR_FN(thread_allocator) {
	if (memory==0) {
		return Error_InvalidArguments;
	}
	if (new_size==0) {
		return Error_InvalidArguments;
	} else {
		if (*memory!=0) {
			return Error_InvalidArguments;
		}

		// TODO:
		THREAD char buffer[0x10000];
		THREAD char *cursor = 0;
		if (cursor == 0) cursor = buffer;

		if (new_size > sizeof(buffer)) {
			return Error_OutOfMemory;
		}

		if((cursor - buffer) + new_size > sizeof(buffer)) {
			cursor = buffer;
		}

		*memory = cursor;
		cursor += new_size;
	}
	return Error_None;
}
