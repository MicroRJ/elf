//
// See Copyright Notice In elf.h
//
// avoid using wherever possible
//

typedef struct {
	int max;
	int min;
   /* contents are past this point */
} Dynamic_Array;

#define d_array_raw(d) (& ( (Dynamic_Array *) (d) ) [-1] )

#define ARRAY(D) ((Dynamic_Array*)(D))[-1]
#define ARRAY_MAX(D) (ARRAY(D).max)
#define ARRAY_MIN(D) (ARRAY(D).min)
#define ARRAY_GET_MAX(D) ((D != 0) ? ARRAY_MAX(D) : 0)
#define ARRAY_GET_MIN(D) ((D != 0) ? ARRAY_MIN(D) : 0)
#define ARRAY_SET_MIN(D,N) ((D != 0) ? ARRAY_MIN(D)=(N) : 0)
#define darr_l ARRAY_GET_MIN
#define darr_free(D) ((D != 0) ? free(&ARRAY(D)), 0 : 0)
#define darr_grow(D,N) (array_allocate((void**)&(D),sizeof(*D),N,N))
#define FOR_ARRAY(N,D) FOR_RANGE(N,0,darr_l(D))

// written like this because it seems to work on all compilers I've
// tested
#define darr_add(d,v) do { int __x__ = darr_grow(d, 1); (d)[__x__] = (v); } while(0)


/* returns the newly allocated starting index of the array,
res is how much to reserve which increments (max) if
necessary, and com is how much to commit, which increments (min) */

static int array_allocate(void **var, int per, int res, int com) {
	Dynamic_Array *arr = 0;
	int max = 0, min = 0;
	if (*var != 0) {
		arr = &ARRAY(*var);
		max = arr->max;
		min = arr->min;
	}
  // ensure reserve > commit
	if (max + res < com) {
		res += (com - (max + res));
	}
	if (min + res > max) {
		max <<= 1;
		if(min + res > max) {
			max = min + res;
		}
		arr = realloc(arr, sizeof(Dynamic_Array) + per * max);
	}
	if (arr != 0) {
		arr->max = max;
		arr->min = min + com;
	}
	*var = arr + 1;
	return min;
}



