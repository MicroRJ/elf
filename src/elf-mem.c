/*
** See Copyright Notice In elf.h
** lmem.c
** Memory Tools
*/



void *clear_memory(void *target, elf_Int length) {
	memset(target,0,length);
	return target;
}


void *copy_memory(void *dst, void const *src, elf_Int length) {
	memcpy(dst,src,length);
	return dst;
}


void elf_dealloc_(elAllocator fn, const void *memory, DBGSource loca) {
	elf_Error error = fn(0,0,0,0,(void **)&memory,loca);
	ASSERT(PASSED(error));
}


void *elf_alloc_(elAllocator fn, elf_Int length, DBGSource loca) {
	void *memory = 0;
	elf_Error error = fn(0,0,0,length,&memory,loca);
	ASSERT(PASSED(error));
	return memory;
}


void *elf_realloc_(elAllocator fn, elf_Int length, void *memory, DBGSource loca) {
	elf_Error error = fn(0,0,0,length,&memory,loca);
	ASSERT(PASSED(error));
	return memory;
}


void *elf_calloc_(elAllocator fn, elf_Int size, DBGSource loca) {
	return clear_memory(elf_alloc_(fn,size,loca),size);
}


ALLOCATOR_FN(heap_allocfn) {
	if (memory==0) {
		return Error_InvalidArguments;
	}
	if (new_size == 0) {
		free(*memory);
	} else {
		if (*memory == 0) {
			*memory = stb_leakcheck_malloc(new_size,debug.fileName,debug.lineNumber);
		} else {
			*memory = stb_leakcheck_realloc(*memory,new_size,debug.fileName,debug.lineNumber);
		}
		if (*memory == 0) {
			elf_debugger("fatal error: out of memory");
			return Error_OutOfMemory;
		}
	}
	return Error_None;
}


ALLOCATOR_FN(tls_allocfn) {
	if (memory==0) {
		return Error_InvalidArguments;
	}
	if (new_size==0) {
		return Error_InvalidArguments;
	} else {
		if (*memory != 0) {
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
