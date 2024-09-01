/*
** See Copyright Notice In elf.h
** array.h
** Dynamic array tool
**
**
** The idea is to allocate a buffer of memory
** along with a header. This header contains
** information about the array.
**
** See stb's implementation.
**
** Arrays are not exposed directly to elf, instead,
** we use them internally as a core data type.
**
** Usage is as follows:
**
** T *array = 0; // 0 is initialized
**
** ARRAY_ADD(array,(T) thing)
**
** ARRAY_ADD and all the other functions take
** l-values strictly...
**
** Needless to say, all of the API functions require
** compatible memory...
**
** [ARRAY-HEADER][PAYLOAD]
**
*/



// - min is the minimum size of the array, or how much
// of it has been used already, in bytes...
// - max is the maximum size of the array, or how much
// has been reserved for if already, must always be
// greater than or equal to min, otherwise the array
// is invalid...
typedef struct ArrayH {
	elf_Int max;
	elf_Int min;
   /* contents are past this point */
} ArrayH;



/* Converts a regular pointer to an array
pointer */
#define ARRAY(D) ((ArrayH*)(D))[-1]


#define ARRAY_MAX(D) (ARRAY(D).max)
#define ARRAY_MIN(D) (ARRAY(D).min)

#define ARRAY_GET_MAX(D) ((D != 0) ? ARRAY_MAX(D) : 0)
#define ARRAY_GET_MIN(D) ((D != 0) ? ARRAY_MIN(D) : 0)

#define ARRAY_SET_MIN(D,N) ((D != 0) ? ARRAY_MIN(D)=(N) : 0)

#define ARRAY_LENGTH ARRAY_GET_MIN


#define ARRAY_DELETE(D) ((D != 0) ? dealloc_memory(GLOBAL_ALLOCATOR,&ARRAY(D)), 0 : 0)


#define FOR_RANGE(N,X,Y) for (elf_Int N = X; N < Y; N += 1)

#define FOR_ARRAY(N,D) FOR_RANGE(N,0,ARRAY_LENGTH(D))


#define ARRAY_POP(D) ((D != 0) ? ARRAY(D).min -= 1 : 0)


#define ARRAY_GROW(D,N) (array_allocate((void**)&(D),sizeof(*D),N,N))



/* this seems to be the more cross compiler solution, emcc fails,
clang fails and gcc fail in other more compact ways...
I'm not sure why, and frankly since it hasn't broken ever since
I can't be bothered... */
#define ARRAY_ADD(D,T) do { elf_Int X = ARRAY_GROW(D,1); D[X] = T; } while(0)



/* returns the newly allocated starting index of the array,
res is how much to reserve which increments (max) if
necessary, and com is how much to commit, which increments (min) */
static elf_Int array_allocate(void **var, elf_Int per, elf_Int res, elf_Int com);
