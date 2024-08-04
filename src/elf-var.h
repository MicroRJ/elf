/*
** See Copyright Notice In elf.h
** elf-arr.h
** variable array tool
*/


/*
** The following is an utility, identical to
** stb's stretchy buffer.
**
** Usage is as follows:
**
** T *items = 0;
** ARRAY_ADD(items,(T){});
**
** int i = elf_xarray_growby(items,5);
** items[i..i+5] == (T){}
**
** T *slot = elf_varaddn(items,5);
** slot[i..i+5] == (T){}
**
** * this is only for internal use
**
*/


#define elf_xarray_delete(var) ((var != 0) ? elf_dealloc(elHEAP_ALLOCATOR,(elArray*)(var)-1),0 : 0)


#define elf_varaddx(var,res,com) ((var) + elf_stretchy_buffer_alloc((void**)&(var),sizeof(*var),res,com))
#define elf_xarray_growby(var,num) (elf_stretchy_buffer_alloc((void**)&(var),sizeof(*var),num,num))
#define elf_varaddn(var,num) ((var) + elf_xarray_growby(var,num))

/* Seems that only msvc compiles this properly or
am I trippin' ? */
#if 0
#define ARRAY_ADD(var,t) ((void)(elf_varaddn(var,1)[0] = t))
#else
#define ARRAY_ADD(var,val) do {\
	elInteger ___i___ = elf_xarray_growby(var,1);\
	var[___i___] = val;\
} while(0)
#endif



/*
** Returns the last index of the array
** that can be written to
*/
elInteger elf_stretchy_buffer_alloc(void **var
, elInteger per, elInteger res, elInteger com);

#define FOR_RANGE(N,RMIN,RMAX) for (elInteger N = RMIN; N < RMAX; N += 1)

#define ARRAY_FOR(N,A) for (elInteger N = 0; N < ARRAY_LENGTH(A); N += 1)
#define ARRAY_PER(T,N,A) for (T N = A; N < A + ARRAY_LENGTH(A); N += 1)

/* todo: these are deprecated */
#define elf_varforj(A) for (elInteger j = 0; j < ARRAY_LENGTH(A); ++ j)
#define elf_xarray_foreachi(A) for (elInteger i = 0; i < ARRAY_LENGTH(A); ++ i)
