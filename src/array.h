/*
** See Copyright Notice In elf.h
** array.h
** Dynamic array tool
**
**
** Arrays are not exposed directly to elf, instead,
** we use them internally as a core data type,
** they work the same as STB's stretchy buffer.
**
**
** Usage is as follows:
**
** T *array = 0; // ensure it is initialized to 0
** // otherwise it thinks it has already been initialized...
**
** ARRAY_ADD(array,T-thing)
**
** ARRAY_ADD and all the other functions take
** l-values strictly...
**
** Needless to say, all of the API functions require
** a compatible memory layout, otherwise they will fail...
**
** [ARRAY-HEADER][....]
**
*/


typedef struct elArray {
	elInteger max;
	elInteger min;
   /* contents are past this point */
} elArray;



/* Converts a regular pointer to an array
pointer */
#define ARRAY(D) ((elArray*)(D))[-1]


/* Access the min and max fields of the array,
takes the regular pointer.

- min is the minimum size of the array, or how much
of it has been used already, in bytes...

- max is the maximum size of the array, or how much
has been reserved for if already, must always be
greater than or equal to min, otherwise the array is invalid... */
#define ARRAY_MAX(D) ((D != 0) ? ARRAY(D).max : 0)
#define ARRAY_MIN(D) ((D != 0) ? ARRAY(D).min : 0)

#define ARRAY_LENGTH ARRAY_MIN


/* delete the array */
#define ARRAY_DELETE(D) ((D != 0) ? elf_dealloc(elHEAP_ALLOCATOR,&ARRAY(D)), 0 : 0)


#define FOR_RANGE(N,X,Y) for (elInteger N = X; N < Y; N += 1)

#define FOR_ARRAY(N,D) FOR_RANGE(N,0,ARRAY_LENGTH(D))


#define ARRAY_POP(D) ((D != 0) ? ARRAY(D).min -= 1 : 0)


#define ARRAY_GROW(D,N) (elf_array_allocate((void**)&(D),sizeof(*D),N,N))



/* this seems to be the more cross compiler solution, emcc fails,
clang fails and gcc fail in other more compact ways...
I'm not sure why, and frankly since it hasn't broken ever since
I can't be bothered... */
#define ARRAY_ADD(D,T) do { elInteger X = ARRAY_GROW(D,1); D[X] = T; } while(0)



/*
** Returns the last index of the array
** that can be written to
*/
elInteger elf_array_allocate(void **var, elInteger per, elInteger res, elInteger com);
