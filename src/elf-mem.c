/*
** See Copyright Notice In elf.h
** lmem.c
** Memory Tools
*/



void *elf_clear_memory(void *target, elInteger length) {
	memset(target,0,length);
	return target;
}


void *elf_copy_memory(void *dst, void const *src, elInteger length) {
	memcpy(dst,src,length);
	return dst;
}


void elf_dealloc_(elAllocator fn, const void *memory, SourceInfo loca) {
	elError error = fn(0,0,0,(void **)&memory,loca);
	ASSERT(PASSED(error));
}


void *elf_alloc_(elAllocator fn, elInteger length, SourceInfo loca) {
	void *memory = 0;
	elError error = fn(0,0,length,&memory,loca);
	ASSERT(PASSED(error));
	return memory;
}


void *elf_realloc_(elAllocator fn, elInteger length, void *memory, SourceInfo loca) {
	elError error = fn(0,0,length,&memory,loca);
	ASSERT(PASSED(error));
	return memory;
}


void *elf_calloc_(elAllocator fn, elInteger size, SourceInfo loca) {
	return elf_clear_memory(elf_alloc_(fn,size,loca),size);
}


elError heap_allocfn(int flags, elInteger old_size, elInteger new_size, void **io, SourceInfo loca) {
	if (io == 0) {
		return Error_InvalidArguments;
	}
	if (new_size == 0) {
		free(*io);
	} else {
		if (*io == 0) {
			*io = stb_leakcheck_malloc(new_size,loca.fileName,loca.lineNumber);
		} else {
			*io = stb_leakcheck_realloc(*io,new_size,loca.fileName,loca.lineNumber);
		}
		if (*io == 0) {
			elf_debugger("fatal error: out of memory");
			return Error_OutOfMemory;
		}
	}
	return Error_None;
}


elError tls_allocfn(int flags, elInteger old_size, elInteger new_size, void **io, SourceInfo loca) {
	if (io == 0) {
		return Error_InvalidArguments;
	}
	if (new_size == 0) {
		return Error_InvalidArguments;
	} else {
		/* reallocation is not permitted */
		if (*io != 0) {
			return Error_InvalidArguments;
		}

		// TODO:
		elTHREAD char memory[0x10000];
		elTHREAD char *cursor = 0;
		if (cursor == 0) cursor = memory;

		if (new_size > sizeof(memory)) {
			return Error_OutOfMemory;
		}

		if((cursor - memory) + new_size > sizeof(memory)) {
			cursor = memory;
		}

		*io = cursor;
		cursor += new_size;
	}
	return Error_None;
}
