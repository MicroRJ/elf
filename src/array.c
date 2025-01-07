/*
** See Copyright Notice In elf.h
** array.h
** Dynamic array tool
*/


elf_i64 array_allocate(void **var, elf_i64 per, elf_i64 res, elf_i64 com) {
	subarrayT *arr = 0;
	elf_i64 max = 0, min = 0;
	if (*var != 0) {
		arr = &ARRAY(*var);
		max = arr->max;
		min = arr->min;
	}
   /* increment reserve if we attempt to commit
   more than we've got reserved */
	if (max + res < com) {
		res += (com - (max + res));
	}
	if (min + res > max) {
		max <<= 1;
		if(min + res > max) {
			max = min + res;
		}
		arr = realloc_memory(GLOBAL_ALLOCATOR,sizeof(subarrayT)+per*max,arr);
	}
	if (arr != 0) {
		arr->max = max;
		arr->min = min + com;
	}
	*var = arr + 1;
	return min;
}