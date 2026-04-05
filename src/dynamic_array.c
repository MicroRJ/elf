//
// See Copyright Notice In elf.h
//
// avoid using wherever possible
//



typedef struct {
	int max;
	int min;
   /* contents are past this point */
} HArray;

#define d_array_raw(d) (& ( (HArray *) (d) ) [-1] )




#define ARRAY(D) ((HArray*)(D))[-1]
#define ARRAY_MAX(D) (ARRAY(D).max)
#define ARRAY_MIN(D) (ARRAY(D).min)
#define ARRAY_GET_MAX(D) ((D != 0) ? ARRAY_MAX(D) : 0)
#define ARRAY_GET_MIN(D) ((D != 0) ? ARRAY_MIN(D) : 0)
#define ARRAY_SET_MIN(D,N) ((D != 0) ? ARRAY_MIN(D)=(N) : 0)
#define heap_array_length ARRAY_GET_MIN
#define free_heap_array(D) ((D != 0) ? free(&ARRAY(D)), 0 : 0)
#define dynamic_array_allocate(D,N) (array_allocate((void**)&(D),sizeof(*D),N,N))

#define FOR_ARRAY(N,D) for (int N = 0; N < heap_array_length(D); N += 1)

#define heap_array_add(d,v) do { int __x__ = dynamic_array_allocate(d, 1); (d)[__x__] = (v); } while(0)





// todo: log2 size !
static inline void *new_heap_array2(int per, int max, int min) {
	ASSERT(min <= max);
	// todo: why calloc!
	HArray *arr = calloc(sizeof(*arr), per * max);
	arr->max = max;
	arr->min = min;
	return arr + 1;
}






static inline void *new_heap_array(int per, int min) {
	return new_heap_array2(per, min, min);
}




static inline void *new_heap_array_with_memory(void *memory, int per, int min) {
	HArray *array = malloc(sizeof(HArray) + per * min);
	array->min = min;
	array->max = min;
	memcpy(array + 1, memory, per * min);
	return array + 1;
}



static int array_allocate(void **var, int per, int res, int com) {
	HArray *arr = 0;
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
		arr = realloc(arr, sizeof(HArray) + per * max);
	}
	if (arr != 0) {
		arr->max = max;
		arr->min = min + com;
	}
	*var = arr + 1;
	return min;
}



