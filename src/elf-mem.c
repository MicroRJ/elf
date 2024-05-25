/*
** See Copyright Notice In elf.h
** lmem.c
** Memory Tools
*/


/* todo: can we do this some other way? */
elf_globaldecl Alloc elf_tlsalloc = {"default-temp-allocator",elf_deftlsallocfn};
elf_globaldecl Alloc langM_globalalloc = {"default-heap-allocator",elf_defglobalallocfn};


/* todo: should prob be using something like stb leak */
void *elf_memclear(void *target, elInteger length) {
	memset(target,0,length);
	return target;
}


void *elf_memcopy(void *target, void const *source, elInteger length) {
	memcpy(target,source,length);
	return target;
}


void elf_dealloc_(Alloc *c, const void *memory, ldebugloc loca) {
	elError error = c->fn(c,0,0,0,(void **)&memory,loca);
	elf_ensure(LPASSED(error));
}


void *elf_alloc_(Alloc *c, elInteger length, ldebugloc loca) {
	void *memory = 0;
	elError error = c->fn(c,0,0,length,&memory,loca);
	elf_ensure(LPASSED(error));
	return memory;
}


void *elf_realloc_(Alloc *c, elInteger length, void *memory, ldebugloc loca) {
	elError error = c->fn(c,0,0,length,&memory,loca);
	elf_ensure(LPASSED(error));
	return memory;
}


void *elf_clearalloc_(Alloc *c, elInteger size, ldebugloc loca) {
	return elf_memclear(elf_alloc_(c,size,loca),size);
}


elError elf_defglobalallocfn(Alloc *allocator, int flags, elInteger oldSize, elInteger newSize, void **io, ldebugloc loca) {
	if (io == 0) {
		return Error_InvalidArguments;
	}
	if (newSize == 0) {
		free(*io);
	} else {
		if (*io == 0) {
			*io = stb_leakcheck_malloc(newSize,loca.fileName,loca.lineNumber);
		} else {
			*io = stb_leakcheck_realloc(*io,newSize,loca.fileName,loca.lineNumber);
		}
		if (*io == 0) {
			elf_debugger("fatal error: out of memory");
			return Error_OutOfMemory;
		}
	}
	return Error_None;
}


elError elf_deftlsallocfn(Alloc *allocator, int flags, elInteger oldSize, elInteger newSize, void **io, ldebugloc loca) {
	if (io == 0) {
		return Error_InvalidArguments;
	}
	if (newSize == 0) {
		return Error_InvalidArguments;
	} else {
		/* reallocation is not permitted */
		if (*io != 0) {
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

		*io = cursor;
		cursor += newSize;
	}
	return Error_None;
}
