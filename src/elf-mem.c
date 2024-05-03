/*
** See Copyright Notice In elf.h
** lmem.c
** Memory Tools
*/


/* todo: can we do this some other way? */
elf_globaldecl Alloc langM_tlocalalloc = {"default-temp-allocator",elf_deftlsallocfn};
elf_globaldecl Alloc langM_globalalloc = {"default-heap-allocator",elf_defglobalallocfn};


/* todo: should prob be using something like stb leak */
void *elf_memclear(void *target, elf_int length) {
	memset(target,0,length);
	return target;
}


void *elf_memcopy(void *target, void const *source, elf_int length) {
	memcpy(target,source,length);
	return target;
}


void elf_dealloc_(Alloc *c, const void *memory, ldebugloc loca) {
	Error error = c->fn(c,0,0,0,(void **)&memory,loca);
	elf_ensure(LPASSED(error));
}


void *elf_alloc_(Alloc *c, elf_int length, ldebugloc loca) {
	void *memory = 0;
	Error error = c->fn(c,0,0,length,&memory,loca);
	elf_ensure(LPASSED(error));
	return memory;
}


void *elf_realloc_(Alloc *c, elf_int length, void *memory, ldebugloc loca) {
	Error error = c->fn(c,0,0,length,&memory,loca);
	elf_ensure(LPASSED(error));
	return memory;
}


void *elf_clearalloc_(Alloc *c, elf_int size, ldebugloc loca) {
	return elf_memclear(elf_alloc_(c,size,loca),size);
}


ALLOCFN(elf_defglobalallocfn) {
	if (oldAndNewMemory == 0) {
		return Error_InvalidArguments;
	}
	if (newSize == 0) {
#if defined(_MSC_VER)
		_aligned_free(*oldAndNewMemory);
#else
		free(*oldAndNewMemory);
#endif
	} else {
		if (*oldAndNewMemory == 0) {
#if defined(_MSC_VER)
			*oldAndNewMemory = _aligned_malloc(newSize,0x10);
#else
			*oldAndNewMemory = malloc(newSize);
#endif
		} else {
#if defined(_MSC_VER)
			*oldAndNewMemory = _aligned_realloc(*oldAndNewMemory,newSize,0x10);
#else
			*oldAndNewMemory = realloc(*oldAndNewMemory,newSize);
#endif
		}
		if (*oldAndNewMemory == 0) {
			elf_debugger("fatal error: out of memory");
			return Error_OutOfMemory;
		}
	}
	return Error_None;
}


ALLOCFN(elf_deftlsallocfn) {
	if (oldAndNewMemory == 0) {
		return Error_InvalidArguments;
	}
	if (newSize == 0) {
		return Error_InvalidArguments;
	} else {
		/* reallocation is not permitted */
		if (*oldAndNewMemory != 0) {
			return Error_InvalidArguments;
		}

		// TODO:
		elf_threaddecl char memory[0x10000];
		elf_threaddecl char *cursor = 0;
		if (cursor == 0) cursor = memory;

		if (newSize > sizeof(memory)) {
			return Error_OutOfMemory;
		}

		if((cursor - memory) + newSize > sizeof(memory)) {
			cursor = memory;
		}

		*oldAndNewMemory = cursor;
		cursor += newSize;
	}
	return Error_None;
}
