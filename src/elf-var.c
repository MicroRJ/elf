/*
** See Copyright Notice In elf.h
** elf-var.c
** variable array tool
*/


elInteger elf_varaddxx(void **var, elInteger per, elInteger res, elInteger com) {
	elInteger max = 0;
	elInteger min = 0;
	elArray *arr = 0;
	if (*var != 0) {
		arr = ((elArray*)(*var)) - 1;
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
		arr = langM_realloc(elHEAP_ALLOCATOR,sizeof(elArray)+per*max,arr);
	}
	if (arr != 0) {
		arr->max = max;
		arr->min = min + com;
	}
	*var = arr + 1;
	return min;
}
