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
** elf_xarray_add(items,(T){});
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

typedef struct elArray {
	elObject obj;
	elInteger max;
	elInteger min;
   /* contents are allocated past this point */
} elArray;


#define elf_vararr(var) ((elArray*)(var))[-1]
#define elf_varfor(T,N,A) for (T N = A; N < A + elf_xarray_length(A); N += 1)
#define elf_varforj(A) for (elInteger j = 0; j < elf_xarray_length(A); ++ j)
#define elf_xarray_foreachi(A) for (elInteger i = 0; i < elf_xarray_length(A); ++ i)
#define elf_xarray_delete(var) ((var != 0) ? elf_dealloc(lHEAP,(elArray*)(var)-1),0 : 0)
#define elf_varmax(var) ((var != 0) ? ((elArray*)(var))[-1].max : 0)
#define elf_varmin(var) ((var != 0) ? ((elArray*)(var))[-1].min : 0)

#define elf_vardec(var) ( (var) != elNil ? (-- ((elArray*)(var))[-1].min) : 0 )

#define elf_varaddx(var,res,com) ((var) + elf_varaddxx((void**)&(var),sizeof(*var),res,com))
#define elf_xarray_growby(var,num) (elf_varaddxx((void**)&(var),sizeof(*var),num,num))
#define elf_varaddn(var,num) ((var) + elf_xarray_growby(var,num))
#define elf_xarray_length elf_varmin

/* Seems that only msvc compiles this properly or
am I trippin' ? */
#if 0
#define elf_xarray_add(var,t) ((void)(elf_varaddn(var,1)[0] = t))
#else
#define elf_xarray_add(var,val) do {\
	elInteger ___i___ = elf_xarray_growby(var,1);\
	var[___i___] = val;\
} while(0)
#endif



/*
** Returns the last index of the array
** that can be written to
*/
elInteger elf_varaddxx(void **var
, elInteger per, elInteger res, elInteger com);
